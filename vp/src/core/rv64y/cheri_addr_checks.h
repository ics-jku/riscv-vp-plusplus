#pragma once

#include "core/common/trap.h"
#include "core/common_y/cheri_cap_common.h"
#include "core/common_y/cheri_exceptions.h"

inline void cheriFetchCheckPc(ProgramCounterCapability start_pc, ProgramCounterCapability pc, rvfi_dii_trace_t *trace,
                              bool has_compressed) {
	if (start_pc == pc) {
		// Do full checks
		if (!pc->cap.fields.tag) {
			handle_cheri_pcc_exception(CapEx_TagViolation, start_pc->cap.fields.address, trace);
		}
		if (pc->isSealed()) {
			handle_cheri_pcc_exception(CapEx_SealViolation, start_pc->cap.fields.address, trace);
		}
		if (!pc->permitExecute()) {
			handle_cheri_pcc_exception(CapEx_PermitExecuteViolation, start_pc->cap.fields.address, trace);
		}
		if (!start_pc.pcc.inCapBounds(pc->cap.fields.address, 2)) {
			handle_cheri_pcc_exception(CapEx_LengthViolation, start_pc->cap.fields.address, trace);
		}
		return;
	}
	// Perform only the bounds checks (remaining checks were already done for the LSB of the instruction, where start_pc
	// == pc)
	if (!start_pc.pcc.inCapBounds(pc->cap.fields.address, 2)) {
		handle_cheri_pcc_exception(CapEx_LengthViolation, start_pc->cap.fields.address, trace);
	}
}

// Used by RISCV-JALR
inline void cheriControlCheckAddr(uint64_t target, Capability pcc, rvfi_dii_trace_t *trace,
                                  uint8_t min_instruction_bytes) {
	// Clear bit 0 of target
	target &= ~0b1;
	if (!pcc.inCapBounds(target, min_instruction_bytes)) {
		handle_cheri_pcc_exception(CapEx_LengthViolation, target, trace);
	}
}
