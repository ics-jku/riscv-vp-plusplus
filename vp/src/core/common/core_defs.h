#pragma once

enum Architecture {
	RV32 = 1,
	RV64 = 2,
	RV128 = 3,
};

enum class CoreExecStatus {
	Runnable,
	HitBreakpoint,
	Terminated,
};

/*
 * According to the RISC-V ABI specification, the
 * stack pointer must be aligned 16-bytes (128-bit)
 * aligned for RV32 and RV64.
 * (RV32E requires only a 4-byte (32-bit) alignment,
 * but it does not hurt to use 16-alignment here
 * also)
 */
template <typename T_uxlen_t>
inline T_uxlen_t rv_align_stack_pointer_address(T_uxlen_t addr) {
	return addr - addr % 16;
}

constexpr unsigned SATP_MODE_BARE = 0;
constexpr unsigned SATP_MODE_SV32 = 1;
constexpr unsigned SATP_MODE_SV39 = 8;
constexpr unsigned SATP_MODE_SV48 = 9;
constexpr unsigned SATP_MODE_SV57 = 10;
constexpr unsigned SATP_MODE_SV64 = 11;

struct csr_misa {
	enum {
		A = 1,
		C = 1 << 2,
		D = 1 << 3,
		E = 1 << 4,
		F = 1 << 5,
		I = 1 << 8,
		M = 1 << 12,
		N = 1 << 13,
		S = 1 << 18,
		U = 1 << 20,
		V = 1 << 21,

		// CHERIv9
		X = 1 << 23
	};
};

/*
 * The ISA configuration of the core
 *
 * This is used by the decoder (instr.h/cpp) to check, if instructions
 * are allowed on a specific ISS.
 *
 * The lower 26 bits [0:25] are exactly the same as defined in the
 * MISA.Extensions CSR (see csr_misa) above.
 * Bits [26:31] are reserved.
 * The upper 32 bits [32:63] define extensions that are not described
 * in csr misa (e.g. Zfh)
 */
class RV_ISA_Config {
   public:
	static const uint32_t misa_extensions_mask = ((1 << 26) - 1);
	static const uint64_t Zfh = (1l << 32);

	uint64_t cfg = 0;

	RV_ISA_Config(bool use_E_base_isa = false, bool en_Zfh = false) {
		// init default: IMACFDV + NUS
		cfg = csr_misa::I | csr_misa::M | csr_misa::A | csr_misa::F | csr_misa::D | csr_misa::C | csr_misa::N |
		      csr_misa::U | csr_misa::S | csr_misa::V;

		if (use_E_base_isa) {
			select_E_base_isa();
		}
		if (en_Zfh) {
			select_Zfh();
		}
	}

	void select_E_base_isa() {
		cfg &= ~csr_misa::I;
		cfg |= csr_misa::E;
	}

	void select_Zfh() {
		cfg |= Zfh;
	}

	uint32_t get_misa_extensions() {
		return cfg & misa_extensions_mask;
	}

	void set_misa_extension(uint64_t ext) {
		cfg |= ext;
	}

	void clear_misa_extension(uint64_t ext) {
		cfg &= ~ext;
	}
};
