#include "iss.h"

// flw/fld can just be done via the normal load/store instruction
inline void rv64::ISS::flw() {
	RAISE_ILLEGAL_INSTRUCTION();
}

inline void rv64::ISS::fsw() {
	RAISE_ILLEGAL_INSTRUCTION();
}

// moving from float to regular registers (or vice versa) does not make sense here
inline void rv64::ISS::fmv_w_x() {
	RAISE_ILLEGAL_INSTRUCTION();
}

inline void rv64::ISS::fmv_x_w() {
	RAISE_ILLEGAL_INSTRUCTION();
}

inline void rv64::ISS::fadd_s() {
	regs.write_f32(RD, f32_add(regs.read_f32(RS1), regs.read_f32(RS2)));
}

inline void rv64::ISS::fsub_s() {
	regs.write_f32(RD, f32_sub(regs.read_f32(RS1), regs.read_f32(RS2)));
}

inline void rv64::ISS::fmul_s() {
	regs.write_f32(RD, f32_mul(regs.read_f32(RS1), regs.read_f32(RS2)));
}

inline void rv64::ISS::fdiv_s() {
	regs.write_f32(RD, f32_div(regs.read_f32(RS1), regs.read_f32(RS2)));
}

inline void rv64::ISS::fsqrt_s() {
	regs.write_f32(RD, f32_sqrt(regs.read_f32(RS1)));
}

inline void rv64::ISS::fmin_s() {
	bool rs1_smaller = f32_lt_quiet(regs.read_f32(RS1), regs.read_f32(RS2)) ||
	                   (f32_eq(regs.read_f32(RS1), regs.read_f32(RS2)) && f32_isNegative(regs.read_f32(RS1)));
	if (f32_isNaN(regs.read_f32(RS1)) && f32_isNaN(regs.read_f32(RS2))) {
		regs.write_f32(RD, f32_defaultNaN);
	} else {
		if (rs1_smaller)
			regs.write_f32(RD, regs.read_f32(RS1));
		else
			regs.write_f32(RD, regs.read_f32(RS2));
	}
}

inline void rv64::ISS::fmax_s() {
	bool rs1_greater = f32_lt_quiet(regs.read_f32(RS2), regs.read_f32(RS1)) ||
	                   (f32_eq(regs.read_f32(RS2), regs.read_f32(RS1)) && f32_isNegative(regs.read_f32(RS2)));
	if (f32_isNaN(regs.read_f32(RS1)) && f32_isNaN(regs.read_f32(RS2))) {
		regs.write_f32(RD, f32_defaultNaN);
	} else {
		if (rs1_greater)
			regs.write_f32(RD, regs.read_f32(RS1));
		else
			regs.write_f32(RD, regs.read_f32(RS2));
	}
}

inline void rv64::ISS::fmadd_s() {
	regs.write_f32(RD, f32_mulAdd(regs.read_f32(RS1), regs.read_f32(RS2), regs.read_f32(RS3)));
}

inline void rv64::ISS::fmsub_s() {
	regs.write_f32(RD, f32_mulAdd(regs.read_f32(RS1), regs.read_f32(RS2), f32_neg(regs.read_f32(RS3))));
}

inline void rv64::ISS::fnmadd_s() {
	regs.write_f32(RD, f32_mulAdd(f32_neg(regs.read_f32(RS1)), regs.read_f32(RS2), f32_neg(regs.read_f32(RS3))));
}

inline void rv64::ISS::fnmsub_s() {
	regs.write_f32(RD, f32_mulAdd(f32_neg(regs.read_f32(RS1)), regs.read_f32(RS2), regs.read_f32(RS3)));
}

inline void rv64::ISS::fcvt_w_s() {
	regs[RD] = f32_to_i32(regs.read_f32(RS1), softfloat_roundingMode, true);
}

inline void rv64::ISS::fcvt_wu_s() {
	regs[RD] = f32_to_ui32(regs.read_f32(RS1), softfloat_roundingMode, true);
}

inline void rv64::ISS::fcvt_s_w() {
	regs.write_f32(RD, i32_to_f32(regs[RS1]));
}

inline void rv64::ISS::fcvt_s_wu() {
	regs.write_f32(RD, ui32_to_f32(regs[RS1]));
}

inline void rv64::ISS::fsgnj_s() {
	auto f1 = regs.read_f32(RS1);
	auto f2 = regs.read_f32(RS2);
	regs.write_f32(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (f2.v & F32_SIGN_BIT)});
}

inline void rv64::ISS::fsgnjn_s() {
	auto f1 = regs.read_f32(RS1);
	auto f2 = regs.read_f32(RS2);
	regs.write_f32(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (~f2.v & F32_SIGN_BIT)});
}

inline void rv64::ISS::fsgnjx_s() {
	auto f1 = regs.read_f32(RS1);
	auto f2 = regs.read_f32(RS2);
	regs.write_f32(RD, float32_t{f1.v ^ (f2.v & F32_SIGN_BIT)});
}

inline void rv64::ISS::feq_s() {
	regs[RD] = f32_eq(regs.read_f32(RS1), regs.read_f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::flt_s() {
	regs[RD] = f32_lt(regs.read_f32(RS1), regs.read_f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::fle_s() {
	regs[RD] = f32_le(regs.read_f32(RS1), regs.read_f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::fclass_s() {
	regs[RD] = f32_classify(regs.read_f32(RS1));
}