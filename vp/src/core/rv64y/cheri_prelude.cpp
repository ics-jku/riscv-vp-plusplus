#include "cheri_prelude.h"

#include "core/common_y/cheri_capability.h"

Capability EncCapability::toCapability(const bool tag, const EncCapability& c) {
	return Capability(c.toUint128(), tag);
}

__uint128_t encCapToUint128(const EncCapability& cap) {
	return cap.toUint128();
}

EncCapability uint128ToEncCapability(__uint128_t data) {
	return EncCapability(data);
}

uint64_t fixMalformedPerms(uint64_t perms) {
	// Rules according to 0.9.8.2 spec
	// Rule 		Permission 		Valid only if
	// base-1		C-permission 	R-permission or W-permission
	// base-2		LM-permission 	C-permission and R-permission
	// base-3		ASR-permission 	X-permission
	// Zylevels1-1	LG-permission 	C-permission and R-permission
	// Zylevels1-2	SL-permission 	C-permission and W-permission
	// Hybrid		P-bit			X-permission // This is not enforced here, as P-bit is not part of the packed perms
	// base-1
	CLEAR_PERM_IF(C, !(HAS_PERM(R) || HAS_PERM(W)));
	// base-2
	CLEAR_PERM_IF(LM, !(HAS_PERM(C) && HAS_PERM(R)));
	// base-3
	CLEAR_PERM_IF(ASR, !HAS_PERM(X));
	// Zylevels1-1
	CLEAR_PERM_IF(LG, !(HAS_PERM(C) && HAS_PERM(R)));
	// Zylevels1-2
	CLEAR_PERM_IF(SL, !(HAS_PERM(C) && HAS_PERM(W)));

	return perms;
}
