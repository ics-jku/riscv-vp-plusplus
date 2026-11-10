#pragma once
#include "cheri_constants.h"
#include "cheri_types.h"
#include "core/common/trap.h"
#include "util/cheri_debug_tools.h"

using rv64y::CheriDebugTool;

constexpr uint8_t cExcCapIdxShift =
    5;  // CHERI defines xtval register format as: [31:11] = WPRI, [10:5] = Cap Idx, [4:0] = cause

inline std::string cap_ex_to_string(CapEx capEx) {
	switch (capEx) {
		case CapEx_None:
			return "None";
		case CapEx_LengthViolation:
			return "LengthViolation";
		case CapEx_TagViolation:
			return "TagViolation";
		case CapEx_SealViolation:
			return "SealViolation";
		case CapEx_TypeViolation:
			return "TypeViolation";
		case CapEx_UserDefViolation:
			return "UserDefViolation";
		case CapEx_UnalignedBase:
			return "UnalignedBase";
		case CapEx_GlobalViolation:
			return "GlobalViolation";
		case CapEx_PermitExecuteViolation:
			return "PermitExecuteViolation";
		case CapEx_PermitLoadViolation:
			return "PermitLoadViolation";
		case CapEx_PermitStoreViolation:
			return "PermitStoreViolation";
		case CapEx_PermitLoadCapViolation:
			return "PermitLoadCapViolation";
		case CapEx_PermitStoreCapViolation:
			return "PermitStoreCapViolation";
		case CapEx_PermitStoreLocalCapViolation:
			return "PermitStoreLocalCapViolation";
		case CapEx_AccessSystemRegsViolation:
			return "AccessSystemRegsViolation";
		case CapEx_PermitCInvokeViolation:
			return "PermitCInvokeViolation";
		case CapEx_PermitSetCIDViolation:
			return "PermitSetCIDViolation";
		default:
			return "Unknown";
	}
}

inline void handle_cheri_exception(ExceptionCode exc, uint64_t mtval, rvfi_dii_trace_t* trace) {
	if (CheriDebugTool::instance().IsCapExceptionHandlingEnabled()) {
		// printf("[VP_ISS] CHERI Exception: %d\n", exc);
		raise_trap(exc, mtval, trace);
	}
}

inline void handle_cheri_cap_exception(CapEx capEx, uint64_t regnum, uint64_t addr, rvfi_dii_trace_t* trace) {
	if (CheriDebugTool::instance().IsCapExceptionHandlingEnabled()) {
		// printf("[VP_ISS] CHERI Exception: %s\n", cap_ex_to_string(capEx).c_str());
		raise_trap(static_cast<ExceptionCode>(EXC_CHERI_INSTR), addr, trace);
	}
}

inline void handle_cheri_ls_exception(uint64_t addr, bool is_load, rvfi_dii_trace_t* trace) {
	if (is_load) {
		handle_cheri_exception(static_cast<ExceptionCode>(EXC_CHERI_LOAD), addr, trace);
	} else {
		handle_cheri_exception(static_cast<ExceptionCode>(EXC_CHERI_STORE), addr, trace);
	}
}

inline void handle_cheri_reg_exception(CapEx capEx, uint64_t regidx, uint64_t addr, rvfi_dii_trace_t* trace) {
	handle_cheri_cap_exception(capEx, regidx, addr, trace);
}

inline void handle_mem_exception(uint64_t addr, ExcType ex, rvfi_dii_trace_t* trace) {
	raise_trap(static_cast<ExceptionCode>(ex), addr, trace);
}

inline void handle_cheri_pcc_exception(CapEx e, uint64_t addr, rvfi_dii_trace_t* trace) {
	handle_cheri_cap_exception(e, cPccIdx, addr, trace);
}
