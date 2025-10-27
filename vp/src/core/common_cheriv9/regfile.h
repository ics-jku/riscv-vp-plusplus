#ifndef RISCV_ISA_REGFILE_H
#define RISCV_ISA_REGFILE_H

#include <cassert>

#include "util/common.h"

template <typename T_sxlen_t, typename T_uxlen_t>
struct RegFile_T {
	static constexpr unsigned NUM_REGS = 32;

	T_sxlen_t regs[NUM_REGS];

	RegFile_T() {
		memset(regs, 0, sizeof(regs));
	}

	RegFile_T(const RegFile_T<T_sxlen_t, T_uxlen_t> &other) {
		memcpy(regs, other.regs, sizeof(regs));
	}

	inline void reset_zero() {
		regs[zero] = 0;
	}

	inline void write(unsigned int index, T_sxlen_t value) {
		assert(index <= x31);
		assert(index != x0);
		regs[index] = value;
	}

	inline T_sxlen_t read(unsigned int index) {
		if (index > x31)
			throw std::out_of_range("out-of-range register access");
		return regs[index];
	}

	inline T_uxlen_t shamt(unsigned int index) {
		assert(index <= x31);
		if (sizeof(T_uxlen_t) == sizeof(uint32_t)) {
			// RV32
			return BIT_RANGE(regs[index], 4, 0);
		} else if (sizeof(T_uxlen_t) == sizeof(uint64_t)) {
			// RV64
			return BIT_RANGE(regs[index], 5, 0);
		} else {
			assert(false && "unsupported XLEN_T in RegFile");
			return 0;
		}
	}

	// Only used on RV64
	inline T_uxlen_t shamt_w(unsigned int index) {
		assert(index <= x31);
		if (sizeof(T_uxlen_t) == sizeof(uint64_t)) {
			return BIT_RANGE(regs[index], 4, 0);
		} else {
			assert(false && "unsupported XLEN_T in RegFile");
			return 0;
		}
	}

	inline T_sxlen_t &operator[](const unsigned int idx) {
		return regs[idx];
	}

	void show() {
		for (unsigned int i = 0; i < NUM_REGS; ++i) {
			if (sizeof(T_uxlen_t) == sizeof(uint32_t)) {
				// RV32
				printf(COLORFRMT " = %8x\n", COLORPRINT(colors[i], regnames[i]), (uint32_t)regs[i]);
			} else if (sizeof(T_uxlen_t) == sizeof(uint64_t)) {
				// RV64
				printf(COLORFRMT " = %16lx\n", COLORPRINT(colors[i], regnames[i]), (uint64_t)regs[i]);
			} else {
				assert(false && "unsupported XLEN_T in RegFile");
			}
		}
	}

	enum e {
		x0 = 0,
		x1,
		x2,
		x3,
		x4,
		x5,
		x6,
		x7,
		x8,
		x9,
		x10,
		x11,
		x12,
		x13,
		x14,
		x15,
		x16,
		x17,
		x18,
		x19,
		x20,
		x21,
		x22,
		x23,
		x24,
		x25,
		x26,
		x27,
		x28,
		x29,
		x30,
		x31,

		zero = x0,
		ra = x1,
		sp = x2,
		gp = x3,
		tp = x4,
		t0 = x5,
		t1 = x6,
		t2 = x7,
		s0 = x8,
		fp = x8,
		s1 = x9,
		a0 = x10,
		a1 = x11,
		a2 = x12,
		a3 = x13,
		a4 = x14,
		a5 = x15,
		a6 = x16,
		a7 = x17,
		s2 = x18,
		s3 = x19,
		s4 = x20,
		s5 = x21,
		s6 = x22,
		s7 = x23,
		s8 = x24,
		s9 = x25,
		s10 = x26,
		s11 = x27,
		t3 = x28,
		t4 = x29,
		t5 = x30,
		t6 = x31,
	};

	static constexpr const char *regnames[NUM_REGS] = {
	    "zero (x0)", "ra   (x1)", "sp   (x2)", "gp   (x3)", "tp   (x4)", "t0   (x5)", "t1   (x6)", "t2   (x7)",
	    "s0/fp(x8)", "s1   (x9)", "a0  (x10)", "a1  (x11)", "a2  (x12)", "a3  (x13)", "a4  (x14)", "a5  (x15)",
	    "a6  (x16)", "a7  (x17)", "s2  (x18)", "s3  (x19)", "s4  (x20)", "s5  (x21)", "s6  (x22)", "s7  (x23)",
	    "s8  (x24)", "s9  (x25)", "s10 (x26)", "s11 (x27)", "t3  (x28)", "t4  (x29)", "t5  (x30)", "t6  (x31)",
	};

	static constexpr const int regcolors[NUM_REGS] = {
#if defined(COLOR_THEME_DARK)
	    0,  1,  2,  3,  4,  5,  6,  52, 8,  9,  53, 54, 55, 56, 57, 58,
	    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
#elif defined(COLOR_THEME_LIGHT)
	    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 153, 154, 155, 156, 157, 158,
	    116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
#else
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
	};
};

#endif /* RISCV_ISA_REGFILE_H */
