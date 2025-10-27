#pragma once
#include <bitset>
#include <cstdint>

#include "core/common_cheriv9/cheri_constants.h"
#include "util/common.h"

#define CHERI_MAX(n) \
	((n) < (sizeof(uint64_t) * 8) ? ((1ULL << (n)) - 1) : 0xFFFFFFFFFFFFFFFFULL)  // Catch edge case of MAX(64)
#define CHERI_BIT_RANGE(instr, upper, lower) (instr & (((1ULL << (upper - lower + 1ULL)) - 1ULL) << lower))
#define CHERI_BIT_SLICE(instr, upper, lower) (CHERI_BIT_RANGE(instr, upper, lower) >> lower)
#define CHERI_BIT_SINGLE(instr, pos) (instr & (1ULL << pos))
#define CHERI_BIT_SINGLE_P1(instr, pos) (CHERI_BIT_SINGLE(instr, pos) >> pos)

template <typename T>
inline T cheri_truncateLsb(T value, int num_bits, int total_bits) {
	// Mask to keep 'num_bits' MSBs
	T mask = ((1ULL << num_bits) - 1) << (total_bits - num_bits);
	return (value & mask) >> (total_bits - num_bits);
}

constexpr uint8_t cCapSize = 16;  // Size of a capability in bytes, ignoring tag
constexpr uint8_t cXlen = 64;

// Capability architectural sizes
constexpr uint8_t cCapUPermsWidth = 4;
constexpr uint8_t cCapHPermsWidth = 12;
constexpr uint8_t cCapReservedWidth = 2;
constexpr uint8_t cCapFlagsWidth = 1;
constexpr uint8_t cCapEWidth = 6;
constexpr uint8_t cCapMantissaWidth = 14;
constexpr uint8_t cCapOTypeWidth = 18;
constexpr uint8_t cCapAddrWidth = cXlen;

constexpr uint8_t cCapsPerCacheLine = 4;
constexpr uint8_t cInternalETakeBits = 3;

constexpr __uint128_t cNullCap128 = static_cast<__uint128_t>(0x1FFFFC018004)
                                    << 64;  // TODO This should be linked to cNullCap

constexpr uint8_t cEncCapPermsWidth = cCapHPermsWidth + cCapUPermsWidth;
constexpr uint8_t cCapUPermsShift = 15;  // TODO Why is this 15?
constexpr uint8_t cCapPermsWidth = cCapUPermsShift + cCapUPermsWidth;

// Constants that are calculated dependent on architecture
constexpr int cap_max_otype = CHERI_MAX(cCapOTypeWidth) - reserved_otypes;
constexpr uint64_t cCapMaxAddr = CHERI_MAX(cCapAddrWidth);
constexpr uint64_t cCapMaxOType = CHERI_MAX(cCapOTypeWidth) - cReservedOtypes;
constexpr uint32_t cOtypeUnsealedUnsigned = (1 << cCapOTypeWidth) + cOtypeUnsealed;  // All bits set for cCapOTypeWidth
constexpr uint32_t cOtypeSentryUnsigned =
    (1 << cCapOTypeWidth) + cOtypeSentry;  // All bits except for LSB set for cCapOTypeWidth

constexpr uint8_t cCapLenWidth = cCapAddrWidth + 1;

constexpr __uint128_t cCapLenMax = (static_cast<__uint128_t>(1ULL) << cCapLenWidth) - 1;

constexpr uint64_t cCapAddrMax = CHERI_MAX(cCapAddrWidth);

constexpr uint8_t cCapMaxE = cCapLenWidth - cCapMantissaWidth + 1;
constexpr uint8_t cCapResetE = cCapMaxE;
constexpr uint16_t cCapResetT = 0b01 << (cCapMantissaWidth - 2);

using CapBase_t = uint64_t;
using CapTop_t = __uint128_t;
using CapAddr_t = uint64_t;
using CapLen_t = __uint128_t;
using CapInt_t = __uint128_t;

// Forward declaration for conversion functions
struct Capability;

struct EncCapability {
	uint32_t perms : cEncCapPermsWidth;
	uint32_t otype : cCapOTypeWidth;
	uint8_t reserved : cCapReservedWidth;
	bool flags : cCapFlagsWidth;
	bool internal_E : 1;
	uint16_t T : cCapMantissaWidth - 2;
	uint16_t B : cCapMantissaWidth;
	int64_t address : cCapAddrWidth;

	static Capability toCapability(bool tag, const EncCapability& c);
};

__uint128_t encCapToUint128(EncCapability cap);
EncCapability uint128ToEncCapability(__uint128_t data);
