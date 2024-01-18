#include "iss.h"
inline void rv64::ISS::flw() {
	uint32_t addr = regs[instr.rs1()] + instr.I_imm();
	trap_check_addr_alignment<4, true>(addr);
	fp_regs.write(RD, float32_t{(uint32_t)mem->load_word(addr)});
}

inline void rv64::ISS::fsw() {
	uint32_t addr = regs[instr.rs1()] + instr.S_imm();
	trap_check_addr_alignment<4, false>(addr);
	mem->store_word(addr, fp_regs.u32(RS2));
}

inline void rv64::ISS::fadd_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_add(fp_regs.f32(RS1), fp_regs.f32(RS2)));
	fp_finish_instr();
}

inline void rv64::ISS::fsub_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_sub(fp_regs.f32(RS1), fp_regs.f32(RS2)));
	fp_finish_instr();
}

inline void rv64::ISS::fmul_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_mul(fp_regs.f32(RS1), fp_regs.f32(RS2)));
	fp_finish_instr();
}

inline void rv64::ISS::fdiv_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_div(fp_regs.f32(RS1), fp_regs.f32(RS2)));
	fp_finish_instr();
}

inline void rv64::ISS::fsqrt_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_sqrt(fp_regs.f32(RS1)));
	fp_finish_instr();
}

inline void rv64::ISS::fmin_s() {
	fp_prepare_instr();
	bool rs1_smaller = f32_lt_quiet(fp_regs.f32(RS1), fp_regs.f32(RS2)) ||
	                   (f32_eq(fp_regs.f32(RS1), fp_regs.f32(RS2)) && f32_isNegative(fp_regs.f32(RS1)));
	if (f32_isNaN(fp_regs.f32(RS1)) && f32_isNaN(fp_regs.f32(RS2))) {
		fp_regs.write(RD, f32_defaultNaN);
	} else {
		if (rs1_smaller)
			fp_regs.write(RD, fp_regs.f32(RS1));
		else
			fp_regs.write(RD, fp_regs.f32(RS2));
	}

	fp_finish_instr();
}

inline void rv64::ISS::fmax_s() {
	fp_prepare_instr();
	bool rs1_greater = f32_lt_quiet(fp_regs.f32(RS2), fp_regs.f32(RS1)) ||
	                   (f32_eq(fp_regs.f32(RS2), fp_regs.f32(RS1)) && f32_isNegative(fp_regs.f32(RS2)));
	if (f32_isNaN(fp_regs.f32(RS1)) && f32_isNaN(fp_regs.f32(RS2))) {
		fp_regs.write(RD, f32_defaultNaN);
	} else {
		if (rs1_greater)
			fp_regs.write(RD, fp_regs.f32(RS1));
		else
			fp_regs.write(RD, fp_regs.f32(RS2));
	}

	fp_finish_instr();
}

inline void rv64::ISS::fmadd_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), fp_regs.f32(RS3)));
	fp_finish_instr();
}

inline void rv64::ISS::fmsub_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_mulAdd(fp_regs.f32(RS1), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
	fp_finish_instr();
}

inline void rv64::ISS::fnmadd_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), f32_neg(fp_regs.f32(RS3))));
	fp_finish_instr();
}

inline void rv64::ISS::fnmsub_s() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, f32_mulAdd(f32_neg(fp_regs.f32(RS1)), fp_regs.f32(RS2), fp_regs.f32(RS3)));
	fp_finish_instr();
}

inline void rv64::ISS::fcvt_w_s() {
	fp_prepare_instr();
	fp_setup_rm();
	regs[RD] = f32_to_i32(fp_regs.f32(RS1), softfloat_roundingMode, true);
	fp_finish_instr();
}

inline void rv64::ISS::fcvt_wu_s() {
	fp_prepare_instr();
	fp_setup_rm();
	regs[RD] = f32_to_ui32(fp_regs.f32(RS1), softfloat_roundingMode, true);
	fp_finish_instr();
}

inline void rv64::ISS::fcvt_s_w() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, i32_to_f32(regs[RS1]));
	fp_finish_instr();
}

inline void rv64::ISS::fcvt_s_wu() {
	fp_prepare_instr();
	fp_setup_rm();
	fp_regs.write(RD, ui32_to_f32(regs[RS1]));
	fp_finish_instr();
}

inline void rv64::ISS::fsgnj_s() {
	fp_prepare_instr();
	auto f1 = fp_regs.f32(RS1);
	auto f2 = fp_regs.f32(RS2);
	fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (f2.v & F32_SIGN_BIT)});
	fp_set_dirty();
}

inline void rv64::ISS::fsgnjn_s() {
	fp_prepare_instr();
	auto f1 = fp_regs.f32(RS1);
	auto f2 = fp_regs.f32(RS2);
	fp_regs.write(RD, float32_t{(f1.v & ~F32_SIGN_BIT) | (~f2.v & F32_SIGN_BIT)});
	fp_set_dirty();
}

inline void rv64::ISS::fsgnjx_s() {
	fp_prepare_instr();
	auto f1 = fp_regs.f32(RS1);
	auto f2 = fp_regs.f32(RS2);
	fp_regs.write(RD, float32_t{f1.v ^ (f2.v & F32_SIGN_BIT)});
	fp_set_dirty();
}

inline void rv64::ISS::fmv_w_x() {
	fp_prepare_instr();
	fp_regs.write(RD, float32_t{(uint32_t)regs[RS1]});
	fp_set_dirty();
}

inline void rv64::ISS::fmv_x_w() {
	fp_prepare_instr();
	regs[RD] = fp_regs.u32(RS1);
}

inline void rv64::ISS::feq_s() {
	fp_prepare_instr();
	regs[RD] = f32_eq(fp_regs.f32(RS1), fp_regs.f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::flt_s() {
	fp_prepare_instr();
	regs[RD] = f32_lt(fp_regs.f32(RS1), fp_regs.f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::fle_s() {
	fp_prepare_instr();
	regs[RD] = f32_le(fp_regs.f32(RS1), fp_regs.f32(RS2));
	fp_update_exception_flags();
}

inline void rv64::ISS::fclass_s() {
	fp_prepare_instr();
	regs[RD] = f32_classify(fp_regs.f32(RS1));
}