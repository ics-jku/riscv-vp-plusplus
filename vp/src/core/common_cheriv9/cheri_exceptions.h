#pragma once
#include "cheri_constants.h"
#include "cheri_types.h"
#include "core/common/trap.h"

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
#ifdef HANDLE_CHERI_EXCEPTIONS
	printf("CHERI Exception: %d\n", exc);
	raise_trap(exc, mtval, trace);
#endif
}

inline void handle_cheri_cap_exception(CapEx capEx, uint64_t regnum, rvfi_dii_trace_t* trace) {
#ifdef HANDLE_CHERI_EXCEPTIONS  // Defined via CMake for CHERI targets
	printf("CHERI Exception: %s\n", cap_ex_to_string(capEx).c_str());
	raise_trap(static_cast<ExceptionCode>(EXC_CHERI_FAULT), (regnum << cExcCapIdxShift) + capEx, trace);
#endif
}

inline void handle_cheri_reg_exception(CapEx capEx, uint64_t regidx, rvfi_dii_trace_t* trace) {
	handle_cheri_cap_exception(capEx, regidx, trace);
}

inline void handle_mem_exception(uint64_t addr, ExcType ex, rvfi_dii_trace_t* trace) {
	raise_trap(static_cast<ExceptionCode>(ex), addr, trace);
}

inline void handle_cheri_pcc_exception(CapEx e, rvfi_dii_trace_t* trace) {
	handle_cheri_cap_exception(e, cPccIdx, trace);
}
