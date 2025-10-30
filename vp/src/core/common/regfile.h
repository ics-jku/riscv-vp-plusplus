/*
 * Template for specific RISC-V register files
 */

#ifndef RISCV_ISA_REGFILE_H
#define RISCV_ISA_REGFILE_H

#include <cassert>

#include "regfile_base.h"
#include "util/common.h"

template <typename T_sxlen_t, typename T_uxlen_t>
struct RegFile_T : public RegFileBase {
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
};

#endif /* RISCV_ISA_REGFILE_H */
