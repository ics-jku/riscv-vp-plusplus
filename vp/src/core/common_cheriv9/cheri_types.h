#pragma once
// This file contains cheri type definitions
enum CapEx {
	CapEx_None = 0,
	CapEx_LengthViolation = 1,
	CapEx_TagViolation = 2,
	CapEx_SealViolation = 3,
	CapEx_TypeViolation = 4,
	CapEx_UserDefViolation = 5,
	CapEx_UnalignedBase = 6,
	// 0x7 reserved
	CapEx_SoftwareDefinedPermViolation = 8,
	// 0x9 - 0xf reserved
	CapEx_GlobalViolation = 0x10,
	CapEx_PermitExecuteViolation = 0x11,
	CapEx_PermitLoadViolation = 0x12,
	CapEx_PermitStoreViolation = 0x13,
	CapEx_PermitLoadCapViolation = 0x14,
	CapEx_PermitStoreCapViolation = 0x15,
	CapEx_PermitStoreLocalCapViolation = 0x16,
	// 0x17 reserved
	CapEx_AccessSystemRegsViolation = 0x18,
	CapEx_PermitCInvokeViolation = 0x19,
	// 0x1a - 0x1b reserved
	CapEx_PermitSetCIDViolation = 0x1C
};

enum ExcType {
	E_FetchAddrAlign = 0,
	E_LoadAddrAlign = 4,
};
