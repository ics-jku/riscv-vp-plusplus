#pragma once
// This file contains cheri type definitions
#include <cstdint>

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

constexpr uint8_t cArchPermCShift = 0;
constexpr uint8_t cArchPermWShift = 1;
constexpr uint8_t cArchPermRShift = 2;
constexpr uint8_t cArchPermXShift = 3;
constexpr uint8_t cArchPermASRShift = 4;
constexpr uint8_t cArchPermLMShift = 5;
constexpr uint8_t cArchPermLGShift = 6;
constexpr uint8_t cArchPermSLShift = 7;

struct ArchPerms {
	uint8_t perms{};

	static constexpr uint8_t CMask = 1u << cArchPermCShift;
	static constexpr uint8_t WMask = 1u << cArchPermWShift;
	static constexpr uint8_t RMask = 1u << cArchPermRShift;
	static constexpr uint8_t XMask = 1u << cArchPermXShift;
	static constexpr uint8_t ASRMask = 1u << cArchPermASRShift;
	static constexpr uint8_t LMMask = 1u << cArchPermLMShift;
	static constexpr uint8_t LGMask = 1u << cArchPermLGShift;
	static constexpr uint8_t SLMask = 1u << cArchPermSLShift;

	constexpr bool operator==(const ArchPerms& other) const {
		return perms == other.perms;
	}

	constexpr ArchPerms() = default;

	// Ctor from uint8_t
	constexpr ArchPerms(uint8_t perms) : perms(perms) {}

	constexpr bool C() const {
		return perms & CMask;
	}

	constexpr bool W() const {
		return perms & WMask;
	}

	constexpr bool R() const {
		return perms & RMask;
	}

	constexpr bool X() const {
		return perms & XMask;
	}

	constexpr bool ASR() const {
		return perms & ASRMask;
	}

	constexpr bool LM() const {
		return perms & LMMask;
	}

	constexpr bool LG() const {
		return perms & LGMask;
	}

	constexpr bool SL() const {
		return perms & SLMask;
	}

	// Convert ArchPerms to uint8_t
	constexpr uint8_t toUInt8() const {
		return perms;
	}
};

static_assert(sizeof(ArchPerms) == sizeof(uint8_t), "ArchPerms must stay byte-sized");

constexpr ArchPerms cArchNullPerms = ArchPerms(0);
