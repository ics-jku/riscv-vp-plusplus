#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <systemc>
#include <unordered_set>
#include <vector>

#include "core/common/bus_lock_if.h"
#include "core/common/clint_if.h"
#include "core/common/debug.h"
#include "core/common/instr.h"
#include "core/common/irq_if.h"
#include "core/common/lscache.h"
#include "core/common/mem_if.h"
#include "core/common/regfile.h"
#include "core/common/trap.h"
#include "csr.h"
#include "fp.h"
#include "platform/gd32/nuclei_core/nuclei_csr.h"
#include "syscall_if.h"
#include "util/common.h"
#include "util/initiator_if.h"
#include "v.h"

namespace rv32 {

static constexpr unsigned XLEN = 32;
using sxlen_t = int32_t;
using uxlen_t = uint32_t;
using xlen_t = sxlen_t;
static constexpr sxlen_t REG32_MIN = INT32_MIN;
static constexpr sxlen_t REG_MIN = REG32_MIN;
using RegFile = RegFile_T<sxlen_t, uxlen_t>;
using data_memory_if = data_memory_if_T<sxlen_t, uxlen_t>;

// NOTE: on this branch, currently the *simple-timing* model is still directly
// integrated in the ISS. Merge the *timedb* branch to use the timing_if.

// predefine default template instance ISS
template <class T_csr_table>
struct ISS_T;
using ISS = ISS_T<csr_table>;

struct timing_if {
	virtual ~timing_if() {}

	virtual void update_timing(Instruction instr, Opcode::Mapping op, ISS &iss) = 0;
};

struct PendingInterrupts {
	PrivilegeLevel target_mode;
	uxlen_t pending;
};

/*
 * NOTE RVxx.1:
 *
 * See also "NOTE RVxx.1" in core/rv32/iss.cpp, core/rv64/iss.h and core/rv64/iss.cpp
 *
 * This RV32 ISS_T template class is used as stand-alone for the classic RV32 ISS and as base class
 * for the derived nuclei_core ISS (platform/gd32/nuclei_core).
 * Currently the implementation is based on C++ templates and polymorphism, so:
 *  1. The ISS_T class is *NOT* set to final to allow derived classes
 *  2. There are virtual methods. The virtual methods to make nuclei_core work are annotated with a
 *     "see NOTE RVXX.1" comment
 *  3. ISS_T is a template with an implementation located in iss.cpp, so every concrete class based
 *     on the ISS_T template has to be *explicitly* specified. The concrete implementations of the
 *     ISS_T template for the classic RV32 ISS and nuclei_core ISS are specified at the end of iss.cpp
 *     annotated with a "see NOTE RVXX.1" comment.
 *
 * TODO: Virtual methods come with a runtime-cost (dynamic dispatch) -> In the long run, we plan to get rid of
 * polymorphism here all together by using a C++ template based approach. (something as described in
 * http://groups.di.unipi.it/~nids/docs/templates_vs_inheritance.html)
 */
template <class T_csr_table>
struct ISS_T : public external_interrupt_target,
               public clint_interrupt_target,
               public iss_syscall_if,
               public debug_target_if,
               public initiator_if {
	clint_if *clint = nullptr;
	instr_memory_if *instr_mem = nullptr;
	LSCacheDefault_T<sxlen_t, uxlen_t> lscache;
	data_memory_if *mem = nullptr;
	syscall_emulator_if *sys = nullptr;  // optional, if provided, the iss will intercept and handle syscalls directly
	RegFile regs;
	FpRegs fp_regs;
	uxlen_t pc = 0;
	uxlen_t last_pc = 0;
	bool trace = false;
	bool shall_exit = false;
	bool ignore_wfi = false;
	bool error_on_zero_traphandler = false;
	T_csr_table csrs;
	VExtension<ISS_T<T_csr_table>> v_ext;
	PrivilegeLevel prv = MachineMode;
	int64_t lr_sc_counter = 0;
	uint64_t total_num_instr = 0;

	// last decoded and executed instruction and opcode
	Instruction instr;
	Opcode::Mapping op;

	CoreExecStatus status = CoreExecStatus::Runnable;
	std::unordered_set<uxlen_t> breakpoints;
	bool debug_mode = false;

	sc_core::sc_event wfi_event;

	std::string systemc_name;
	tlm_utils::tlm_quantumkeeper quantum_keeper;
	sc_core::sc_time cycle_time;
	sc_core::sc_time cycle_counter;  // use a separate cycle counter, since cycle count can be inhibited
	std::array<sc_core::sc_time, Opcode::NUMBER_OF_INSTRUCTIONS> instr_cycles;

	static constexpr unsigned xlen = XLEN;

	ISS_T(uxlen_t hart_id, bool use_E_base_isa = false);

	Architecture get_architecture(void) override {
		return RV32;
	}

	std::string name();
	void halt();

	void exec_step();

	uint64_t _compute_and_get_current_cycles();

	void init(instr_memory_if *instr_mem, data_memory_if *data_mem, clint_if *clint, uxlen_t entrypoint, uxlen_t sp);

	void trigger_external_interrupt(PrivilegeLevel level) override;
	void clear_external_interrupt(PrivilegeLevel level) override;

	void trigger_timer_interrupt() override;
	void clear_timer_interrupt() override;

	void trigger_software_interrupt() override;
	void clear_software_interrupt() override;

	void sys_exit() override;
	unsigned get_syscall_register_index() override;
	uint64_t read_register(unsigned idx) override;
	void write_register(unsigned idx, uint64_t value) override;

	std::vector<uint64_t> get_registers(void) override;

	uint64_t get_progam_counter(void) override;
	void enable_debug(void) override;
	CoreExecStatus get_status(void) override;
	void set_status(CoreExecStatus) override;
	void block_on_wfi(bool) override;

	void insert_breakpoint(uint64_t) override;
	void remove_breakpoint(uint64_t) override;

	uint64_t get_hart_id() override;

	void release_lr_sc_reservation() {
		lr_sc_counter = 0;
		mem->atomic_unlock();
	}

	void fp_prepare_instr();
	void fp_finish_instr();
	void fp_set_dirty();
	void fp_update_exception_flags();
	void fp_setup_rm();
	void fp_require_not_off();

	/* virtual -> see NOTE RVxx.1 above */
	virtual uxlen_t get_csr_value(uxlen_t addr);
	/* virtual -> see NOTE RVxx.1 above */
	virtual void set_csr_value(uxlen_t addr, uxlen_t value);

	bool is_invalid_csr_access(uxlen_t csr_addr, bool is_write);
	void validate_csr_counter_read_access_rights(uxlen_t addr);

	uxlen_t pc_alignment_mask() {
		if (csrs.misa.has_C_extension()) {
			return ~uxlen_t(0x1);
		} else {
			return ~uxlen_t(0x3);
		}
	}

	inline void trap_check_pc_alignment() {
		assert(!(pc & 0x1) && "not possible due to immediate formats and jump execution");

		if (unlikely((pc & 0x3) && (!csrs.misa.has_C_extension()))) {
			// NOTE: misaligned instruction address not possible on machines supporting compressed instructions
			raise_trap(EXC_INSTR_ADDR_MISALIGNED, pc);
		}
	}

	template <unsigned Alignment, bool isLoad>
	inline void trap_check_addr_alignment(uxlen_t addr) {
		if (unlikely(addr % Alignment)) {
			raise_trap(isLoad ? EXC_LOAD_ADDR_MISALIGNED : EXC_STORE_AMO_ADDR_MISALIGNED, addr);
		}
	}

	inline void execute_amo_w(Instruction &instr, std::function<int32_t(int32_t, int32_t)> operation) {
		uxlen_t addr = regs[instr.rs1()];
		trap_check_addr_alignment<4, false>(addr);
		int32_t data;
		try {
			data = mem->atomic_load_word(addr);
		} catch (SimulationTrap &e) {
			if (e.reason == EXC_LOAD_ACCESS_FAULT)
				e.reason = EXC_STORE_AMO_ACCESS_FAULT;
			throw e;
		}
		int32_t val = operation(data, (int32_t)regs[instr.rs2()]);
		mem->atomic_store_word(addr, val);
		// ignore write to zero/x0
		if (instr.rd() != RegFile::zero) {
			regs[instr.rd()] = data;
		}
	}

	inline bool m_mode() {
		return prv == MachineMode;
	}

	inline bool s_mode() {
		return prv == SupervisorMode;
	}

	inline bool u_mode() {
		return prv == UserMode;
	}

	PrivilegeLevel prepare_trap(SimulationTrap &e);

	void prepare_interrupt(const PendingInterrupts &x);

	PendingInterrupts compute_pending_interrupts();

	bool has_pending_enabled_interrupts() {
		return compute_pending_interrupts().target_mode != NoneMode;
	}

	/* virtual -> see NOTE RVxx.1 above */
	virtual bool has_local_pending_enabled_interrupts() {
		return csrs.mie.reg & csrs.mip.reg;
	}

	/* virtual -> see NOTE RVxx.1 above */
	virtual void return_from_trap_handler(PrivilegeLevel return_mode);

	void switch_to_trap_handler(PrivilegeLevel target_mode);

	void performance_and_sync_update(Opcode::Mapping executed_op);

	/* virtual -> see NOTE RVxx.1 above */
	virtual void handle_interrupt();

	/* virtual -> see NOTE RVxx.1 above */
	virtual void handle_trap(SimulationTrap &e);

	void run_step() override;

	void run() override;

	void show();
};

/* Do not call the run function of the ISS directly but use one of the Runner
 * wrappers. */
template <class T_csr_table>
struct DirectCoreRunner : public sc_core::sc_module {
	ISS_T<T_csr_table> &core;
	std::string thread_name;

	SC_HAS_PROCESS(DirectCoreRunner);

	DirectCoreRunner(ISS_T<T_csr_table> &core)
	    : sc_module(sc_core::sc_module_name(core.systemc_name.c_str())), core(core) {
		thread_name = "run" + std::to_string(core.get_hart_id());
		SC_NAMED_THREAD(run, thread_name.c_str());
	}

	void run() {
		core.run();

		if (core.status == CoreExecStatus::HitBreakpoint) {
			throw std::runtime_error(
			    "Breakpoints are not supported in the direct runner, use the debug "
			    "runner instead.");
		}
		assert(core.status == CoreExecStatus::Terminated);

		sc_core::sc_stop();
	}
};

}  // namespace rv32
