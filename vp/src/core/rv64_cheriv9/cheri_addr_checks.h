#pragma once

#include "core/common/trap.h"
#include "core/common_cheriv9/cheri_cap_common.h"
#include "core/common_cheriv9/cheri_exceptions.h"

inline void cheriFetchCheckPc(ProgramCounterCapability start_pc, ProgramCounterCapability pc, rvfi_dii_trace_t *trace,
                              bool has_compressed) {
	if (start_pc == pc) {
		// Do full checks
		if (!pc->fields.tag) {
			handle_cheri_pcc_exception(CapEx_TagViolation, trace);
		}
		if (pc->isSealed()) {
			handle_cheri_pcc_exception(CapEx_SealViolation, trace);
		}
		if (!pc->fields.permit_execute) {
			handle_cheri_pcc_exception(CapEx_PermitExecuteViolation, trace);
		}
		// If PCC relocation is enabled, require that PCC.base be as aligned as PC
		// This is also enforced when setting PCC in most places
		if (have_pcc_relocation() &&
		    (((pc.pcc.getBase() & 0b1) != 0) | (((pc.pcc.getBase() & 0b10) != 0) && !has_compressed))) {
			handle_cheri_pcc_exception(CapEx_UnalignedBase, trace);
		}
		if (!start_pc.pcc.inCapBounds(pc->fields.address, 2)) {
			handle_cheri_pcc_exception(CapEx_LengthViolation, trace);
		}
		return;
	}
	// Perform only the bounds checks (remaining checks were already done for the LSB of the instruction, where start_pc
	// == pc)
	if (!start_pc.pcc.inCapBounds(pc->fields.address, 2)) {
		handle_cheri_pcc_exception(CapEx_LengthViolation, trace);
	}
}

// Used by RISCV-JALR
inline void cheriControlCheckAddr(uint64_t target, Capability pcc, rvfi_dii_trace_t *trace,
                                  uint8_t min_instruction_bytes) {
	if (have_pcc_relocation()) {
		target += pcc.getBase();
	}
	// Clear bit 0 of target
	target &= ~0b1;
	if (!pcc.inCapBounds(target, min_instruction_bytes)) {
		handle_cheri_pcc_exception(CapEx_LengthViolation, trace);
	}
}
