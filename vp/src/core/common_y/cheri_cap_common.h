#pragma once
#include <cstdint>

#include "cheri_capability.h"

inline uint64_t getRepresentableAlignmentMask(uint64_t len) {
	Capability c = cDefaultCap;
	c.setCapBounds(0, len);
	uint8_t e = std::min(c.cap.fields.E, cCapMaxE);
	uint8_t e_prime = c.cap.fields.internal_E ? e + cInternalETakeBits : 0;
	return e_prime == 0 ? 0xFFFFFFFFFFFFFFFF : ((1ULL << (cCapAddrWidth - e_prime)) - 1) << e_prime;
}