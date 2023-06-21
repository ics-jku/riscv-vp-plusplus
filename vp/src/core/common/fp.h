#pragma once

#include <softfloat/softfloat.hpp>

// floating-point rounding modes
constexpr unsigned FRM_RNE = 0b000;  // Round to Nearest, ties to Even
constexpr unsigned FRM_RTZ = 0b001;  // Round towards Zero
constexpr unsigned FRM_RDN = 0b010;  // Round Down (towards -inf)
constexpr unsigned FRM_RUP = 0b011;  // Round Down (towards +inf)
constexpr unsigned FRM_RMM = 0b100;  // Round to Nearest, ties to Max Magnitude
constexpr unsigned FRM_DYN =
    0b111;  // In instruction’s rm field, selects dynamic rounding mode; In Rounding Mode register, Invalid

// NaN values
constexpr uint16_t defaultNaNF16UI = 0x7E00;
constexpr uint32_t defaultNaNF32UI = 0x7fc00000;
constexpr uint64_t defaultNaNF64UI = 0x7FF8000000000000;

constexpr float16_t f16_defaultNaN = {defaultNaNF16UI};
constexpr float32_t f32_defaultNaN = {defaultNaNF32UI};
constexpr float64_t f64_defaultNaN = {defaultNaNF64UI};

constexpr uint16_t F16_SIGN_BIT = 1 << 15;
constexpr uint32_t F32_SIGN_BIT = 1 << 31;
constexpr uint64_t F64_SIGN_BIT = 1ul << 63;

inline bool f16_isNegative(float16_t x) {
	return x.v & F16_SIGN_BIT;
}

inline bool f32_isNegative(float32_t x) {
	return x.v & F32_SIGN_BIT;
}

inline bool f64_isNegative(float64_t x) {
	return x.v & F64_SIGN_BIT;
}

inline float16_t f16_neg(float16_t x) {
	uint16_t res = x.v ^ F16_SIGN_BIT;
	return float16_t{res};
}

inline float32_t f32_neg(float32_t x) {
	return float32_t{x.v ^ F32_SIGN_BIT};
}

inline float64_t f64_neg(float64_t x) {
	return float64_t{x.v ^ F64_SIGN_BIT};
}

inline bool f16_isNaN(float16_t x) {
	// TODO correct?
	// f32 NaN:
	// - sign bit can be 0 or 1
	// - all exponent bits set
	// - at least one fraction bit set
	return ((~x.v & 0x7E00) == 0) && (x.v & 0x01FF);
}

inline bool f32_isNaN(float32_t x) {
	// f32 NaN:
	// - sign bit can be 0 or 1
	// - all exponent bits set
	// - at least one fraction bit set
	return ((~x.v & 0x7F800000) == 0) && (x.v & 0x007FFFFF);
}

inline bool f64_isNaN(float64_t x) {
	// similar to f32 NaN
	return ((~x.v & 0x7FF0000000000000) == 0) && (x.v & 0x000FFFFFFFFFFFFF);
}

inline float16_t f16_sgnj(float16_t f1, float16_t f2) {
	uint16_t res = (f1.v & ~F16_SIGN_BIT) | (f2.v & F16_SIGN_BIT);
	return float16_t{res};
}

inline float16_t f16_sgnjn(float16_t f1, float16_t f2) {
	uint16_t res = (f1.v & ~F16_SIGN_BIT) | (~f2.v & F16_SIGN_BIT);
	return float16_t{res};
}

inline float16_t f16_sgnjx(float16_t f1, float16_t f2) {
	uint16_t res = f1.v ^ (f2.v & F16_SIGN_BIT);
	return float16_t{res};
}

inline float32_t f32_sgnj(float32_t f1, float32_t f2) {
	return float32_t{(f1.v & ~F32_SIGN_BIT) | (f2.v & F32_SIGN_BIT)};
}

inline float32_t f32_sgnjn(float32_t f1, float32_t f2) {
	return float32_t{(f1.v & ~F32_SIGN_BIT) | (~f2.v & F32_SIGN_BIT)};
}

inline float32_t f32_sgnjx(float32_t f1, float32_t f2) {
	return float32_t{f1.v ^ (f2.v & F32_SIGN_BIT)};
}

inline float64_t f64_sgnj(float64_t f1, float64_t f2) {
	return float64_t{(f1.v & ~F64_SIGN_BIT) | (f2.v & F64_SIGN_BIT)};
}

inline float64_t f64_sgnjn(float64_t f1, float64_t f2) {
	return float64_t{(f1.v & ~F64_SIGN_BIT) | (~f2.v & F64_SIGN_BIT)};
}

inline float64_t f64_sgnjx(float64_t f1, float64_t f2) {
	return float64_t{f1.v ^ (f2.v & F64_SIGN_BIT)};
}

struct FpRegs {
   private:
	std::array<float64_t, 32> regs;

	bool is_boxed_f16(float64_t x) {
		return (x.v >> 16) == 0xFFFFFFFFFFFF;
	}

	float64_t box_f16(float16_t x) {
		return float64_t{(uint64_t)x.v | 0xFFFFFFFFFFFF0000};
	}

	float16_t unbox_f16(float64_t x) {
		return float16_t{(uint16_t)x.v};
	}

	bool is_boxed_f32(float64_t x) {
		return (x.v >> 32) == (uint32_t)-1;
	}

	float32_t unbox_f32(float64_t x) {
		return float32_t{(uint32_t)x.v};
	}

	float64_t box_f32(float32_t x) {
		return float64_t{(uint64_t)x.v | 0xFFFFFFFF00000000};
	}

   public:
	void write(unsigned idx, float16_t x) {
		write(idx, box_f16(x));
	}

	void write(unsigned idx, float32_t x) {
		write(idx, box_f32(x));
	}

	void write(unsigned idx, float64_t x) {
		regs[idx] = x;
	}

	uint32_t u32(unsigned idx) {
		// access raw data, e.g. to store to memory
		return regs[idx].v;
	}

	float16_t f16_boxed(float64_t element) {
		if (is_boxed_f16(element))
			return unbox_f16(element);
		else
			return f16_defaultNaN;
	}

	float16_t f16(unsigned idx) {
		return f16_boxed(regs[idx]);
	}

	float32_t f32(unsigned idx) {
		return f32_boxed(regs[idx]);
	}

	float32_t f32_boxed(float64_t element) {
		if (is_boxed_f32(element))
			return unbox_f32(element);
		else
			return f32_defaultNaN;
	}

	float64_t f64(unsigned idx) {
		return regs[idx];
	}
};