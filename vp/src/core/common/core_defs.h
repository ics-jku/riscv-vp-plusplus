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
	};
};
