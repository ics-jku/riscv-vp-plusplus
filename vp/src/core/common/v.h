#pragma once
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstring>

/*
 * print unmet traps (reasons) to stdout
 * (see v_assert)
 */
//#define DEBUG_PRINT_TRAPS
#undef DEBUG_PRINT_TRAPS

// TODO these should be compile arguments
constexpr unsigned VLEN = 512;
constexpr unsigned ELEN = 64;
constexpr unsigned SEW_MIN = 8;
constexpr unsigned VLENB = VLEN / 8;
constexpr unsigned NUM_REGS = 32;

typedef uint64_t xlen_reg_t;  // TODO change to generic

typedef uint64_t op_reg_t;
typedef int64_t s_op_reg_t;

template <typename iss_type>
class VExtension {
   private:
	void* v_regs;  // TODO: could be initialized randomly
	iss_type& iss;

   public:
	constexpr static unsigned VS_OFF = 0b00;
	constexpr static unsigned VS_INITIAL = 0b01;
	constexpr static unsigned VS_CLEAN = 0b10;
	constexpr static unsigned VS_DIRTY = 0b11;

	enum elem_sel_t {
		// vd widen enable
		// op2 widen enable
		// op1 widen enable
		// vd signed
		// op2 signed
		// op1 signed
		xxxuuu = 0b000000,
		xxxuus = 0b000001,
		xxxssu = 0b000110,
		xxxsss = 0b000111,
		xwxuuu = 0b010000,
		xwxssu = 0b010110,
		xwxsss = 0b010111,
		wxxuuu = 0b100000,
		wxxuus = 0b100001,
		wxxusu = 0b100010,
		wxxsss = 0b100111,
		wxwuuu = 0b101000,
		wxwsss = 0b101111,
		wwxuuu = 0b110000,
		wwxsss = 0b110111,
	};
	enum param_sel_t { vv, vi, vx, vf, v };
	enum load_store_type_t { standard, standard_reg, masked, indexed, fofl, whole };
	enum load_store_t { load, store };

	elem_sel_t elem_sel;
	param_sel_t param_sel;
	op_reg_t vd_eew_overwrite;
	op_reg_t o2_eew_overwrite;
	op_reg_t o1_eew_overwrite;
	bool require_no_overlap;
	bool require_vd_not_v0;
	bool ignoreAlignment;
	bool ignoreOverlap;
	bool vd_is_mask;
	bool vs1_is_mask;
	bool vs2_is_mask;
	bool vd_is_scalar;
	bool vs1_is_scalar;

	VExtension(iss_type& iss) : iss(iss) {
		assert(ELEN >= 8 && isPowerOfTwo(ELEN));
		assert(VLEN >= ELEN && isPowerOfTwo(VLEN) && VLEN <= 1 << 16);  // TODO: is last limit realistic?
		iss.csrs.vlenb.reg = VLENB;
		v_regs = malloc(NUM_REGS * VLENB);
		memset(v_regs, 0, NUM_REGS * VLENB);
	}

	template <typename T>
	void reg_write(xlen_reg_t vec_idx, xlen_reg_t elem_num, T val) {
		get_reg<T>(vec_idx, elem_num) = val;
	}

	template <typename T>
	T reg_read(xlen_reg_t vec_idx, xlen_reg_t elem_num) {
		return get_reg<T>(vec_idx, elem_num);
	}

	template <typename T>
	T& get_reg(xlen_reg_t vec_idx, xlen_reg_t elem_num) {
		xlen_reg_t num_elem_per_reg = (VLENB) / (sizeof(T));
		vec_idx += elem_num / num_elem_per_reg;
		elem_num = elem_num % num_elem_per_reg;

		T* reg_element = (T*)((char*)v_regs + vec_idx * VLENB);
		return reg_element[elem_num];
	}

	xlen_reg_t iss_reg_read(xlen_reg_t addr) {
		op_reg_t val = iss.regs[addr];
		return val;
	}

	/*
	 * we need this only for RV32
	 * (e.g. for reading addresses without sign-extension to 64 bit)
	 * for RV64 the result is similar to iss_reg_read
	 */
	xlen_reg_t iss_reg_read_unsigned(xlen_reg_t addr) {
		op_reg_t val = iss.regs[addr];
		if (sizeof(iss.regs[addr]) == 4)
			val = val & 0xffffffff;
		return val;
	}

	void iss_reg_write(xlen_reg_t addr, xlen_reg_t value) {
		iss.regs[addr] = value;
	}

	void set_fp_rm() {
		auto frm = iss.csrs.fcsr.fields.frm;
		v_assert(frm <= FRM_RMM, "invalid frm");
		softfloat_roundingMode = frm;
	}

	op_reg_t fp_reg_read(xlen_reg_t addr, op_reg_t sew) {
		op_reg_t raw_val = iss.fp_regs.f64(addr).v;
		op_reg_t actual_val;
		switch (sew) {
			case 16:
				actual_val = iss.fp_regs.f16(addr).v;
				break;
			case 32:
				actual_val = iss.fp_regs.f32(addr).v;
				break;
			case 64:
				actual_val = iss.fp_regs.f64(addr).v;
				break;
			case 0:
				actual_val = raw_val;
				break;
			default:
				v_assert(false, "invalid sew");
		}
		return actual_val;
	}

	void fp_reg_write(xlen_reg_t addr, op_reg_t value, op_reg_t sew) {
		switch (sew) {
			case 16:
				iss.fp_regs.write(addr, f16(value));
				break;
			case 32:
				iss.fp_regs.write(addr, f32(value));
				break;
			case 64:
				iss.fp_regs.write(addr, f64(value));
				break;
			default:
				v_assert(false, "invalid sew");
		}
	}

	op_reg_t invertBytes(op_reg_t memory, op_reg_t numBits) {
		op_reg_t rev_mem = 0;
		for (xlen_reg_t idx = 0; idx < numBits / 8; idx++) {
			rev_mem |= ((memory >> (idx * 8)) & ((1 << 8) - 1)) << ((numBits / 8 - idx - 1) * 8);
		}
		return rev_mem;
	}

	void v_assert(bool cond) {
		v_assert(cond, "Caught unknown error");
	}

	void v_assert(bool cond, std::string error_message) {
		if (!(cond)) {
#ifdef DEBUG_PRINT_TRAPS
			std::cout << error_message << std::endl;
#endif
			raise_trap(EXC_ILLEGAL_INSTR, iss.instr.data());
		}
	}

	bool v_is_aligned(op_reg_t addr, op_reg_t alignment) {
		return alignment == 0 || (addr & (alignment - 1)) == 0;
	}

	/* check, if the overlap of two registers is allowed */
	bool vreg_overlap_valid(op_reg_t vd, double vd_emul, unsigned int vd_eew, op_reg_t vs, double vs_emul,
	                        unsigned int vs_eew, bool strict) {
		if ((vd > vs + std::ceil(vs_emul) - 1) || (vs > vd + std::ceil(vd_emul) - 1)) {
			/* no overlap at all -> valid */
			// std::cout << "NO OVERLAP" << std::endl;
			return true;
		}

		if (strict) {
			/* overlap strictly forbidden -> invalid */
			// std::cout << "OVERLAP AND STRICTLY FORBIDDEN" << std::endl;
			return false;
		}

		if (vd_eew == vs_eew) {
			/* C1: The destination EEW equals the source EEW. */
			// std::cout << "OVERLAP OK C1" << std::endl;
			return true;
		}

		if (vd_eew < vs_eew) {
			/*
			 * C2:
			 * The destination EEW is smaller than the source EEW and the overlap
			 * is in the lowest-numbered part of the source register group
			 */
			if (vd == vs) {
				// std::cout << "OVERLAP OK C2" << std::endl;
				return true;
			}
		}

		if (vd_eew > vs_eew && vs_emul >= 1) {
			/*
			 * C3:
			 * The destination EEW is greater than the source EEW, the source EMUL
			 * is at least 1, and the overlap is in the highest-numbered part of
			 * the destination register group
			 */
			if (vs + vs_emul - 1 >= vd + vd_emul - 1) {
				// std::cout << "OVERLAP OK C3" << std::endl;
				return true;
			}
		}

		/* invalid */
		// std::cout << "OVERLAP AND INVALID" << std::endl;
		return false;
	}

	void prepInstr(bool require_not_off, bool require_vill, bool is_fp) {
		if (require_not_off) {
			requireNotOff();
		}

		iss.csrs.mstatus.fields.sd = 1;
		iss.csrs.mstatus.fields.vs = VS_DIRTY;

		if (require_vill) {
			v_assert(iss.csrs.vtype.fields.vill != 1, "vill == 1");
		}

		if (is_fp) {
			/*
			 * Half-precision fp is implemented, but disabled for now
			 * Simply remove the v_assert as soon as the Zfh extension is supported
			 */
			v_assert(getIntVSew() >= 32, "half-precision fp not supported");

			iss.fp_prepare_instr();
			set_fp_rm();
		}

		vd_eew_overwrite = 0;
		o1_eew_overwrite = 0;
		o2_eew_overwrite = 0;
		require_no_overlap = false;
		require_vd_not_v0 = true;
		ignoreAlignment = false;
		ignoreOverlap = false;
		vd_is_mask = false;
		vs1_is_mask = false;
		vs2_is_mask = false;
		vd_is_scalar = false;
		vs1_is_scalar = false;
	}

	void requireNotOff() {
		v_assert(iss.csrs.mstatus.fields.vs != VS_OFF, "vs == VS_OFF");
	}

	void finishInstr(bool is_fp) {
		iss.csrs.vstart.reg = 0;

		if (is_fp) {
			iss.fp_finish_instr();
		}
	}

	xlen_reg_t getIntVSew() {
		return 1 << (iss.csrs.vtype.fields.vsew + 3);
	}

	s_op_reg_t signExtend(op_reg_t value, xlen_reg_t width) {
		return ((s_op_reg_t)value << (64 - width)) >> (64 - width);
	}

	double getVlmul() {
		xlen_reg_t vlmul = iss.csrs.vtype.fields.vlmul;
		int8_t signed_vlmul = int8_t(vlmul << 5) >> 5;
		double lmul = signed_vlmul <= 0 ? 1.0 / (1 << -signed_vlmul) : 1 << signed_vlmul;
		return lmul;
	}

	void v_set_operation(xlen_reg_t rd, xlen_reg_t rs1, xlen_reg_t vtype_new, xlen_reg_t avl) {
		xlen_reg_t vlmul = BIT_RANGE(vtype_new, 2, 0);
		xlen_reg_t intVSew = 1 << (BIT_SLICE(vtype_new, 5, 3) + 3);

		bool is_vsetivli = iss.instr.bhigh() && iss.instr.bhigh2();

		int8_t signed_vlmul = int8_t(vlmul << 5) >> 5;
		double lmul = signed_vlmul <= 0 ? 1.0 / (1 << -signed_vlmul) : 1 << signed_vlmul;

		xlen_reg_t vlmax = lmul * VLEN / intVSew;

		if (!is_vsetivli) {
			if (rs1 != 0) {
				avl = iss_reg_read(rs1);
			} else if (rd != 0 && rs1 == 0) {
				avl = ~0;
			} else if (rd == 0 && rs1 == 0) {
				avl = iss.csrs.vl.reg;
			}
		}
		// VL strategy: always set to maximum allowed value
		iss.csrs.vl.reg = avl <= vlmax ? avl : vlmax;

		/* write new value (incl. possible vill) */
		iss.csrs.vtype.reg = vtype_new;
		/* check -> set possible vill */
		iss.csrs.vtype.fields.vill |= (lmul * ELEN < SEW_MIN) ||
		                              /* check reserved bits */
		                              (vtype_new & ~0xff) ||
		                              /* check reserved values */
		                              (intVSew > 64 || vlmul == (1 << 2)) ||
		                              /* check fractional lmul (see table in spec chapter 4.4.) */
		                              (intVSew / lmul > ELEN);

		/* reset values, if vill */
		if (iss.csrs.vtype.fields.vill) {
			iss.csrs.vl.reg = 0;
			iss.csrs.vtype.reg = 0;
			iss.csrs.vtype.fields.vill = 1;
		}

		if ((!is_vsetivli && !(rd == 0 && rs1 == 0)) || is_vsetivli) {
			iss_reg_write(rd, iss.csrs.vl.reg);
		}
	}

	bool isPowerOfTwo(unsigned num) {
		return ((num & (num - 1)) == 0);
	}

	op_reg_t getMask(op_reg_t numBits) {
		return numBits < 64 ? (1ul << numBits) - 1 : UINT64_MAX;
	}

	template <typename T>
	T getRoundingIncrement(T v, op_reg_t d) {
		op_reg_t vxrm = iss.csrs.vxrm.reg;
		op_reg_t r = 0;
		op_reg_t lsb = (v >> d) & 1;
		op_reg_t lsb_half = d == 0 ? 0 : (v >> (d - 1)) & 1;
		op_reg_t lsb_mask = getMask(d);
		if (vxrm == 0) {
			r = lsb_half;
		} else if (vxrm == 1) {
			r = (lsb_half) & (((v & (lsb_mask >> 1)) != 0) | lsb);
		} else if (vxrm == 2) {
			r = 0;
		} else {
			r = !lsb & ((v & lsb_mask) != 0);
		}
		return r;
	}

	template <typename T>
	T vRound(T v, op_reg_t d) {
		T r = getRoundingIncrement(v, d);
		return d == 64 ? r : (v >> d) + r;
	}

	template <typename T>
	T setSingleBit(T w, T m, T f) {
		return (w & ~m) | (-f & m);
	}

	template <typename T>
	T setSingleBitUnmasked(T w, T f, op_reg_t reg_pos) {
		return setSingleBit<T>(w, 1ul << reg_pos, f);
	}

	op_reg_t getSewSingleOperand(xlen_reg_t sew, xlen_reg_t addr, xlen_reg_t index, bool printTrace) {
		if (printTrace) {
			xlen_reg_t num_elem_per_reg = VLEN / sew;
			xlen_reg_t vec_idx = addr + index / num_elem_per_reg;
			xlen_reg_t elem_num = index % num_elem_per_reg;
			xlen_reg_t elem_num_64 = elem_num / (64 / sew);
			op_reg_t out = readSewSingleOperand(64, vec_idx, elem_num_64);
			printf("Cpu %lu Reg R v%ld_%ld val 0x%016lx mask 0xffffffffffffffff\n", (uint64_t)iss.csrs.mhartid.reg,
			       vec_idx, elem_num_64, out);
		}
		return readSewSingleOperand(sew, addr, index);
	}

	op_reg_t readSewSingleOperand(xlen_reg_t sew, xlen_reg_t addr, xlen_reg_t index) {
		switch (sew) {
			case 8:
				return get_reg<uint8_t>(addr, index);
			case 16:
				return get_reg<uint16_t>(addr, index);
			case 32:
				return get_reg<uint32_t>(addr, index);
			case 64:
				return get_reg<uint64_t>(addr, index);
			default:
				v_assert(false);
				return 0;
		};
	};

	std::pair<op_reg_t, op_reg_t> getOperands(unsigned i) {
		return getOperandsRed(i, i);
	}

	std::pair<op_reg_t, op_reg_t> getOperandsRed(unsigned i_op1, unsigned i_op2) {
		op_reg_t op1;
		op_reg_t op2;

		auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();

		op2 = getSewSingleOperand(op2_eew, iss.instr.rs2(), i_op2, false);
		switch (param_sel) {
			case param_sel_t::vv: {
				op1 = getSewSingleOperand(op1_eew, iss.instr.rs1(), i_op1, false);
			} break;
			case param_sel_t::vi: {
				op1 = iss.instr.rs1();
				if (op1_signed) {
					op1 = signExtend(op1, 5);
				}
			} break;
			case param_sel_t::vx: {
				op1 = iss_reg_read(iss.instr.rs1()) & getMask(op1_eew);
				if (op1_signed) {
					op1 = signExtend(op1, op1_eew);
				}
			} break;
			case param_sel_t::vf: {
				op1 = fp_reg_read(iss.instr.rs1(), op1_eew);
			} break;
			default:
				v_assert(false, "invalid param_sel");
		}

		return std::make_pair(op1, op2);
	};

	std::tuple<op_reg_t, bool, op_reg_t, bool, op_reg_t, bool> getSignedEew() {
		// see functionality in declaration of elem_sel_t
		xlen_reg_t sew = getIntVSew();
		bool o1_signed = BIT_SINGLE_P1(elem_sel, 0);
		bool o2_signed = BIT_SINGLE_P1(elem_sel, 1);
		bool vd_signed = BIT_SINGLE_P1(elem_sel, 2);
		xlen_reg_t o1_eew = o1_eew_overwrite ? o1_eew_overwrite : (BIT_SINGLE_P1(elem_sel, 3) ? sew << 1 : sew);
		xlen_reg_t o2_eew = o2_eew_overwrite ? o2_eew_overwrite : (BIT_SINGLE_P1(elem_sel, 4) ? sew << 1 : sew);
		xlen_reg_t vd_eew = vd_eew_overwrite ? vd_eew_overwrite : (BIT_SINGLE_P1(elem_sel, 5) ? sew << 1 : sew);

		return std::make_tuple(vd_eew, vd_signed, o2_eew, o2_signed, o1_eew, o1_signed);
	}

	op_reg_t clampSigned(op_reg_t elem, xlen_reg_t orig_elemWidth, xlen_reg_t dest_elemWidth) {
		s_op_reg_t elem_signed = signExtend(elem, orig_elemWidth);
		s_op_reg_t upper_bound;
		s_op_reg_t lower_bound;
		switch (dest_elemWidth) {
			case 8:
				upper_bound = INT8_MAX;
				lower_bound = INT8_MIN;
				break;
			case 16:
				upper_bound = INT16_MAX;
				lower_bound = INT16_MIN;
				break;
			case 32:
				upper_bound = INT32_MAX;
				lower_bound = INT32_MIN;
				break;
			case 64:
				upper_bound = INT64_MAX;
				lower_bound = INT64_MIN;
				break;
			default:
				v_assert(false);
		}
		s_op_reg_t clamped = std::clamp(elem_signed, lower_bound, upper_bound);
		iss.csrs.vxsat.fields.vxsat |= (clamped != elem_signed);
		return clamped;
	}

	void applyChecks() {
		auto [vd_eew, vd_signed, vs2_eew, vs2_signed, vs1_eew, vs1_signed] = getSignedEew();
		v_assert(vd_eew >= 8 && vd_eew <= 64 && vs2_eew >= 8 && vs2_eew <= 64 && vs1_eew >= 8 && vs1_eew <= 64,
		         "EEW out of range");

		double lmul = getVlmul();
		op_reg_t sew = getIntVSew();

		/* see spec 5.3 */
		if (require_vd_not_v0 && !iss.instr.vm()) {
			v_assert(iss.instr.rd() != 0, "vd: v0 not allowed for masked");
		}

		/* For the purpose of determining register group overlap constraints, mask elements have EEW=1. */
		if (vd_is_mask) {
			vd_eew = 1;
		}
		if (vs1_is_mask) {
			vs1_eew = 1;
		}
		if (vs2_is_mask) {
			vs2_eew = 1;
		}

		unsigned int vd = iss.instr.rd();
		double vd_emul = lmul * vd_eew / sew;

		int vop_start = 1;  // check only vs2
		unsigned int vop[2], vop_eew[2];
		vop[1] = iss.instr.rs2();
		double vop_emul[2]{0};
		vop_eew[1] = vs2_eew;
		vop_emul[1] = lmul * vs2_eew / sew;
		if (param_sel == param_sel_t::vv) {
			/* vs1 used */
			vop[0] = iss.instr.rs1();
			vop_emul[0] = lmul * vs1_eew / sew;
			vop_eew[0] = vs1_eew;
			vop_start = 0;
		}

		if (!vd_is_scalar)
			v_assert(vd_emul <= 8, "vd_emul > 8");
		v_assert(vop_emul[1] <= 8, "vs2_emul > 8");
		if (!vs1_is_scalar)
			v_assert(vop_emul[0] <= 8, "vs1_emul > 8");

		if (!ignoreOverlap) {
			for (int i = vop_start; i < 2; i++) {
				bool overlap_valid =
				    vreg_overlap_valid(vd, vd_emul, vd_eew, vop[i], vop_emul[i], vop_eew[i], require_no_overlap);
				if (i == 0) {
					v_assert(overlap_valid == true, "vd overlaps source vector vs1");
				} else {
					v_assert(overlap_valid == true, "vd overlaps source vector vs2");
				}
			}
		}

		if (!ignoreAlignment) {
			if (!vd_is_mask && !vd_is_scalar) {
				v_assert(v_is_aligned(vd, vd_emul), "vd is not aligned");
			}
			if (!vs2_is_mask) {
				v_assert(v_is_aligned(vop[1], vop_emul[1]), "vs2 is not aligned");
			}
			if (param_sel == param_sel_t::vv && !vs1_is_mask && !vs1_is_scalar) {
				v_assert(v_is_aligned(vop[0], vop_emul[0]), "vs1 is not aligned");
			}
		}
	}

	std::tuple<op_reg_t, op_reg_t, op_reg_t> getOperandsAll(op_reg_t i) {
		auto [op1, op2] = getOperands(i);

		auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();

		op_reg_t vd = getSewSingleOperand(vd_eew, iss.instr.rd(), i, false);

		return std::make_tuple(op1, op2, vd);
	}

	void writeSewSingleOperand(xlen_reg_t sew, xlen_reg_t addr, xlen_reg_t index, op_reg_t result) {
		switch (sew) {
			case 8: {
				reg_write<uint8_t>(addr, index, result);
			} break;
			case 16: {
				reg_write<uint16_t>(addr, index, result);
			} break;
			case 32: {
				reg_write<uint32_t>(addr, index, result);
			} break;
			case 64: {
				reg_write<uint64_t>(addr, index, result);
			} break;
		};

		xlen_reg_t num_elem_per_reg = VLEN / sew;
		xlen_reg_t vec_idx = addr + index / num_elem_per_reg;
		xlen_reg_t elem_num = index % num_elem_per_reg;
		xlen_reg_t elem_num_64 = elem_num / (64 / sew);
		getSewSingleOperand(64, vec_idx, elem_num_64, false);
	};

	void writeGeneric(op_reg_t result, unsigned i) {
		auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();

		writeSewSingleOperand(vd_eew, iss.instr.rd(), i, result);
	};

	xlen_reg_t getShiftWidth(load_store_type_t ldstType, xlen_reg_t numBits, xlen_reg_t i, xlen_reg_t field) {
		xlen_reg_t numBytes = numBits >> 3;
		switch (ldstType) {
			case load_store_type_t::masked:
				return i;
			case load_store_type_t::standard:
			case load_store_type_t::fofl:
			case load_store_type_t::whole:
				return (i * (iss.instr.nf() + 1) + field) * numBytes;
			case load_store_type_t::standard_reg:
				return i * iss_reg_read(iss.instr.rs2()) + field * numBytes;
			case load_store_type_t::indexed:
				return getSewSingleOperand(numBits, iss.instr.rs2(), i, false) + field * (getIntVSew() >> 3);
		}
		v_assert(false);
		return 0;
	}

	std::pair<xlen_reg_t, xlen_reg_t> vLoadReqs(load_store_type_t ldstType, bool isLoad, xlen_reg_t eew) {
		float emul;
		xlen_reg_t effective_mul_idx;
		bool is_masked_instr = ldstType == load_store_type_t::masked;
		if (ldstType != load_store_type_t::whole) {
			if (is_masked_instr) {
				effective_mul_idx = 1;
			} else {
				double lmul = getVlmul();
				emul = ((float)eew / getIntVSew()) * lmul;

				if (ldstType == load_store_type_t::indexed) {
					effective_mul_idx = lmul < 1 ? 1 : lmul;
				} else {
					effective_mul_idx = emul < 1 ? 1 : emul;
				}

				v_assert(emul >= 0.125 && emul <= 8, "Emul not in range " + std::to_string(emul));
				v_assert(effective_mul_idx * (iss.instr.nf() + 1) <= 8, "emul*nf not in range");
				v_assert((iss.instr.rd() + (iss.instr.nf() + 1) * effective_mul_idx) <= 32,
				         "Destination register out of range");
			}

		} else {
			effective_mul_idx = 1;
		}

		xlen_reg_t evl;
		if (is_masked_instr) {
			evl = std::ceil((float)iss.csrs.vl.reg / 8.0);
		} else if (ldstType == load_store_type_t::whole) {
			evl = VLEN / eew;
		} else {
			evl = iss.csrs.vl.reg;
		}

		return std::make_pair(effective_mul_idx, evl);
	}

	// TODO tail mask agnostic
	bool vInactiveHandling(xlen_reg_t i) {
		return vInactiveHandling(i, iss.csrs.vl.reg);
	}

	bool vInactiveHandling(xlen_reg_t i, xlen_reg_t evl) {
		if (i < iss.csrs.vstart.reg) {
			return true;
		}
		if (i >= evl) {
			return true;
		}
		if (iss.instr.vm() == 0) {
			auto [reg_idx, reg_pos] = getCarryElements(i);
			op_reg_t mask = (getSewSingleOperand(64, 0, reg_idx, false) >> reg_pos) & 1;
			if (mask == 0) {
				return true;
			}
		}
		return false;
	}

	std::pair<xlen_reg_t, xlen_reg_t> vIndices(xlen_reg_t i, load_store_type_t ldstType, xlen_reg_t field,
	                                           xlen_reg_t switchElem, op_reg_t effective_mul_idx) {
		xlen_reg_t num_elem_per_reg = VLEN / switchElem;
		xlen_reg_t vec_idx, elem_num;
		if (ldstType == load_store_type_t::whole) {
			xlen_reg_t curr_idx = i * (iss.instr.nf() + 1) + field;
			vec_idx = iss.instr.rd() + curr_idx / num_elem_per_reg;
			elem_num = curr_idx % num_elem_per_reg;

		} else {
			vec_idx = iss.instr.rd() + field * effective_mul_idx + i / num_elem_per_reg;
			elem_num = i % num_elem_per_reg;
		}

		return std::make_pair(vec_idx, elem_num);
	}

	void vLoadStore(load_store_t ldst, xlen_reg_t numBits, load_store_type_t ldstType) {
		auto [effective_mul_idx, evl] = vLoadReqs(ldstType, true, numBits);
		bool break_loop = false;
		xlen_reg_t switchElem = (ldstType == load_store_type_t::indexed) ? getIntVSew() : numBits;

		/* check overlap destination with mask */
		if (ldst == load_store_t::load && !iss.instr.vm()) {
			v_assert(iss.instr.rd() != 0, "rd: v0 not allowed for masked on load");
		}

		if (ldstType == load_store_type_t::indexed) {
			double lmul = getVlmul();

			/* check data vector reg alignement */
			unsigned int vd = iss.instr.rd();
			unsigned int vd_eew = getIntVSew();
			double vd_emul = lmul;
			v_assert(v_is_aligned(vd, vd_emul), "vd is not aligned");

			/* check index vector reg alignment */
			unsigned int vs2 = iss.instr.rs2();
			unsigned int vs2_eew = numBits;
			unsigned int sew = getIntVSew();
			double vs2_emul = lmul * vs2_eew / sew;
			v_assert(v_is_aligned(vs2, vs2_emul), "vs2 is not aligned");

			/* check overlap of index vector with data (groups(emul) and fields) vector on load */
			if (ldst == load_store_t::load) {
				for (xlen_reg_t field = 0; field < iss.instr.nf() + 1; field++) {
					unsigned int vdfield = vd + field * std::ceil(vd_emul);
					if (!vreg_overlap_valid(vdfield, vd_emul, vd_eew, vs2, vs2_emul, vs2_eew, iss.instr.nf() > 0)) {
						v_assert(false, "vd field overlaps source vector vs2");
					}
				}
			}

		} else if (ldstType != load_store_type_t::masked) {
			double lmul;
			unsigned int sew;
			if (ldstType == load_store_type_t::whole) {
				lmul = iss.instr.nf() + 1;
				sew = numBits;
			} else {
				lmul = getVlmul();
				sew = getIntVSew();
			}

			/* check data vector reg alignment */
			unsigned int vd = iss.instr.rd();
			unsigned int vd_eew = numBits;
			double vd_emul = lmul * vd_eew / sew;
			v_assert(v_is_aligned(vd, vd_emul), "vd is not aligned");
		}

		for (xlen_reg_t i = 0; i < evl; ++i) {
			bool is_inactive = vInactiveHandling(i, evl);
			if (!is_inactive) {
				iss.csrs.vstart.reg = i;
				for (xlen_reg_t field = 0; field < iss.instr.nf() + 1; field++) {
					xlen_reg_t addr =
					    iss_reg_read_unsigned(iss.instr.rs1()) + getShiftWidth(ldstType, numBits, i, field);
					auto [vec_idx, elem_num] = vIndices(i, ldstType, field, switchElem, effective_mul_idx);

					op_reg_t value;

					if (ldst == load_store_t::load) {
						switch (switchElem) {
							case 8:
								value = iss.mem->load_byte(addr);
								break;
							case 16:
								value = iss.mem->load_half(addr);
								break;
							case 32:
								value = iss.mem->load_word(addr);
								break;
							case 64:
								value = iss.mem->load_double(addr);
								break;
						}

						if (ldstType == load_store_type_t::fofl) {
							try {
								writeSewSingleOperand(switchElem, vec_idx, elem_num, value);
							} catch (SimulationTrap e) {
								if (i == 0) {
									throw e;
								} else {
									iss.csrs.vl.reg = i;
									break_loop = true;
									break;
								}
							}
						} else {
							writeSewSingleOperand(switchElem, vec_idx, elem_num, value);
						}

					} else {
						value = getSewSingleOperand(switchElem, vec_idx, elem_num, false);
						switch (switchElem) {
							case 8:
								iss.mem->store_byte(addr, value);
								break;
							case 16:
								iss.mem->store_half(addr, value);
								break;
							case 32:
								iss.mem->store_word(addr, value);
								break;
							case 64:
								iss.mem->store_double(addr, value);
								break;
						}
					}
				}
				if (break_loop) {
					break;
				}
			}
		}

		// if (!break_loop) {
		//      std::cout << "LOAD/STORE OK " << Opcode::mappingStr.at(iss.op) << std::endl;
		// }
	}

	void genericVLoop(std::function<void(xlen_reg_t)> func) {
		genericVLoop([=](xlen_reg_t i) { func(i); }, elem_sel_t::xxxuuu, param_sel_t::vv);
	}

	void genericVLoop(std::function<void(xlen_reg_t)> func, bool runAll) {
		genericVLoop([=](xlen_reg_t i) { func(i); }, elem_sel_t::xxxuuu, param_sel_t::vv, runAll);
	}

	void genericVLoop(std::function<void(xlen_reg_t)> f, elem_sel_t elem, param_sel_t param) {
		genericVLoop(f, elem, param, false);
	}

	void genericVLoop(std::function<void(xlen_reg_t)> f, elem_sel_t elem, param_sel_t param, bool ignore_inactive) {
		elem_sel = elem;
		param_sel = param;

		applyChecks();
		for (xlen_reg_t i = 0; i < iss.csrs.vl.reg; ++i) {
			bool is_inactive = ignore_inactive ? false : vInactiveHandling(i, iss.csrs.vl.reg);
			if (!is_inactive) {
				f(i);
			}
		}
		iss.csrs.vstart.reg = 0;
	}

	void vLoop(std::function<op_reg_t(op_reg_t, op_reg_t)> func, elem_sel_t elem, param_sel_t param) {
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2] = getOperands(i);
			    writeGeneric(func(op2, op1), i);
		    },
		    elem, param);
	}

	void vLoopVdExt(std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> func, elem_sel_t elem,
	                param_sel_t param) {
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2, vd] = getOperandsAll(i);
			    writeGeneric(func(op2, op1, vd, i), i);
		    },
		    elem, param);
	}

	void vLoopVdExtVoid(std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> func, elem_sel_t elem,
	                    param_sel_t param) {
		require_vd_not_v0 = false;
		vd_is_mask = true;
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2, vd] = getOperandsAll(i);
			    func(op2, op1, vd, i);
		    },
		    elem, param);
	}

	void vLoopVoid(std::function<void(xlen_reg_t)> func, param_sel_t param) {
		genericVLoop([=](xlen_reg_t i) { func(i); }, elem_sel_t::xxxsss, param);
	}

	void vLoopVoidNoOverlap(std::function<void(xlen_reg_t)> func, param_sel_t param) {
		require_no_overlap = true;
		vLoopVoid(func, param);
	}

	void vLoopVoid(std::function<void(xlen_reg_t)> func) {
		// TODO this version can probably be removed
		genericVLoop([=](xlen_reg_t i) { func(i); });
	}

	/* TODO: used for mask generation operations -> rename??? */
	void vLoopVoidAll(std::function<void(xlen_reg_t)> func) {
		vd_is_mask = true;
		genericVLoop([=](xlen_reg_t i) { func(i); }, true);
	}

	/* TODO: used for 15.1. Vector Mask-Register Logical Instructions -> rename??? */
	void vLoopVoidAllMask(std::function<void(xlen_reg_t)> func) {
		vd_is_mask = true;
		vs1_is_mask = true;
		vs2_is_mask = true;
		genericVLoop([=](xlen_reg_t i) { func(i); }, true);
	}

	void vLoopVoidAll(std::function<void(xlen_reg_t)> func, elem_sel_t elem, param_sel_t param) {
		require_vd_not_v0 = false;
		vd_is_mask = true;
		genericVLoop([=](xlen_reg_t i) { func(i); }, elem, param, true);
	}

	void vLoopExt(std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> func, elem_sel_t elem, param_sel_t param) {
		require_no_overlap = true;
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2] = getOperands(i);
			    writeGeneric(func(op2, op1, i), i);
		    },
		    elem, param);
	}

	void vLoopExtCarry(std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> func, elem_sel_t elem,
	                   param_sel_t param) {
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2] = getOperands(i);
			    writeGeneric(func(op2, op1, i), i);
		    },
		    elem, param, true);
	}

	void vLoopVdExtCarry(std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> func, elem_sel_t elem,
	                     param_sel_t param) {
		genericVLoop(
		    [=](xlen_reg_t i) {
			    auto [op1, op2, vd] = getOperandsAll(i);
			    writeGeneric(func(op2, op1, vd, i), i);
		    },
		    elem, param, true);
	}

	/* used for reduction instructions */
	void vLoopRed(std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> func, elem_sel_t elem,
	              param_sel_t param) {
		op_reg_t res = 0;
		bool added_first = false;
		require_vd_not_v0 = false;
		ignoreOverlap = true;
		vd_is_scalar = true;
		vs1_is_scalar = true;
		genericVLoop(
		    [=, &res, &added_first](xlen_reg_t i) {
			    auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			    auto [op1, op2] = getOperandsRed(0, i);
			    if (!added_first) {
				    res = op1_signed ? signExtend(op1, op1_eew) : op1;
				    added_first = true;
			    }

			    func(op2, op1, i, res);
		    },
		    elem, param);
		if (iss.csrs.vl.reg > 0) {
			if (!added_first) {
				auto [op1, op2] = getOperandsRed(0, 0);
				res = op1;
			}
			writeGeneric(res, 0);
		}
	}

	// Lambda Function Definitions
	std::function<op_reg_t(op_reg_t, op_reg_t)> vAdd() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			return vd_signed ? signExtend(op2, op2_eew) + signExtend(op1, op1_eew) : op2 + op1;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSub() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			return vd_signed ? signExtend(op2, op2_eew) - signExtend(op1, op1_eew) : op2 - op1;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vRSub() {
		return [](op_reg_t op2, op_reg_t op1) -> op_reg_t { return op1 - op2; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vAnd() {
		return [](op_reg_t op2, op_reg_t op1) -> op_reg_t { return op2 & op1; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vOr() {
		return [](op_reg_t op2, op_reg_t op1) -> op_reg_t { return op2 | op1; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vXor() {
		return [](op_reg_t op2, op_reg_t op1) -> op_reg_t { return op2 ^ op1; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vShift(bool shr) {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			xlen_reg_t shift_mask = getMask(iss.csrs.vtype.fields.vsew + 2 + op2_eew / vd_eew);
			xlen_reg_t shift_step = op1 & shift_mask;
			if (shr) {
				if (vd_signed) {
					return signExtend(op2, op2_eew) >> shift_step;
				}
				return op2 >> shift_step;
			} else {
				return op2 << shift_step;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vMin() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			bool comp = op2_signed ? signExtend(op2, op2_eew) < signExtend(op1, op1_eew) : op2 < op1;

			return comp ? op2 : op1;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vMax() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			bool comp = op2_signed ? signExtend(op2, op2_eew) > signExtend(op1, op1_eew) : op2 > op1;

			return comp ? op2 : op1;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vMul() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (op2_signed && op1_signed) {
				return multiply(signExtend(op2, op2_eew), signExtend(op1, op1_eew), 0, false).lower;
			} else if (op2_signed && !op1_signed) {
				return multiply(signExtend(op2, op2_eew), op1, 0, false).lower;
			} else if (!op2_signed && op1_signed) {
				return multiply(signExtend(op1, op1_eew), op2, 0, false).lower;
			} else {
				return multiply(op1, op2, 0, false).lower;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vMv() {
		return [](op_reg_t op2, op_reg_t op1) -> op_reg_t { return op1; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vDiv() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			xlen_reg_t sew = getIntVSew();
			if (!vd_signed) {
				return op1 == 0 ? -1 : op2 / op1;
			}
			// TODO necessary ?
			s_op_reg_t op1_s = signExtend(op1, op1_eew);
			s_op_reg_t op2_s = signExtend(op2, op2_eew);
			return op1_s == 0 ? -1 : (op2_s == (INT64_MIN >> (64 - sew)) && op1_s == -1 ? op2_s : op2_s / op1_s);
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vRem() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (!vd_signed) {
				return op1 == 0 ? op2 : op2 % op1;
			}
			s_op_reg_t op1_s = signExtend(op1, op1_eew);
			s_op_reg_t op2_s = signExtend(op2, op2_eew);
			return op1_s == 0 ? op2_s : op2_s % op1_s;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vAadd() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			const op_reg_t msb = (1ul << (vd_eew - 1));

			op_reg_t res = (op2 + op1) & getMask(vd_eew);

			bool op1_msb_set = op1 & msb;
			bool op2_msb_set = op2 & msb;
			bool res_msb_set = res & msb;

			op_reg_t shiftResult = vd_signed ? vRound(signExtend(res, vd_eew), 1) : vRound(res, 1);

			if (vd_signed &&
			    ((op1_msb_set && op2_msb_set && !res_msb_set) || (!op1_msb_set && !op2_msb_set && res_msb_set))) {
				shiftResult ^= msb;
			} else if (!vd_signed && ((op1_msb_set && op2_msb_set) || (!op1_msb_set && op2_msb_set && !res_msb_set) ||
			                          ((op1_msb_set && !op2_msb_set && !res_msb_set)))) {
				shiftResult |= msb;
			}

			return shiftResult;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vAsub() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			const op_reg_t msb = (1ul << (vd_eew - 1));

			op_reg_t res = (op2 - op1) & getMask(vd_eew);

			bool op1_msb_set = op1 & msb;
			bool op2_msb_set = op2 & msb;
			bool res_msb_set = res & msb;

			op_reg_t shiftResult = vd_signed ? vRound(signExtend(res, vd_eew), 1) : vRound(res, 1);

			if (vd_signed &&
			    ((!op1_msb_set && op2_msb_set && !res_msb_set) || (op1_msb_set && !op2_msb_set && res_msb_set))) {
				shiftResult ^= msb;
			} else if (!vd_signed &&
			           ((!op1_msb_set && !op2_msb_set && res_msb_set) || (op1_msb_set && !op2_msb_set && res_msb_set) ||
			            (op1_msb_set && !op2_msb_set && !res_msb_set) || (op1_msb_set && op2_msb_set && res_msb_set))) {
				shiftResult ^= msb;
			}

			return shiftResult;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSmul() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (((op2 & getMask(op2_eew)) == 1ul << (op2_eew - 1)) &&
			    ((op1 & getMask(op1_eew)) == 1ul << (op1_eew - 1))) {
				iss.csrs.vxsat.fields.vxsat |= true;
				return getMask(op2_eew - 1);
			}

			multiplicationResult res = multiply(signExtend(op2, op2_eew), signExtend(op1, op1_eew), vd_eew - 1, true);

			return res.lower;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vShiftRight(bool clip_result) {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			xlen_reg_t shift_mask = getMask(iss.csrs.vtype.fields.vsew + 2 + op2_eew / vd_eew);
			op_reg_t result;
			if (vd_signed) {
				result = vRound(signExtend(op2, op2_eew), op1 & shift_mask);
			} else {
				result = vRound(op2, op1 & shift_mask);
			}

			if (clip_result) {
				if (!vd_signed) {
					if (result > getMask(vd_eew)) {
						result = getMask(vd_eew);
						iss.csrs.vxsat.fields.vxsat |= true;
					}
				} else {
					result = clampSigned(result, op2_eew, vd_eew);
				}
			}

			return result;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> vAdc() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i) -> op_reg_t { return op2 + op1 + vCarry(i); };
	}

	std::function<void(xlen_reg_t)> vMadc() {
		return [=](xlen_reg_t i) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();

			const op_reg_t msb = (1ul << (vd_eew - 1));
			auto [reg_idx, reg_pos] = getCarryElements(i);
			auto [op1, op2] = getOperands(i);

			op_reg_t res = signExtend(op2, op2_eew) + signExtend(op1, op1_eew);
			bool op1_msb_set = op1 & msb;
			bool op2_msb_set = op2 & msb;
			bool res_msb_set = res & msb;

			op_reg_t has_overflown = (op1_msb_set && op2_msb_set) || (op1_msb_set && !op2_msb_set && !res_msb_set) ||
			                         (!op1_msb_set && op2_msb_set && !res_msb_set);

			op_reg_t carry = vCarry(i);
			has_overflown |= carry && res == getMask(64);

			op_reg_t vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, has_overflown, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> vSbc() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i) -> op_reg_t { return op2 - op1 - vCarry(i); };
	}

	std::function<void(xlen_reg_t)> vMsbc() {
		return [=](xlen_reg_t i) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			const op_reg_t msb = (1ul << (vd_eew - 1));
			auto [reg_idx, reg_pos] = getCarryElements(i);
			auto [op1, op2] = getOperands(i);

			op_reg_t res = signExtend(op2, op2_eew) - signExtend(op1, op1_eew);
			bool op1_msb_set = op1 & msb;
			bool op2_msb_set = op2 & msb;
			bool res_msb_set = res & msb;

			op_reg_t has_overflown = (op1_msb_set && !op2_msb_set) || (!op1_msb_set && !op2_msb_set && res_msb_set) ||
			                         (op1_msb_set && op2_msb_set && res_msb_set);
			op_reg_t carry = vCarry(i);
			has_overflown |= carry && res == 0;

			op_reg_t vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, has_overflown, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> vMerge() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i) -> op_reg_t { return vCarry(i) ? op1 : op2; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMacc() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (op2_signed && op1_signed) {
				return signExtend(op2, op2_eew) * signExtend(op1, op1_eew) + signExtend(vd, vd_eew);
			} else if (!op2_signed && !op1_signed) {
				return op2 * op1 + vd;
			} else if (op2_signed && !op1_signed) {
				return signExtend(op2, op2_eew) * op1 + vd;
			} else {
				return op2 * signExtend(op1, op1_eew) + vd;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vNmsac() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t { return -(op2 * op1) + vd; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMadd() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t { return (op1 * vd) + op2; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vNmsub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t { return -(op1 * vd) + op2; };
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vMulh() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();

			if (op2_signed && op1_signed) {
				return multiply(signExtend(op2, op2_eew), signExtend(op1, op1_eew), op2_eew, false).lower;
			} else if (!op2_signed && !op1_signed) {
				return multiply(op2, op1, op2_eew, false).lower;
			} else {
				return multiply(signExtend(op2, op2_eew), op1, op2_eew, false).lower;
			}
		};
	}

	enum int_compare_t { eq, ne, lt, le, gt };
	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vCompInt(int_compare_t type) {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			op_reg_t elem_pos = i / ELEN;
			op_reg_t vd_mask = getSewSingleOperand(ELEN, iss.instr.rd(), elem_pos, false);
			op1 &= getMask(op1_eew);
			bool comp;
			switch (type) {
				case int_compare_t::eq:
					comp = op2 == op1;
					break;
				case int_compare_t::ne:
					comp = op2 != op1;
					break;
				case int_compare_t::lt:
					comp = op2_signed ? signExtend(op2, op2_eew) < signExtend(op1, op1_eew) : op2 < op1;
					break;
				case int_compare_t::le:
					comp = op2_signed ? signExtend(op2, op2_eew) <= signExtend(op1, op1_eew) : op2 <= op1;
					break;
				case int_compare_t::gt:
					comp = op2_signed ? signExtend(op2, op2_eew) > signExtend(op1, op1_eew) : op2 > op1;
					break;
				default:
					v_assert(false, "invalid compare type");
			}
			op_reg_t vd_out = setSingleBitUnmasked<op_reg_t>(vd_mask, comp, i % ELEN);
			writeSewSingleOperand(ELEN, iss.instr.rd(), elem_pos, vd_out);
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedSum() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			res = vd_signed ? signExtend(op2, op2_eew) + signExtend(res, vd_eew) : op2 + res;
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedMax() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (vd_signed) {
				res = (signExtend(op2, op2_eew) >= signExtend(res, vd_eew)) ? op2 : res;
			} else {
				res = (op2 >= res) ? op2 : res;
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedMin() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (vd_signed) {
				res = (signExtend(op2, op2_eew) <= signExtend(res, vd_eew)) ? op2 : res;
			} else {
				res = (op2 <= res) ? op2 : res;
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedAnd() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void { res &= op2; };
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedOr() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void { res |= op2; };
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vRedXor() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void { res ^= op2; };
	}

	struct multiplicationResult {
		uint64_t upper;
		uint64_t lower;
	};

	template <typename T, typename U>
	multiplicationResult multiply(T op2, U op1, uint64_t shift, bool round) {
		multiplicationResult result;
		uint64_t aHigh = static_cast<uint64_t>(op2 >> 32);
		uint64_t aLow = static_cast<uint64_t>(op2 & 0xFFFFFFFF);
		uint64_t bHigh = static_cast<uint64_t>(op1 >> 32);
		uint64_t bLow = static_cast<uint64_t>(op1 & 0xFFFFFFFF);

		uint64_t t0 = aLow * bLow;
		uint64_t t1 = aLow * bHigh;
		uint64_t t2 = aHigh * bLow;
		uint64_t t3 = aHigh * bHigh;

		uint64_t carry = (t0 >> 32) + (t1 & 0xFFFFFFFF) + (t2 & 0xFFFFFFFF);

		uint64_t carry_other;
		if (std::is_signed_v<decltype(op2)> && std::is_signed_v<decltype(op1)>) {
			carry_other = (signExtend(t1, 64) >> 32) + (signExtend(t2, 64) >> 32) + (signExtend(carry, 64) >> 32);
		} else if (std::is_signed_v<decltype(op2)> && std::is_unsigned_v<decltype(op1)>) {
			carry_other = (t1 >> 32) + (signExtend(t2, 64) >> 32) + (signExtend(carry, 64) >> 32);
		} else {
			carry_other = (t1 >> 32) + (t2 >> 32) + (carry >> 32);
		}

		uint64_t upper = t3 + (t1 >> 32) + (t2 >> 32) + (carry >> 32);
		upper = t3 + carry_other;

		uint64_t lower = (carry << 32) | (t0 & 0xFFFFFFFF);

		result.upper = upper >> shift;
		uint64_t shiftOut = (upper & getMask(shift)) << (64 - shift);
		uint64_t lower_part = shift == 64 ? 0 : (lower >> shift);
		result.lower = lower_part | shiftOut;
		if (round) {
			result.lower += getRoundingIncrement(lower, shift);
		}

		return result;
	}

	std::pair<s_op_reg_t, bool> add_saturate(op_reg_t op2, op_reg_t op1, int bitWidth) {
		op_reg_t maxVal = (1l << (bitWidth - 1)) - 1;
		op_reg_t minVal = signExtend(1l << (bitWidth - 1), bitWidth);
		const op_reg_t msb = (1ul << (bitWidth - 1));

		op_reg_t sum = (op2 + op1) & getMask(bitWidth);

		bool op1_msb_set = op1 & msb;
		bool op2_msb_set = op2 & msb;
		bool res_msb_set = sum & msb;

		if (op1_msb_set && op2_msb_set && !res_msb_set) {
			return std::make_pair(minVal, true);
		} else if (!op1_msb_set && !op2_msb_set && res_msb_set) {
			return std::make_pair(maxVal, true);
		}

		return std::make_pair(sum, false);
	}

	std::pair<op_reg_t, bool> addu_saturate(op_reg_t op2, op_reg_t op1) {
		op_reg_t res = (op2 + op1) & getMask(getIntVSew());
		bool sat = res < (op1 & getMask(getIntVSew()));
		res |= -(sat);
		return std::make_pair(res, sat);
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSadd() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			auto [res, sat] = add_saturate(op2, op1, vd_eew);
			iss.csrs.vxsat.fields.vxsat |= sat;
			return res;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSaddu() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [res, sat] = addu_saturate(op2, op1);
			iss.csrs.vxsat.fields.vxsat |= sat;
			return res;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSsub() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			op_reg_t maxVal = (1l << (op2_eew - 1)) - 1;
			op_reg_t minVal = signExtend(1l << (op2_eew - 1), op2_eew);
			const op_reg_t msb = (1ul << (vd_eew - 1));

			op_reg_t res = (op2 - op1) & getMask(vd_eew);

			bool op1_msb_set = op1 & msb;
			bool op2_msb_set = op2 & msb;
			bool res_msb_set = res & msb;
			bool sat = false;

			if (op2_msb_set && !op1_msb_set && !res_msb_set) {
				res = minVal;
				sat = true;
			} else if (!op2_msb_set && op1_msb_set && res_msb_set) {
				res = maxVal;
				sat = true;
			}
			iss.csrs.vxsat.fields.vxsat |= sat;

			return res;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vSsubu() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			op_reg_t res = op2 - op1;
			bool sat = (res & getMask(getIntVSew())) <= op2;
			res &= -(sat);
			iss.csrs.vxsat.fields.vxsat |= (!sat);
			return res;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vExt(xlen_reg_t division) {
		o2_eew_overwrite = getIntVSew() / division;
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (op2_signed) {
				return signExtend(op2, op2_eew);
			}
			return op2;
		};
	}

	enum maskOperation { m_and, m_nand, m_andn, m_or, m_xor, m_nor, m_orn, m_xnor };
	std::function<void(xlen_reg_t)> vMask(maskOperation op) {
		return [=](xlen_reg_t i) -> void {
			auto [reg_idx, reg_pos] = getCarryElements(i);

			op_reg_t vs1 = (getSewSingleOperand(64, iss.instr.rs1(), reg_idx, false) >> reg_pos) & 1;
			op_reg_t vs2 = (getSewSingleOperand(64, iss.instr.rs2(), reg_idx, false) >> reg_pos) & 1;

			op_reg_t res;
			switch (op) {
				case maskOperation::m_and:
					res = vs2 & vs1;
					break;
				case maskOperation::m_nand:
					res = ~(vs2 & vs1);
					break;
				case maskOperation::m_andn:
					res = vs2 & ~vs1;
					break;
				case maskOperation::m_or:
					res = vs2 | vs1;
					break;
				case maskOperation::m_xor:
					res = vs2 ^ vs1;
					break;
				case maskOperation::m_nor:
					res = ~(vs2 | vs1);
					break;
				case maskOperation::m_orn:
					res = vs2 | ~vs1;
					break;
				case maskOperation::m_xnor:
					res = vs2 ^ ~vs1;
					break;
				default:
					v_assert(false, "invalid vMask operation");
			}
			res &= 0b1;

			op_reg_t vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);
			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	void vCpop() {
		require_vd_not_v0 = false;
		op_reg_t count = 0;
		ignoreAlignment = true;
		genericVLoop([=, &count](xlen_reg_t i) {
			auto [reg_idx, reg_pos] = getCarryElements(i);

			op_reg_t mask = (getSewSingleOperand(64, iss.instr.rs2(), reg_idx, false) >> reg_pos) & 1;
			count += mask;
		});
		iss_reg_write(iss.instr.rd(), count);
	}

	void vFirst() {
		require_vd_not_v0 = false;
		ignoreAlignment = true;
		op_reg_t initial_position = -1;
		op_reg_t position = initial_position;
		genericVLoop([=, &position](xlen_reg_t i) {
			auto [reg_idx, reg_pos] = getCarryElements(i);

			op_reg_t mask = (getSewSingleOperand(64, iss.instr.rs2(), reg_idx, false) >> reg_pos) & 1;
			if (position == initial_position && mask) {
				position = i;
			}
		});
		iss_reg_write(iss.instr.rd(), position);
	}

	enum vms_type_t { sbf, sif, sof };
	void vMs(vms_type_t type) {
		bool hit = false;
		ignoreAlignment = true;

		v_assert(iss.instr.rd() != iss.instr.rs2(), "vd overlaps source vector vs2");

		genericVLoop([=, &hit](xlen_reg_t i) {
			auto [reg_idx, reg_pos] = getCarryElements(i);

			bool mask_elem = (getSewSingleOperand(64, iss.instr.rs2(), reg_idx, false) >> reg_pos) & 1;

			op_reg_t vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			op_reg_t result = 0;
			if (!hit) {
				switch (type) {
					case vms_type_t::sof:
						if (mask_elem) {
							hit = true;
							result = 1;
						}
						break;
					case vms_type_t::sif:
						result = 1;
						if (mask_elem) {
							hit = true;
						}
						break;
					case vms_type_t::sbf:
						if (!mask_elem) {
							result = 1;
						} else {
							hit = true;
						}
						break;
				}
			}
			vd = setSingleBitUnmasked(vd, result, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		});
	}

	void vIota() {
		op_reg_t count = 0;
		require_no_overlap = true;
		vs2_is_mask = true;
		genericVLoop(
		    [=, &count](xlen_reg_t i) {
			    auto [reg_idx, reg_pos] = getCarryElements(i);

			    bool hit = (getSewSingleOperand(64, iss.instr.rs2(), reg_idx, false) >> reg_pos) & 1;

			    writeGeneric(count, i);

			    if (hit) {
				    count++;
			    }
		    },
		    elem_sel_t::xxxuuu, param_sel_t::v);
	}

	std::function<void(xlen_reg_t)> vId() {
		return [=](xlen_reg_t index) -> void {
			elem_sel = elem_sel_t::xxxuuu;
			writeGeneric(index, index);
		};
	}

	void vMvXs() {
		op_reg_t res = getSewSingleOperand(getIntVSew(), iss.instr.rs2(), 0, false);
		res = signExtend(res, getIntVSew());
		iss_reg_write(iss.instr.rd(), res);
	}

	void vMvSx() {
		if (iss.csrs.vstart.reg < iss.csrs.vl.reg) {
			elem_sel = elem_sel_t::xxxuuu;
			op_reg_t res = signExtend(iss_reg_read(iss.instr.rs1()), iss.xlen);
			writeGeneric(res, 0);
		}
	}

	std::function<void(xlen_reg_t)> vSlideUp(xlen_reg_t offset) {
		return [=](xlen_reg_t index) -> void {
			if (iss.csrs.vstart.reg < offset && index < offset) {
				return;
			}
			elem_sel = elem_sel_t::xxxsss;

			op_reg_t res = getSewSingleOperand(getIntVSew(), iss.instr.rs2(), index - offset, false);
			writeGeneric(res, index);
		};
	}

	std::function<void(xlen_reg_t)> vSlideDown(xlen_reg_t offset) {
		return [=](xlen_reg_t index) -> void {
			elem_sel = elem_sel_t::xxxsss;

			double lmul = getVlmul();

			xlen_reg_t vlmax = lmul * VLEN / getIntVSew();
			bool is_zero = (index + offset) >= vlmax || offset & ((uint64_t)1 << 63);

			op_reg_t res = is_zero ? 0 : getSewSingleOperand(getIntVSew(), iss.instr.rs2(), index + offset, false);
			writeGeneric(res, index);
		};
	}

	std::function<void(xlen_reg_t)> vSlide1Up(param_sel_t param) {
		return [=](xlen_reg_t index) -> void {
			op_reg_t sew = getIntVSew();
			if (index != 0) {
				elem_sel = elem_sel_t::xxxsss;

				op_reg_t res = getSewSingleOperand(sew, iss.instr.rs2(), index - 1, false);
				writeGeneric(res, index);
			} else {
				elem_sel = elem_sel_t::xxxuuu;

				op_reg_t result =
				    param == param_sel_t::vx ? iss_reg_read(iss.instr.rs1()) : fp_reg_read(iss.instr.rs1(), sew);
				writeGeneric(result, index);
			}
		};
	}
	std::function<void(xlen_reg_t)> vSlide1Down(param_sel_t param) {
		return [=](xlen_reg_t index) -> void {
			op_reg_t sew = getIntVSew();
			if (index != (iss.csrs.vl.reg - 1)) {
				elem_sel = elem_sel_t::xxxsss;

				op_reg_t res = getSewSingleOperand(sew, iss.instr.rs2(), index + 1, false);
				writeGeneric(res, index);
			} else {
				elem_sel = elem_sel_t::xxxuuu;

				op_reg_t result =
				    param == param_sel_t::vx ? iss_reg_read(iss.instr.rs1()) : fp_reg_read(iss.instr.rs1(), sew);
				writeGeneric(result, index);
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, xlen_reg_t)> vGather(bool isGather16) {
		if (isGather16) {
			o1_eew_overwrite = 16;
		}
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i) -> op_reg_t {
			if (param_sel == param_sel_t::vv) {
				// TODO implememtn overlapping constraint
				v_assert(iss.instr.rd() != iss.instr.rs1() && iss.instr.rd() != iss.instr.rs2(),
				         "Overlapping not allowed");
			}

			double lmul = getVlmul();

			xlen_reg_t vlmax = lmul * VLEN / getIntVSew();
			uint64_t rs1_value = param_sel == param_sel_t::vx ? iss_reg_read(iss.instr.rs1()) : op1;
			if (rs1_value >= vlmax) {
				return 0;
			}

			return getSewSingleOperand(getIntVSew(), iss.instr.rs2(), op1, false);
		};
	}

	void vCompress() {
		op_reg_t current_position = 0;
		require_no_overlap = true;
		vs1_is_mask = true;
		genericVLoop([=, &current_position](xlen_reg_t i) {
			elem_sel = elem_sel_t::xxxuuu;

			auto [reg_idx, reg_pos] = getCarryElements(i);
			bool mask_elem = ((reg_read<op_reg_t>(iss.instr.rs1(), reg_idx) >> reg_pos) & 1) == 1;
			if (!mask_elem) {
				return;
			}

			op_reg_t res = getSewSingleOperand(getIntVSew(), iss.instr.rs2(), i, false);
			writeGeneric(res, current_position);
			current_position++;
		});
	}

	void vMvNr() {
		xlen_reg_t nreg = iss.instr.rs1() + 1;
		xlen_reg_t start = iss.csrs.vstart.reg;
		xlen_reg_t sew = getIntVSew();
		xlen_reg_t evl = nreg * VLEN / sew;

		/* check, if registers are aligned */
		v_assert(v_is_aligned(iss.instr.rd(), nreg), "rd is not aligned");
		v_assert(v_is_aligned(iss.instr.rs2(), nreg), "vs2 is not aligned");

		if (iss.instr.rd() != iss.instr.rs2()) {
			for (xlen_reg_t idx = start; idx < evl; idx++) {
				xlen_reg_t reg = idx / (VLEN / sew);
				xlen_reg_t elem = idx % (VLEN / sew);
				op_reg_t res = getSewSingleOperand(sew, iss.instr.rs2() + reg, elem, false);

				writeSewSingleOperand(sew, iss.instr.rd() + reg, elem, res);
			}
		}
	}
	// Further functions
	std::pair<op_reg_t, op_reg_t> getCarryElements(xlen_reg_t index) {
		op_reg_t reg_idx = index / ELEN;
		op_reg_t reg_pos = index % ELEN;
		return std::make_pair(reg_idx, reg_pos);
	}

	op_reg_t vCarry(xlen_reg_t index) {
		auto [reg_idx, reg_pos] = getCarryElements(index);
		op_reg_t carry = (getSewSingleOperand(64, 0, reg_idx, false) >> reg_pos) & 1;
		if (iss.instr.vm() == 1) {
			carry = 0;
		}
		return carry;
	}

	// Floating point instructions

	float16_t f16(op_reg_t value) {
		float16_t cast_f16{(uint16_t)value};
		return cast_f16;
	}

	float32_t f32(op_reg_t value) {
		float32_t cast_f32{(uint32_t)value};
		return cast_f32;
	}

	float64_t f64(op_reg_t value) {
		float64_t cast_f64{(uint64_t)value};
		return cast_f64;
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfAdd() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_add(f16(op2), f16(op1)).v;
				case 32:
					return f32_add(f32(op2), f32(op1)).v;
				case 64:
					return f64_add(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwAdd() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_add(f16_to_f32(f16(op2)), f16_to_f32(f16(op1))).v;
				case 32:
					return f64_add(f32_to_f64(f32(op2)), f32_to_f64(f32(op1))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwAddw() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_add(f32(op2), f16_to_f32(f16(op1))).v;
				case 32:
					return f64_add(f64(op2), f32_to_f64(f32(op1))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfSub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sub(f16(op2), f16(op1)).v;
				case 32:
					return f32_sub(f32(op2), f32(op1)).v;
				case 64:
					return f64_sub(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwSub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_sub(f16_to_f32(f16(op2)), f16_to_f32(f16(op1))).v;
				case 32:
					return f64_sub(f32_to_f64(f32(op2)), f32_to_f64(f32(op1))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwSubw() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_sub(f32(op2), f16_to_f32(f16(op1))).v;
				case 32:
					return f64_sub(f64(op2), f32_to_f64(f32(op1))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfrSub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sub(f16(op1), f16(op2)).v;
				case 32:
					return f32_sub(f32(op1), f32(op2)).v;
				case 64:
					return f64_sub(f64(op1), f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMul() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mul(f16(op2), f16(op1)).v;
				case 32:
					return f32_mul(f32(op2), f32(op1)).v;
				case 64:
					return f64_mul(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwMul() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (op2_eew) {
				case 16:
					return f32_mul(f16_to_f32(f16(op2)), f16_to_f32(f16(op1))).v;
				case 32:
					return f64_mul(f32_to_f64(f32(op2)), f32_to_f64(f32(op1))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfDiv() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_div(f16(op2), f16(op1)).v;
				case 32:
					return f32_div(f32(op2), f32(op1)).v;
				case 64:
					return f64_div(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfrDiv() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_div(f16(op1), f16(op2)).v;
				case 32:
					return f32_div(f32(op1), f32(op2)).v;
				case 64:
					return f64_div(f64(op1), f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMacc() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op2), f16(op1), f16(vd)).v;
				case 32:
					return f32_mulAdd(f32(op2), f32(op1), f32(vd)).v;
				case 64:
					return f64_mulAdd(f64(op2), f64(op1), f64(vd)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwMacc() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_mulAdd(f16_to_f32(f16(op2)), f16_to_f32(f16(op1)), f32(vd)).v;
				case 32:
					return f64_mulAdd(f32_to_f64(f32(op2)), f32_to_f64(f32(op1)), f64(vd)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfNmacc() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op2), f16_neg(f16(op1)), f16_neg(f16(vd))).v;
				case 32:
					return f32_mulAdd(f32(op2), f32_neg(f32(op1)), f32_neg(f32(vd))).v;
				case 64:
					return f64_mulAdd(f64(op2), f64_neg(f64(op1)), f64_neg(f64(vd))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwNmacc() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_mulAdd(f16_to_f32(f16(op2)), f16_to_f32(f16_neg(f16(op1))), f32_neg(f32(vd))).v;
				case 32:
					return f64_mulAdd(f32_to_f64(f32(op2)), f32_to_f64(f32_neg(f32(op1))), f64_neg(f64(vd))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMsac() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op2), f16(op1), f16_neg(f16(vd))).v;
				case 32:
					return f32_mulAdd(f32(op2), f32(op1), f32_neg(f32(vd))).v;
				case 64:
					return f64_mulAdd(f64(op2), f64(op1), f64_neg(f64(vd))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwMsac() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_mulAdd(f16_to_f32(f16(op2)), f16_to_f32(f16(op1)), f32_neg(f32(vd))).v;
				case 32:
					return f64_mulAdd(f32_to_f64(f32(op2)), f32_to_f64(f32(op1)), f64_neg(f64(vd))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfNmsac() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op2), f16_neg(f16(op1)), f16(vd)).v;
				case 32:
					return f32_mulAdd(f32(op2), f32_neg(f32(op1)), f32(vd)).v;
				case 64:
					return f64_mulAdd(f64(op2), f64_neg(f64(op1)), f64(vd)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfwNmsac() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f32_mulAdd(f16_to_f32(f16(op2)), f16_to_f32(f16_neg(f16(op1))), f32(vd)).v;
				case 32:
					return f64_mulAdd(f32_to_f64(f32(op2)), f32_to_f64(f32_neg(f32(op1))), f64(vd)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMadd() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op1), f16(vd), f16(op2)).v;
				case 32:
					return f32_mulAdd(f32(op1), f32(vd), f32(op2)).v;
				case 64:
					return f64_mulAdd(f64(op1), f64(vd), f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfNmadd() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16_neg(f16(op1)), f16(vd), f16_neg(f16(op2))).v;
				case 32:
					return f32_mulAdd(f32_neg(f32(op1)), f32(vd), f32_neg(f32(op2))).v;
				case 64:
					return f64_mulAdd(f64_neg(f64(op1)), f64(vd), f64_neg(f64(op2))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMsub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16(op1), f16(vd), f16_neg(f16(op2))).v;
				case 32:
					return f32_mulAdd(f32(op1), f32(vd), f32_neg(f32(op2))).v;
				case 64:
					return f64_mulAdd(f64(op1), f64(vd), f64_neg(f64(op2))).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfNmsub() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_mulAdd(f16_neg(f16(op1)), f16(vd), f16(op2)).v;
				case 32:
					return f32_mulAdd(f32_neg(f32(op1)), f32(vd), f32(op2)).v;
				case 64:
					return f64_mulAdd(f64_neg(f64(op1)), f64(vd), f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfSqrt() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sqrt(f16(op2)).v;
				case 32:
					return f32_sqrt(f32(op2)).v;
				case 64:
					return f64_sqrt(f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMin() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_min(f16(op2), f16(op1)).v;
				case 32:
					return f32_min(f32(op2), f32(op1)).v;
				case 64:
					return f64_min(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfMax() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_max(f16(op2), f16(op1)).v;
				case 32:
					return f32_max(f32(op2), f32(op1)).v;
				case 64:
					return f64_max(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfRsqrt7() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_rsqrte7(f16(op2)).v;
				case 32:
					return f32_rsqrte7(f32(op2)).v;
				case 64:
					return f64_rsqrte7(f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfFrec7() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_recip7(f16(op2)).v;
				case 32:
					return f32_recip7(f32(op2)).v;
				case 64:
					return f64_recip7(f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfSgnj() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sgnj(f16(op2), f16(op1)).v;
				case 32:
					return f32_sgnj(f32(op2), f32(op1)).v;
				case 64:
					return f64_sgnj(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfSgnjn() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sgnjn(f16(op2), f16(op1)).v;
				case 32:
					return f32_sgnjn(f32(op2), f32(op1)).v;
				case 64:
					return f64_sgnjn(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfSgnjx() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_sgnjx(f16(op2), f16(op1)).v;
				case 32:
					return f32_sgnjx(f32(op2), f32(op1)).v;
				case 64:
					return f64_sgnjx(f64(op2), f64(op1)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vfMv() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16(op1).v;
				case 32:
					return f32(op1).v;
				case 64:
					return f64(op1).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMfeq() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = f16_eq(f16(op2), f16(op1));
					break;
				case 32:
					res = f32_eq(f32(op2), f32(op1));
					break;
				case 64:
					res = f64_eq(f64(op2), f64(op1));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);
			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMfneq() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = !f16_eq(f16(op2), f16(op1));
					break;
				case 32:
					res = !f32_eq(f32(op2), f32(op1));
					break;
				case 64:
					res = !f64_eq(f64(op2), f64(op1));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMflt() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = f16_lt(f16(op2), f16(op1));
					break;
				case 32:
					res = f32_lt(f32(op2), f32(op1));
					break;
				case 64:
					res = f64_lt(f64(op2), f64(op1));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMfle() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = f16_le(f16(op2), f16(op1));
					break;
				case 32:
					res = f32_le(f32(op2), f32(op1));
					break;
				case 64:
					res = f64_le(f64(op2), f64(op1));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMfgt() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = f16_lt(f16(op1), f16(op2));
					break;
				case 32:
					res = f32_lt(f32(op1), f32(op2));
					break;
				case 64:
					res = f64_lt(f64(op1), f64(op2));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<void(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vMfge() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> void {
			op_reg_t res;
			switch (getIntVSew()) {
				case 16:
					res = f16_le(f16(op1), f16(op2));
					break;
				case 32:
					res = f32_le(f32(op1), f32(op2));
					break;
				case 64:
					res = f64_le(f64(op1), f64(op2));
					break;
				default:
					v_assert(false);
			}
			auto [reg_idx, reg_pos] = getCarryElements(i);
			vd = getSewSingleOperand(64, iss.instr.rd(), reg_idx, false);

			vd = setSingleBitUnmasked(vd, res, reg_pos);
			writeSewSingleOperand(ELEN, iss.instr.rd(), reg_idx, vd);
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfClass() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_classify(f16(op2));
				case 32:
					return f32_classify(f32(op2));
				case 64:
					return f64_classify(f64(op2));
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	void vMvFs() {
		xlen_reg_t sew = getIntVSew();
		op_reg_t val = getSewSingleOperand(sew, iss.instr.rs2(), 0, false);
		fp_reg_write(iss.instr.rd(), val, sew);
	}

	void vMvSf() {
		if (iss.csrs.vstart.reg < iss.csrs.vl.reg) {
			xlen_reg_t sew = getIntVSew();
			writeSewSingleOperand(sew, iss.instr.rd(), 0, fp_reg_read(iss.instr.rs1(), sew));
		}
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfCvtXF(bool rtz) {
		uint_fast8_t roundMode = rtz ? (uint_fast8_t)softfloat_round_minMag : softfloat_roundingMode;
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (getIntVSew()) {
				case 16:
					return vd_signed ? f16_to_i16(f16(op2), roundMode, true) : f16_to_ui16(f16(op2), roundMode, true);
				case 32:
					return vd_signed ? f32_to_i32(f32(op2), roundMode, true) : f32_to_ui32(f32(op2), roundMode, true);
				case 64:
					return vd_signed ? f64_to_i64(f64(op2), roundMode, true) : f64_to_ui64(f64(op2), roundMode, true);
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t)> vfCvtFX() {
		return [=](op_reg_t op2, op_reg_t op1) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			if (vd_eew >= op2_eew) {
				switch (vd_eew) {
					case 16:
						return vd_signed ? i32_to_f16(signExtend(op2, op2_eew)).v : ui32_to_f16(op2).v;
					case 32:
						return vd_signed ? i32_to_f32(signExtend(op2, op2_eew)).v : ui32_to_f32(op2).v;
					case 64:
						return vd_signed ? i64_to_f64(signExtend(op2, op2_eew)).v : ui64_to_f64(op2).v;
				}
			} else {
				switch (vd_eew) {
					case 16:
						return vd_signed ? i32_to_f16(op2).v : ui32_to_f16(op2).v;
					case 32:
						return vd_signed ? i64_to_f32(op2).v : ui64_to_f32(op2).v;
				}
			}
			v_assert(false);
			return 0;
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfCvtwXF(bool rtz) {
		uint_fast8_t roundMode = rtz ? (uint_fast8_t)softfloat_round_minMag : softfloat_roundingMode;
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (getIntVSew()) {
				case 16:
					return vd_signed ? f16_to_i32(f16(op2), roundMode, true) : f16_to_ui32(f16(op2), roundMode, true);
				case 32:
					return vd_signed ? f32_to_i64(f32(op2), roundMode, true) : f32_to_ui64(f32(op2), roundMode, true);

				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfCvtwFF() {
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			switch (getIntVSew()) {
				case 16:
					return f16_to_f32(f16(op2)).v;
				case 32:
					return f32_to_f64(f32(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfCvtnXF(bool rtz) {
		uint_fast8_t roundMode = rtz ? (uint_fast8_t)softfloat_round_minMag : softfloat_roundingMode;
		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (op2_eew) {
				case 16:
					return vd_signed ? f16_to_i8(f16(op2), roundMode, true) : f16_to_ui8(f16(op2), roundMode, true);
				case 32:
					return vd_signed ? f32_to_i16(f32(op2), roundMode, true) : f32_to_ui16(f32(op2), roundMode, true);
				case 64:
					return vd_signed ? f64_to_i32(f64(op2), roundMode, true) : f64_to_ui32(f64(op2), roundMode, true);
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<op_reg_t(op_reg_t, op_reg_t, op_reg_t, xlen_reg_t)> vfCvtnFF(bool roundOdd) {
		if (roundOdd) {
			softfloat_roundingMode = softfloat_round_odd;
		}

		return [=](op_reg_t op2, op_reg_t op1, op_reg_t vd, xlen_reg_t i) -> op_reg_t {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (op2_eew) {
				case 32:
					return f32_to_f16(f32(op2)).v;
				case 64:
					return f64_to_f32(f64(op2)).v;
				default:
					v_assert(false);
					return 0;
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vfRedSum() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			switch (getIntVSew()) {
				case 16:
					res = f16_add(f16(res), f16(op2)).v;
					break;
				case 32:
					res = f32_add(f32(res), f32(op2)).v;
					break;
				case 64:
					res = f64_add(f64(res), f64(op2)).v;
					break;
				default:
					v_assert(false);
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vfwRedSum() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			auto [vd_eew, vd_signed, op2_eew, op2_signed, op1_eew, op1_signed] = getSignedEew();
			switch (vd_eew) {
				case 32:
					res = f32_add(f32(res), f16_to_f32(f16(op2))).v;
					break;
				case 64:
					res = f64_add(f64(res), f32_to_f64(f32(op2))).v;
					break;
				default:
					v_assert(false);
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vfRedMax() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			switch (getIntVSew()) {
				case 16:
					res = f16_max(f16(res), f16(op2)).v;
					break;
				case 32:
					res = f32_max(f32(res), f32(op2)).v;
					break;
				case 64:
					res = f64_max(f64(res), f64(op2)).v;
					break;
				default:
					v_assert(false);
			}
		};
	}

	std::function<void(op_reg_t, op_reg_t, xlen_reg_t, op_reg_t&)> vfRedMin() {
		return [=](op_reg_t op2, op_reg_t op1, xlen_reg_t i, op_reg_t& res) -> void {
			switch (getIntVSew()) {
				case 16:
					res = f16_min(f16(res), f16(op2)).v;
					break;
				case 32:
					res = f32_min(f32(res), f32(op2)).v;
					break;
				case 64:
					res = f64_min(f64(res), f64(op2)).v;
					break;
				default:
					v_assert(false);
			}
		};
	}
};
