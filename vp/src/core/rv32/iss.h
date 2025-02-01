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
#include "core/common/dbbcache.h"
#include "core/common/debug.h"
#include "core/common/instr.h"
#include "core/common/irq_if.h"
#include "core/common/iss_stats.h"
#include "core/common/lscache.h"
#include "core/common/mem_if.h"
#include "core/common/regfile.h"
#include "core/common/syscall_if.h"
#include "core/common/trap.h"
#include "csr.h"
#include "fp.h"
#include "platform/gd32/nuclei_core/nuclei_csr.h"
#include "util/common.h"
#include "util/initiator_if.h"
#include "v.h"

namespace rv32 {

static constexpr Architecture ARCH = RV32;
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

template <class T_ISS>
struct timing_if {
	virtual ~timing_if() {}

	virtual void update_timing(Instruction instr, Opcode::Mapping op, T_ISS &iss) = 0;
};

struct PendingInterrupts {
	PrivilegeLevel target_mode;
	uxlen_t pending;
};

/*
 * NOTE RVxx.2: C-style macros
 * Create concrete ISS type definitions from iss_ctemplate.h using iss_ctemplate_handle.h
 * see NOTE RVxx.2 in iss_ctemplate_handle.h for more details
 */
#define ISS_CT_CREATE_DEFINITION
#include "iss_ctemplate_handle.h"

/* Do not call the run function of the ISS directly but use one of the Runner
 * wrappers. */
template <class T_ISS>
struct DirectCoreRunner : public sc_core::sc_module {
	T_ISS &core;
	std::string thread_name;

	SC_HAS_PROCESS(DirectCoreRunner);

	DirectCoreRunner(T_ISS &core) : sc_module(sc_core::sc_module_name(core.systemc_name.c_str())), core(core) {
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
