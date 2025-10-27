#include "cheri_prelude.h"

#include "core/common_cheriv9/cheri_capability.h"

Capability EncCapability::toCapability(const bool tag, const EncCapability& c) {
	bool internal_E = c.internal_E;
	uint8_t E = 0;
	uint16_t Bs = c.B;
	uint16_t T = c.T;
	uint8_t lenMSBs = 0;
	if (internal_E) {
		E = ((T & 0b111) << cInternalETakeBits) | (Bs & 0b111);
		lenMSBs = 0b01;
		T &= (~(0b111));
		Bs &= (~(0b111));
	}
	uint8_t carry_out = T < CHERI_BIT_RANGE(Bs, cCapMantissaWidth - 3, 0);
	uint16_t Ttop2 = CHERI_BIT_SLICE(Bs, (cCapMantissaWidth - 1), (cCapMantissaWidth - 2)) + lenMSBs + carry_out;
	uint8_t uperms = CHERI_BIT_SLICE(c.perms, cCapHPermsWidth + cCapUPermsWidth - 1, cCapHPermsWidth);
	T |= (Ttop2 << (cCapMantissaWidth - 2));

	return Capability(tag, uperms, CHERI_BIT_SINGLE_P1(c.perms, 11), CHERI_BIT_SINGLE_P1(c.perms, 10),
	                  CHERI_BIT_SINGLE_P1(c.perms, 9), CHERI_BIT_SINGLE_P1(c.perms, 8), CHERI_BIT_SINGLE_P1(c.perms, 7),
	                  CHERI_BIT_SINGLE_P1(c.perms, 6), CHERI_BIT_SINGLE_P1(c.perms, 5), CHERI_BIT_SINGLE_P1(c.perms, 4),
	                  CHERI_BIT_SINGLE_P1(c.perms, 3), CHERI_BIT_SINGLE_P1(c.perms, 2), CHERI_BIT_SINGLE_P1(c.perms, 1),
	                  CHERI_BIT_SINGLE_P1(c.perms, 0), c.reserved, CHERI_BIT_SINGLE_P1(c.flags, 0), internal_E, E, Bs,
	                  T, c.otype, c.address);
}

__uint128_t encCapToUint128(EncCapability cap) {
	return static_cast<__uint128_t>(static_cast<uint64_t>(cap.address)) |
	       static_cast<__uint128_t>(cap.B) << cCapAddrWidth |
	       static_cast<__uint128_t>(cap.T) << (cCapAddrWidth + cCapMantissaWidth) |
	       static_cast<__uint128_t>(cap.internal_E) << (cCapAddrWidth + cCapMantissaWidth + cCapMantissaWidth - 2) |
	       static_cast<__uint128_t>(cap.otype) << (cCapAddrWidth + cCapMantissaWidth + cCapMantissaWidth - 2 + 1) |
	       static_cast<__uint128_t>(cap.flags)
	           << (cCapAddrWidth + cCapMantissaWidth + cCapMantissaWidth - 2 + 1 + cCapOTypeWidth) |
	       static_cast<__uint128_t>(cap.reserved)
	           << (cCapAddrWidth + cCapMantissaWidth + cCapMantissaWidth - 2 + 1 + cCapOTypeWidth + cCapFlagsWidth) |
	       static_cast<__uint128_t>(cap.perms) << (cCapAddrWidth + cCapMantissaWidth + cCapMantissaWidth - 2 + 1 +
	                                               cCapOTypeWidth + cCapFlagsWidth + cCapReservedWidth);
}

EncCapability uint128ToEncCapability(__uint128_t data) {
	EncCapability encCap{};
	uint64_t metadata = data >> 64;
	encCap.perms = CHERI_BIT_SLICE(metadata, 63, 48);
	encCap.reserved = CHERI_BIT_SLICE(metadata, 47, 46);
	encCap.flags = CHERI_BIT_SINGLE_P1(metadata, 45);
	encCap.otype = CHERI_BIT_SLICE(metadata, 44, 27);
	encCap.internal_E = CHERI_BIT_SINGLE_P1(metadata, 26);
	encCap.T = CHERI_BIT_SLICE(metadata, 25, 14);
	encCap.B = CHERI_BIT_SLICE(metadata, 13, 0);
	encCap.address = static_cast<int64_t>(data);
	return encCap;
}
