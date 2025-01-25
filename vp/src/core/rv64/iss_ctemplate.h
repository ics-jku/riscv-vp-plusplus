/*
 * NEVER INCLUDE THIS FILE DIRECTLY!!!
 *
 * NOTE RVxx.2: C-style macros
 * concrete types are derived in iss.h (by iss_ctemplate_handle.h)
 * see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h for more details
 */

/* see NOTE RVxx.1 */
#ifndef ISS_CT_ENABLE_POLYMORPHISM
#define PROP_CLASS_FINAL final
#define PROP_METHOD_VIRTUAL
#else
#define PROP_CLASS_FINAL
#define PROP_METHOD_VIRTUAL virtual
#endif

/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
struct ISS_CT PROP_CLASS_FINAL : public external_interrupt_target,
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
	ISS_CT_T_CSR_TABLE csrs;
	VExtension<ISS_CT> v_ext;
	PrivilegeLevel prv = MachineMode;
	int64_t lr_sc_counter = 0;

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

	ISS_CT(uxlen_t hart_id);

	Architecture get_architecture(void) override {
		return RV64;
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

	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL uxlen_t get_csr_value(uxlen_t addr);
	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL void set_csr_value(uxlen_t addr, uxlen_t value);

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

	inline void execute_amo_d(Instruction &instr, std::function<int64_t(int64_t, int64_t)> operation) {
		uxlen_t addr = regs[instr.rs1()];
		trap_check_addr_alignment<8, false>(addr);
		uint64_t data;
		try {
			data = mem->atomic_load_double(addr);
		} catch (SimulationTrap &e) {
			if (e.reason == EXC_LOAD_ACCESS_FAULT)
				e.reason = EXC_STORE_AMO_ACCESS_FAULT;
			throw e;
		}
		uint64_t val = operation(data, regs[instr.rs2()]);
		mem->atomic_store_double(addr, val);
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

	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL bool has_local_pending_enabled_interrupts() {
		return csrs.mie.reg & csrs.mip.reg;
	}

	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL void return_from_trap_handler(PrivilegeLevel return_mode);

	void switch_to_trap_handler(PrivilegeLevel target_mode);

	void performance_and_sync_update(Opcode::Mapping executed_op);

	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL void handle_interrupt();

	/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
	PROP_METHOD_VIRTUAL void handle_trap(SimulationTrap &e);

	void run_step() override;

	void run() override;

	void show();
};

/* see NOTE RVxx.1 and NOTE RVxx.2 in iss_ctemplate_handle.h */
#undef PROP_CLASS_FINAL
#undef PROP_METHOD_VIRTUAL
