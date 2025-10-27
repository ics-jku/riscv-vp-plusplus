#pragma once
#define HANDLE_CHERI_EXCEPTIONS
#include <cstdint>

#include "cheri_capability.h"

inline uint64_t getRepresentableAlignmentMask(uint64_t len) {
	Capability c = cDefaultCap;
	c.setCapBounds(0, len);
	uint8_t e = c.fields.E < cCapMaxE ? c.fields.E : cCapMaxE;
	uint8_t e_prime = c.fields.internal_E ? e + cInternalETakeBits : 0;
	return e_prime == 0 ? 0xFFFFFFFFFFFFFFFF : ((1ULL << (cCapAddrWidth - e_prime)) - 1) << e_prime;
}

inline uint64_t getRepresentableLength(uint64_t len) {
	uint64_t m = getRepresentableAlignmentMask(len);
	return (len + ~(m)) & m;
}

std::string capToString(Capability cap);
std::string capToShortString(Capability cap);

/**
 * We use a single feature flag controlling DDC/PCC relocation.
 * However, the code uses two separate guards to make it possible
 * to bring back only one of the two for future experiments.
 */
constexpr bool have_cheri_relocation() {
	return false;
}

/**
 * DDC relocation will not be part of an initial standardized version of
 * CHERI-RISC-V. However, it may be useful to enable for research
 * prototypes and could potentially be an optional extensions, so we use
 * this hardcoded boolean and a helper function (ddc_and_resulting_addr)
 * to allow turning it back on again rather than removing the code outright.
 */

constexpr bool have_ddc_relocation() {
	return have_cheri_relocation();
}

/**
 * PCC relocation will not be part of an initial standardized version of
 * CHERI-RISC-V. Without PCC relocation the RISC-V integer PC is always
 * equal to PCC.address which allows us to simplify various corner cases
 * (e.g. writes to SCRs that could end up in PCC)
 */
constexpr bool have_pcc_relocation() {
	return have_cheri_relocation();
}

/**
 * Returns an integer program counter from a given capability.
 * By default, this is equivalent to reading the address field of the capability,
 * but if PCC relocation is enabled (not part of standardized CHERI-RISC-V) it
 * returns the capability offset instead.
 */
inline uint64_t capToIntegerPC(Capability cap) {
	if constexpr (have_pcc_relocation()) {
		return cap.getOffset();
	}
	return cap.fields.address;
}

/**
 * Updates a capability to reference an integer program counter.
 * By default, this is equivalent to updating the address field of the capability,
 * but if PCC relocation is enabled (not part of standardized CHERI-RISC-V) it
 * updates the capability offset instead.
 */
inline Capability updateCapWithIntegerPC(Capability cap, uint64_t pc) {
	if constexpr (have_pcc_relocation()) {
		cap.setCapOffsetChecked(pc);
		return cap;
	}
	cap.setCapAddrChecked(static_cast<int64_t>(pc));
	return cap;
}
