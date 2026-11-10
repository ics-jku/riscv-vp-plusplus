#pragma once
#include <bit>
#include <bitset>
#include <cstdint>

#include "core/common_y/cheri_constants.h"
#include "core/common_y/cheri_types.h"
#include "util/common.h"

#define CHERI_MAX(n) \
	((n) < (sizeof(uint64_t) * 8) ? ((1ULL << (n)) - 1) : 0xFFFFFFFFFFFFFFFFULL)  // Catch edge case of MAX(64)
#define CHERI_BIT_RANGE(instr, upper, lower) (instr & (((1ULL << (upper - lower + 1ULL)) - 1ULL) << lower))
#define CHERI_BIT_SLICE(instr, upper, lower) (CHERI_BIT_RANGE(instr, upper, lower) >> lower)
#define CHERI_BIT_SINGLE(instr, pos) (instr & (1ULL << pos))
#define CHERI_BIT_SINGLE_P1(instr, pos) (CHERI_BIT_SINGLE(instr, pos) >> pos)

// Capability architectural sizes

constexpr uint8_t cCapSize = 16;  // Size of a capability in bytes, excluding tag
constexpr uint8_t cXlen = 64;
constexpr uint8_t cCapAddrWidth = cXlen;
constexpr uint8_t cCapLenWidth = cCapAddrWidth + 1;

constexpr uint8_t cCapMantissaWidth = 14;

constexpr uint8_t cCapMaxE = cCapLenWidth - cCapMantissaWidth + 1;

constexpr uint8_t cCapEWidth = bitWidth(cCapMaxE);

constexpr uint8_t cInternalETakeBits = 3;

constexpr uint64_t cCapMaxAddr = CHERI_MAX(cCapAddrWidth);
constexpr __uint128_t cCapLenMax = (static_cast<__uint128_t>(1ULL) << cCapLenWidth) - 1;
constexpr uint8_t cCapResetE = cCapMaxE;
constexpr uint16_t cCapResetT = 0b01 << (cCapMantissaWidth - 2);

constexpr uint8_t cCapReserved1Width = 7;
constexpr uint8_t cSDPermsWidth = 4;
constexpr uint8_t cAPWidth = 8;
constexpr uint8_t cCapReserved0Width = 15;

using CapBase_t = uint64_t;
using CapTop_t = __uint128_t;
using CapAddr_t = uint64_t;
using CapLen_t = __uint128_t;

// Forward declaration for conversion functions
struct Capability;

constexpr uint8_t cEncCapBShift = cXlen;
constexpr uint8_t cEncCapTShift = cEncCapBShift + cCapMantissaWidth;
constexpr uint8_t cEncCapEFShift = cEncCapTShift + (cCapMantissaWidth - 2);
constexpr uint8_t cEncCapCTShift = cEncCapEFShift + 1;
constexpr uint8_t cEncCapReserved0Shift = cEncCapCTShift + 1;
constexpr uint8_t cEncCapGLShift = cEncCapReserved0Shift + cCapReserved0Width;
constexpr uint8_t cEncCapPShift = cEncCapGLShift + 1;
constexpr uint8_t cEncCapAPShift = cEncCapPShift + 1;
constexpr uint8_t cEncCapReserved1Shift = cEncCapAPShift + cAPWidth;
constexpr uint8_t cEncCapSDPermsShift = cEncCapReserved1Shift + cCapReserved1Width;

struct EncCapability {
   private:
	__uint128_t raw = 0;

   public:
	constexpr EncCapability() = default;
	constexpr explicit EncCapability(__uint128_t data) : raw(data) {}

	constexpr __uint128_t toUint128() const {
		return raw;
	}

	constexpr operator __uint128_t() const {
		return raw;
	}

	static Capability toCapability(bool tag, const EncCapability& c);
};

// Assert size of EncCapability is 128 bits
static_assert(sizeof(EncCapability) * 8 == 128, "EncCapability must be 128 bits in size!");

__uint128_t encCapToUint128(const EncCapability& cap);
EncCapability uint128ToEncCapability(__uint128_t data);

// Macros for Perm validation in fixMalformedPerms
#define PERM_SHIFT(name) cCapPermBitField##name##Shift

#define HAS_PERM(name) ((perms & (UINT64_C(1) << PERM_SHIFT(name))) != 0)

#define CLEAR_PERM_IF(name, condition) perms &= ~((uint64_t)((condition)) << PERM_SHIFT(name))

constexpr uint8_t cCapPermBitFieldSDPWidth = cSDPermsWidth;
constexpr uint8_t cCapPermBitFieldReserved1_0Width = 10 - cCapPermBitFieldSDPWidth;
constexpr uint8_t cCapPermBitFieldReserved1_1Width = 5;
constexpr uint8_t cCapPermBitFieldReserved0Width = cXlen - 24;

constexpr uint8_t cCapPermBitFieldWShift = 0;
constexpr uint8_t cCapPermBitFieldLMShift = 1;
constexpr uint8_t cCapPermBitFieldLGShift = 2;
constexpr uint8_t cCapPermBitFieldSLShift = 3;
constexpr uint8_t cCapPermBitFieldGLShift = 4;
constexpr uint8_t cCapPermBitFieldCShift = 5;
constexpr uint8_t cCapPermBitFieldSDPShift = cCapPermBitFieldCShift + 1;
constexpr uint8_t cCapPermBitFieldReserved1_0Shift = cCapPermBitFieldSDPShift + cCapPermBitFieldSDPWidth;
constexpr uint8_t cCapPermBitFieldASRShift = cCapPermBitFieldReserved1_0Shift + cCapPermBitFieldReserved1_0Width;
constexpr uint8_t cCapPermBitFieldXShift = cCapPermBitFieldASRShift + 1;
constexpr uint8_t cCapPermBitFieldRShift = cCapPermBitFieldXShift + 1;
constexpr uint8_t cCapPermBitFieldReserved1_1Shift = cCapPermBitFieldRShift + 1;
constexpr uint8_t cCapPermBitFieldReserved0Shift = cCapPermBitFieldReserved1_1Shift + cCapPermBitFieldReserved1_1Width;

static_assert(cCapPermBitFieldReserved0Shift + cCapPermBitFieldReserved0Width == cXlen);

uint64_t fixMalformedPerms(uint64_t perms);
