#pragma once

#include <cstdint>

#include "cheri_cap_common.h"

constexpr uint64_t cTvecModeMask = 0x3;
constexpr uint64_t cTvecReservedModeBit = 0x2;
constexpr uint64_t cTvecBaseMask = ~cTvecModeMask;

/*
 * QEMU uses RISCV_HICAUSE = 0x3f when checking whether a vectored trap table is
 * representable. Keep the same limit here so tvecc tag behavior matches QEMU.
 */
constexpr uint64_t cTvecHighestCause = 0x3f;

inline uint64_t legalizeTvecBits(uint64_t value) {
	return value & ~cTvecReservedModeBit;
}

inline bool tvecIsVectored(uint64_t value) {
	return (value & cTvecModeMask) == 1;
}

inline uint64_t tvecBaseAddress(uint64_t value) {
	return value & cTvecBaseMask;
}

inline uint64_t tvecMaxVectoredAddress(uint64_t value) {
	return tvecBaseAddress(value) + cTvecHighestCause * 4;
}

inline bool capCanRepresentAddress(const Capability& cap, uint64_t address) {
	Capability probe = cap;
	return probe.setCapAddr(static_cast<int64_t>(address));
}

inline Capability legalizeTvecc(Capability value) {
	uint64_t legalized = legalizeTvecBits(static_cast<uint64_t>(value.cap.fields.address));

	value.setCapAddrChecked(static_cast<int64_t>(legalized));

	if (tvecIsVectored(legalized)) {
		value.clearTagIf(!capCanRepresentAddress(value, tvecMaxVectoredAddress(legalized)));
	}

	return value;
}
