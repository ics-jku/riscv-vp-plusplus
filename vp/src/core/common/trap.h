#pragma once

// for rv64_cheriv9 rvfi_dii
#include "../rv64_cheriv9/rvfi-dii/rvfi_dii_trace.h"  // TODO: Should not include rv64 in common!

enum ExceptionCode {
	// interrupt exception codes (mcause)
	EXC_U_SOFTWARE_INTERRUPT = 0,
	EXC_S_SOFTWARE_INTERRUPT = 1,
	EXC_M_SOFTWARE_INTERRUPT = 3,

	EXC_U_TIMER_INTERRUPT = 4,
	EXC_S_TIMER_INTERRUPT = 5,
	EXC_M_TIMER_INTERRUPT = 7,

	EXC_U_EXTERNAL_INTERRUPT = 8,
	EXC_S_EXTERNAL_INTERRUPT = 9,
	EXC_M_EXTERNAL_INTERRUPT = 11,

	// non-interrupt exception codes (mcause)
	EXC_INSTR_ADDR_MISALIGNED = 0,
	EXC_INSTR_ACCESS_FAULT = 1,
	EXC_ILLEGAL_INSTR = 2,
	EXC_BREAKPOINT = 3,
	EXC_LOAD_ADDR_MISALIGNED = 4,
	EXC_LOAD_ACCESS_FAULT = 5,
	EXC_STORE_AMO_ADDR_MISALIGNED = 6,
	EXC_STORE_AMO_ACCESS_FAULT = 7,

	EXC_ECALL_U_MODE = 8,
	EXC_ECALL_S_MODE = 9,
	EXC_ECALL_M_MODE = 11,

	EXC_INSTR_PAGE_FAULT = 12,
	EXC_LOAD_PAGE_FAULT = 13,
	EXC_STORE_AMO_PAGE_FAULT = 15,

	// CHERIv9 exception codes
	EXC_CHERI_LOAD_FAULT = 26,
	EXC_CHERI_STORE_FAULT = 27,
	EXC_CHERI_FAULT = 28,
};

struct SimulationTrap {
	ExceptionCode reason;
	uint64_t mtval;
};

inline void raise_trap(ExceptionCode exc, uint64_t mtval) {
	throw SimulationTrap({exc, mtval});
}

inline void raise_trap(ExceptionCode exc, unsigned long mtval, rvfi_dii_trace_t* trace) {
	if (trace != nullptr) {
		// trace->rvfi_dii_pc_wdata = mtval;
		trace->rvfi_dii_trap = 1;
		// trace->rvfi_dii_rs1_addr = 0;
		// trace->rvfi_dii_rs2_addr = 0;
		// trace->rvfi_dii_mem_addr = 0;
	}
	raise_trap(exc, mtval);
}
