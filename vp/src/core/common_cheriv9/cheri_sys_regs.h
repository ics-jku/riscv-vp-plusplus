#pragma once
#include "cheri_cap_common.h"

struct Mtvec {
	union {
		uint64_t bits;
		struct {
			uint8_t mode : 2;
			uint64_t base : 62;
		} fields;
	};
};

inline Mtvec MkMtvec(uint64_t t) {
	Mtvec mtvec{};
	mtvec.bits = t;
	return mtvec;
}

enum TrapVectorMode { TV_Direct, TV_Vector, TV_Reserved };

inline TrapVectorMode trapVectorModeOfBits(uint64_t bits) {
	switch (bits) {
		case 0:
			return TV_Direct;
		case 1:
			return TV_Vector;
		default:
			return TV_Reserved;
	}
}

inline Mtvec updateMode(Mtvec o, uint8_t mode) {
	Mtvec n = o;
	n.fields.mode = mode;
	return n;
}
inline Mtvec legalizeTvec(const Mtvec o, uint64_t v) {
	Mtvec v_mtvec = MkMtvec(v);
	switch (trapVectorModeOfBits(v_mtvec.fields.mode)) {
		case TV_Direct:
		case TV_Vector:
			return v_mtvec;
		default:
			return updateMode(v_mtvec, o.fields.mode);
	}
}

inline Capability legalizeTcc(const Capability& o, const Capability& v) {
	CapAddr_t new_base = v.getBase();
	if (have_pcc_relocation() & (((new_base & 0b1) != 0) | ((new_base & 0b10) != 0))) {
		return o;
	}
	CapAddr_t new_tvec = capToIntegerPC(v);
	Mtvec new_mtvec = MkMtvec(capToIntegerPC(o));
	CapAddr_t legalized_tvec = legalizeTvec(new_mtvec, new_tvec).bits;
	return updateCapWithIntegerPC(v, legalized_tvec);
}
