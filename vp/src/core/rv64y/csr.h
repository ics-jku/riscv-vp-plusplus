#pragma once

#include <assert.h>
#include <stdint.h>

#include <stdexcept>
#include <unordered_map>

#include "cheri_prelude.h"
#include "core/common/core_defs.h"
#include "core/common/trap.h"
#include "core/common_y/cheri_types.h"
#include "util/common.h"

namespace rv64y {

using csr_reg_t = uint64_t;

constexpr unsigned FS_OFF = 0b00;
constexpr unsigned FS_INITIAL = 0b01;
constexpr unsigned FS_CLEAN = 0b10;
constexpr unsigned FS_DIRTY = 0b11;

inline bool is_valid_privilege_level(PrivilegeLevel mode) {
	return mode == MachineMode || mode == SupervisorMode || mode == UserMode;
}

struct csr_64 {
	union reg {
		csr_reg_t val = 0;
	} reg;
};

struct csr_misa_64 : csr_misa {
	csr_misa_64() {
		init();
	}

	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t extensions : 26;
			csr_reg_t wiri : 36;
			csr_reg_t mxl : 2;
		} fields;
	} reg;

	bool has_C_extension() {
		return reg.fields.extensions & C;
	}

	bool has_E_base_isa() {
		return reg.fields.extensions & E;
	}

	bool has_user_mode_extension() {
		return reg.fields.extensions & U;
	}

	bool has_supervisor_mode_extension() {
		return reg.fields.extensions & S;
	}

	bool has_N_extension() {
		return reg.fields.extensions & N;
	}

	bool has_Y_extension() {
		return reg.fields.extensions & Y;
	}

	void init() {
		// supported extensions will be set by the iss according to RV_ISA_Config
		reg.fields.extensions = 0;
		// RV64;
		reg.fields.mxl = 2;
	}
};

struct csr_mvendorid {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t offset : 7;
			csr_reg_t bank : 25;
			csr_reg_t _unused : 32;
		} fields;
	} reg;
};

struct csr_mstatus {
	csr_mstatus() {
		// hardwire to 64 bit mode for now
		reg.fields.sxl = 2;
		reg.fields.uxl = 2;
	}

	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t uie : 1;
			csr_reg_t sie : 1;
			csr_reg_t wpri1 : 1;
			csr_reg_t mie : 1;
			csr_reg_t upie : 1;
			csr_reg_t spie : 1;
			csr_reg_t ube : 1;
			csr_reg_t mpie : 1;
			csr_reg_t spp : 1;
			csr_reg_t vs : 2;
			csr_reg_t mpp : 2;
			csr_reg_t fs : 2;
			csr_reg_t xs : 2;
			csr_reg_t mprv : 1;
			csr_reg_t sum : 1;
			csr_reg_t mxr : 1;
			csr_reg_t tvm : 1;
			csr_reg_t tw : 1;
			csr_reg_t tsr : 1;
			csr_reg_t wpri4 : 9;
			csr_reg_t uxl : 2;
			csr_reg_t sxl : 2;
			csr_reg_t sbe : 1;
			csr_reg_t mbe : 1;
			csr_reg_t wpri5 : 25;
			csr_reg_t sd : 1;
		} fields;
	} reg;
};

struct csr_mtvec {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t mode : 2;   // WARL
			csr_reg_t base : 62;  // WARL
		} fields;
	} reg;

	csr_reg_t get_base_address() {
		return reg.fields.base << 2;
	}

	enum Mode { Direct = 0, Vectored = 1 };

	void checked_write(csr_reg_t val) {
		reg.val = val;
		if (reg.fields.mode >= 1)
			reg.fields.mode = 0;
	}
};

struct csr_mie {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t usie : 1;
			csr_reg_t ssie : 1;
			csr_reg_t wpri1 : 1;
			csr_reg_t msie : 1;

			csr_reg_t utie : 1;
			csr_reg_t stie : 1;
			csr_reg_t wpri2 : 1;
			csr_reg_t mtie : 1;

			csr_reg_t ueie : 1;
			csr_reg_t seie : 1;
			csr_reg_t wpri3 : 1;
			csr_reg_t meie : 1;

			csr_reg_t wpri4 : 52;
		} fields;
	} reg;
};

struct csr_mip {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t usip : 1;
			csr_reg_t ssip : 1;
			csr_reg_t wiri1 : 1;
			csr_reg_t msip : 1;

			csr_reg_t utip : 1;
			csr_reg_t stip : 1;
			csr_reg_t wiri2 : 1;
			csr_reg_t mtip : 1;

			csr_reg_t ueip : 1;
			csr_reg_t seip : 1;
			csr_reg_t wiri3 : 1;
			csr_reg_t meip : 1;

			csr_reg_t wiri4 : 52;
		} fields;
	} reg;
};

struct csr_mepc {
	union reg {
		csr_reg_t val = 0;
	} reg;
};

struct csr_mcause {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t exception_code : 63;  // WLRL
			csr_reg_t interrupt : 1;
		} fields;
	} reg;
};

struct csr_mcounteren {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t CY : 1;
			csr_reg_t TM : 1;
			csr_reg_t IR : 1;
			csr_reg_t reserved : 61;
		} fields;
	} reg;
};

struct csr_mcountinhibit {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t CY : 1;
			csr_reg_t zero : 1;
			csr_reg_t IR : 1;
			csr_reg_t reserved : 61;
		} fields;
	} reg;
};

// struct csr_pmpcfg {
// 	union reg {
// 		csr_reg_t val = 0;
// 	} reg;
// };

// struct csr_pmpaddr {
// 	union reg {
// 		csr_reg_t val = 0;
// 		struct fields {
// 			csr_reg_t address : 54;
// 			csr_reg_t zero : 10;
// 		} fields;
// 	} reg;

// 	csr_reg_t get_address() {
// 		return reg.fields.address << 2;
// 	}
// };

struct csr_satp {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t ppn : 44;   // WARL
			csr_reg_t asid : 16;  // WARL
			csr_reg_t mode : 4;   // WARL
		} fields;
	} reg;
};

/* Actually 32 bit large, but use 64 value for consistency and simply set the read/write mask accordingly. */
struct csr_fcsr {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t fflags : 5;
			csr_reg_t frm : 3;
			csr_reg_t reserved : 24;
			csr_reg_t _ : 32;
		} fields;
		struct fflags {
			csr_reg_t NX : 1;  // invalid operation
			csr_reg_t UF : 1;  // divide by zero
			csr_reg_t OF : 1;  // overflow
			csr_reg_t DZ : 1;  // underflow
			csr_reg_t NV : 1;  // inexact
		} fflags;
	} reg;
};

struct csr_vtype {
	union reg {
		csr_reg_t val = 0x8000000000000000;  // vill=1 at reset
		struct fields {
			csr_reg_t vlmul : 3;
			csr_reg_t vsew : 3;
			csr_reg_t vta : 1;
			csr_reg_t vma : 1;
			csr_reg_t reserved : 55;
			csr_reg_t vill : 1;
		} fields;
	} reg;
};

struct csr_vl {
	union reg {
		csr_reg_t val = 0;
	} reg;
};

struct csr_vstart {
	union reg {
		csr_reg_t val = 0;
	} reg;
};

struct csr_vxrm {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t vxrm : 2;
			csr_reg_t zero : 62;
		} fields;
	} reg;
};

struct csr_vxsat {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t vxsat : 1;
			csr_reg_t zero : 63;
		} fields;
	} reg;
};

struct csr_vcsr {
	union reg {
		csr_reg_t val = 0;
		struct fields {
			csr_reg_t vxsat : 1;
			csr_reg_t vxrm : 2;
			csr_reg_t reserved : 61;
		} fields;
	} reg;
};

struct csr_vlenb {
	union reg {
		csr_reg_t val = 0;
	} reg;
};

struct csr_ccsr {
	union {
		csr_reg_t val = 0;
		struct {
			csr_reg_t e : 1;  // Enable
			csr_reg_t reserved : 29;
			csr_reg_t nr : 1;  // No DDC/PCC relocation
			csr_reg_t tc : 1;  // Tag clearing error sematnics
			csr_reg_t upper : 32;
		} fields;
	} reg;
};

struct csr_capability {
	Capability cap;
	csr_capability() {
		cap = cNullCap;
	}
	csr_capability(const Capability &cap) : cap(cap) {}

	// Cast to csr64
	operator csr_64 &() {
		return *reinterpret_cast<csr_64 *>(&cap.cap.fields.address);
	}
	operator Capability() {
		return cap;
	}
};

struct csr_mtvec_capability {
	Capability cap;

	// default ctor
	csr_mtvec_capability() {
		cap = cNullCap;
	}

	// ctor from capability
	csr_mtvec_capability(const Capability &cap) : cap(cap) {}
	// Cast to csr_mtvec
	operator csr_mtvec &() {
		return *reinterpret_cast<csr_mtvec *>(&cap.cap.fields.address);
	}

	operator Capability() {
		return cap;
	}
	Capability get_base_address() {
		Capability legalized = cap;
		legalized.cap.fields.address =
		    static_cast<int64_t>(reinterpret_cast<csr_mtvec *>(&cap.cap.fields.address)->get_base_address());
		return legalized;
	}
};

struct csr_eepc_capability {
	Capability cap;
	csr_eepc_capability() {
		cap = cNullCap;
	}
	csr_eepc_capability(const Capability &cap) : cap(cap) {}
	operator csr_mepc &() {
		return *reinterpret_cast<csr_mepc *>(&cap.cap.fields.address);
	}
	operator Capability() {
		return cap;
	}
};

struct csr_menvcfg {
	union reg {
		uint64_t val = 0;
		struct fields {
			unsigned rfu1 : 9;
			unsigned Y : 1;
			uint64_t rfu2 : 54;
		} fields;
	} reg;
};

struct csr_senvcfg {
	union reg {
		uint64_t val = 0;
		struct fields {
			unsigned rfu1 : 9;
			unsigned Y : 1;
			uint64_t rfu2 : 54;
		} fields;
	} reg;
};

namespace csr {
template <typename T>
inline bool is_bitset(T &csr, unsigned bitpos) {
	return csr.reg.val & (1 << bitpos);
}

constexpr csr_reg_t MIE_MASK = 0b101110111011;
constexpr csr_reg_t SIE_MASK = 0b001100110011;
constexpr csr_reg_t UIE_MASK = 0b000100010001;

constexpr csr_reg_t MIP_WRITE_MASK = 0b001100110011;
constexpr csr_reg_t MIP_READ_MASK = MIE_MASK;
constexpr csr_reg_t SIP_MASK = 0b11;
constexpr csr_reg_t UIP_MASK = 0b1;

constexpr csr_reg_t MEDELEG_MASK = 0b0011100001100111100001011111111111111;
// M-level interrupts (MSIP, MTIP, MEIP) are not delegatable (read-only zero), to match QEMU's behavior
constexpr csr_reg_t MIDELEG_MASK = SIE_MASK;

constexpr csr_reg_t MTVEC_MASK = ~2;

constexpr csr_reg_t MCOUNTEREN_MASK = 0b111;
constexpr csr_reg_t MCOUNTINHIBIT_MASK = 0b101;

constexpr csr_reg_t STATUS_RVY_MASK = (1ULL << 59) | (1ULL << 60) | (1ULL << 61);
constexpr csr_reg_t MSTATUS_WRITE_MASK =
    0b1000000000000000000000000000000000000000011111111111111110111011 | STATUS_RVY_MASK;
constexpr csr_reg_t MSTATUS_READ_MASK =
    0b1000000000000000000000000011111100000000011111111111111111111011 | STATUS_RVY_MASK;
constexpr csr_reg_t SSTATUS_WRITE_MASK =
    0b1000000000000000000000000000000000000000000011011110011100110011 | STATUS_RVY_MASK;
constexpr csr_reg_t SSTATUS_READ_MASK =
    0b1000000000000000000000000000001100000000000011011110011101110011 | STATUS_RVY_MASK;
constexpr csr_reg_t USTATUS_WRITE_MASK = 0b0000000000000000000000000000000000000000000000000000000000010001;
constexpr csr_reg_t USTATUS_READ_MASK = 0b0000000000000000000000000000000000000000000000000000000001010001;

constexpr csr_reg_t PMPADDR_MASK = 0b0000000000111111111111111111111111111111111111111111111111111111;

constexpr csr_reg_t SATP_MASK = 0b1111000000000000000011111111111111111111111111111111111111111111;
constexpr csr_reg_t SATP_MODE = 0b1111000000000000000000000000000000000000000000000000000000000000;

constexpr csr_reg_t FCSR_MASK = 0b11111111;
constexpr csr_reg_t VTYPE_MASK = 0b1000000000000000000000000000000000000000000000000000000011111111;
constexpr csr_reg_t VXRM_MASK = 0b11;
constexpr csr_reg_t VXSAT_MASK = 0b1;
constexpr csr_reg_t VCSR_MASK = 0b111;

constexpr csr_reg_t MENVCFG_MASK = 0xC0000000000002F1;
constexpr csr_reg_t SENVCFG_MASK = 0x000002F1;
// 64 bit timer csrs
constexpr unsigned CYCLE_ADDR = 0xC00;
constexpr unsigned TIME_ADDR = 0xC01;
constexpr unsigned INSTRET_ADDR = 0xC02;

// shadows for the above CSRs
constexpr unsigned MCYCLE_ADDR = 0xB00;
constexpr unsigned MINSTRET_ADDR = 0xB02;

// 32 bit machine CSRs
constexpr unsigned MVENDORID_ADDR = 0xF11;
constexpr unsigned MARCHID_ADDR = 0xF12;
constexpr unsigned MIMPID_ADDR = 0xF13;
constexpr unsigned MHARTID_ADDR = 0xF14;

constexpr unsigned MSTATUS_ADDR = 0x300;
constexpr unsigned MISA_ADDR = 0x301;
constexpr unsigned MEDELEG_ADDR = 0x302;
constexpr unsigned MIDELEG_ADDR = 0x303;
constexpr unsigned MIE_ADDR = 0x304;
constexpr unsigned MTVEC_ADDR = 0x305;
constexpr unsigned MCOUNTEREN_ADDR = 0x306;
constexpr unsigned MENVCFG_ADDR = 0x30A;
constexpr unsigned MCOUNTINHIBIT_ADDR = 0x320;

constexpr unsigned MSCRATCH_ADDR = 0x340;
constexpr unsigned MEPC_ADDR = 0x341;
constexpr unsigned MCAUSE_ADDR = 0x342;
constexpr unsigned MTVAL_ADDR = 0x343;
constexpr unsigned MIP_ADDR = 0x344;

// constexpr unsigned PMPCFG0_ADDR = 0x3A0;
// constexpr unsigned PMPCFG1_ADDR = 0x3A1;
// constexpr unsigned PMPCFG2_ADDR = 0x3A2;
// constexpr unsigned PMPCFG3_ADDR = 0x3A3;

// constexpr unsigned PMPADDR0_ADDR = 0x3B0;
// constexpr unsigned PMPADDR1_ADDR = 0x3B1;
// constexpr unsigned PMPADDR2_ADDR = 0x3B2;
// constexpr unsigned PMPADDR3_ADDR = 0x3B3;
// constexpr unsigned PMPADDR4_ADDR = 0x3B4;
// constexpr unsigned PMPADDR5_ADDR = 0x3B5;
// constexpr unsigned PMPADDR6_ADDR = 0x3B6;
// constexpr unsigned PMPADDR7_ADDR = 0x3B7;
// constexpr unsigned PMPADDR8_ADDR = 0x3B8;
// constexpr unsigned PMPADDR9_ADDR = 0x3B9;
// constexpr unsigned PMPADDR10_ADDR = 0x3BA;
// constexpr unsigned PMPADDR11_ADDR = 0x3BB;
// constexpr unsigned PMPADDR12_ADDR = 0x3BC;
// constexpr unsigned PMPADDR13_ADDR = 0x3BD;
// constexpr unsigned PMPADDR14_ADDR = 0x3BE;
// constexpr unsigned PMPADDR15_ADDR = 0x3BF;

// 32 bit supervisor CSRs
constexpr unsigned SSTATUS_ADDR = 0x100;
constexpr unsigned SIE_ADDR = 0x104;
constexpr unsigned STVEC_ADDR = 0x105;
constexpr unsigned SCOUNTEREN_ADDR = 0x106;
constexpr unsigned SENVCFG_ADDR = 0x10A;
constexpr unsigned SSCRATCH_ADDR = 0x140;
constexpr unsigned SEPC_ADDR = 0x141;
constexpr unsigned SCAUSE_ADDR = 0x142;
constexpr unsigned STVAL_ADDR = 0x143;
constexpr unsigned SIP_ADDR = 0x144;
constexpr unsigned SATP_ADDR = 0x180;

// 32 bit user CSRs
constexpr unsigned USTATUS_ADDR = 0x000;
constexpr unsigned UIE_ADDR = 0x004;
constexpr unsigned UTVEC_ADDR = 0x005;
constexpr unsigned USCRATCH_ADDR = 0x040;
constexpr unsigned UEPC_ADDR = 0x041;
constexpr unsigned UCAUSE_ADDR = 0x042;
constexpr unsigned UTVAL_ADDR = 0x043;
constexpr unsigned UIP_ADDR = 0x044;

// floating point CSRs
constexpr unsigned FFLAGS_ADDR = 0x001;
constexpr unsigned FRM_ADDR = 0x002;
constexpr unsigned FCSR_ADDR = 0x003;

// performance counters
constexpr unsigned HPMCOUNTER3_ADDR = 0xC03;
constexpr unsigned HPMCOUNTER4_ADDR = 0xC04;
constexpr unsigned HPMCOUNTER5_ADDR = 0xC05;
constexpr unsigned HPMCOUNTER6_ADDR = 0xC06;
constexpr unsigned HPMCOUNTER7_ADDR = 0xC07;
constexpr unsigned HPMCOUNTER8_ADDR = 0xC08;
constexpr unsigned HPMCOUNTER9_ADDR = 0xC09;
constexpr unsigned HPMCOUNTER10_ADDR = 0xC0A;
constexpr unsigned HPMCOUNTER11_ADDR = 0xC0B;
constexpr unsigned HPMCOUNTER12_ADDR = 0xC0C;
constexpr unsigned HPMCOUNTER13_ADDR = 0xC0D;
constexpr unsigned HPMCOUNTER14_ADDR = 0xC0E;
constexpr unsigned HPMCOUNTER15_ADDR = 0xC0F;
constexpr unsigned HPMCOUNTER16_ADDR = 0xC10;
constexpr unsigned HPMCOUNTER17_ADDR = 0xC11;
constexpr unsigned HPMCOUNTER18_ADDR = 0xC12;
constexpr unsigned HPMCOUNTER19_ADDR = 0xC13;
constexpr unsigned HPMCOUNTER20_ADDR = 0xC14;
constexpr unsigned HPMCOUNTER21_ADDR = 0xC15;
constexpr unsigned HPMCOUNTER22_ADDR = 0xC16;
constexpr unsigned HPMCOUNTER23_ADDR = 0xC17;
constexpr unsigned HPMCOUNTER24_ADDR = 0xC18;
constexpr unsigned HPMCOUNTER25_ADDR = 0xC19;
constexpr unsigned HPMCOUNTER26_ADDR = 0xC1A;
constexpr unsigned HPMCOUNTER27_ADDR = 0xC1B;
constexpr unsigned HPMCOUNTER28_ADDR = 0xC1C;
constexpr unsigned HPMCOUNTER29_ADDR = 0xC1D;
constexpr unsigned HPMCOUNTER30_ADDR = 0xC1E;
constexpr unsigned HPMCOUNTER31_ADDR = 0xC1F;

constexpr unsigned HPMCOUNTER3H_ADDR = 0xC83;
constexpr unsigned HPMCOUNTER4H_ADDR = 0xC84;
constexpr unsigned HPMCOUNTER5H_ADDR = 0xC85;
constexpr unsigned HPMCOUNTER6H_ADDR = 0xC86;
constexpr unsigned HPMCOUNTER7H_ADDR = 0xC87;
constexpr unsigned HPMCOUNTER8H_ADDR = 0xC88;
constexpr unsigned HPMCOUNTER9H_ADDR = 0xC89;
constexpr unsigned HPMCOUNTER10H_ADDR = 0xC8A;
constexpr unsigned HPMCOUNTER11H_ADDR = 0xC8B;
constexpr unsigned HPMCOUNTER12H_ADDR = 0xC8C;
constexpr unsigned HPMCOUNTER13H_ADDR = 0xC8D;
constexpr unsigned HPMCOUNTER14H_ADDR = 0xC8E;
constexpr unsigned HPMCOUNTER15H_ADDR = 0xC8F;
constexpr unsigned HPMCOUNTER16H_ADDR = 0xC90;
constexpr unsigned HPMCOUNTER17H_ADDR = 0xC91;
constexpr unsigned HPMCOUNTER18H_ADDR = 0xC92;
constexpr unsigned HPMCOUNTER19H_ADDR = 0xC93;
constexpr unsigned HPMCOUNTER20H_ADDR = 0xC94;
constexpr unsigned HPMCOUNTER21H_ADDR = 0xC95;
constexpr unsigned HPMCOUNTER22H_ADDR = 0xC96;
constexpr unsigned HPMCOUNTER23H_ADDR = 0xC97;
constexpr unsigned HPMCOUNTER24H_ADDR = 0xC98;
constexpr unsigned HPMCOUNTER25H_ADDR = 0xC99;
constexpr unsigned HPMCOUNTER26H_ADDR = 0xC9A;
constexpr unsigned HPMCOUNTER27H_ADDR = 0xC9B;
constexpr unsigned HPMCOUNTER28H_ADDR = 0xC9C;
constexpr unsigned HPMCOUNTER29H_ADDR = 0xC9D;
constexpr unsigned HPMCOUNTER30H_ADDR = 0xC9E;
constexpr unsigned HPMCOUNTER31H_ADDR = 0xC9F;

constexpr unsigned MHPMCOUNTER3_ADDR = 0xB03;
constexpr unsigned MHPMCOUNTER4_ADDR = 0xB04;
constexpr unsigned MHPMCOUNTER5_ADDR = 0xB05;
constexpr unsigned MHPMCOUNTER6_ADDR = 0xB06;
constexpr unsigned MHPMCOUNTER7_ADDR = 0xB07;
constexpr unsigned MHPMCOUNTER8_ADDR = 0xB08;
constexpr unsigned MHPMCOUNTER9_ADDR = 0xB09;
constexpr unsigned MHPMCOUNTER10_ADDR = 0xB0A;
constexpr unsigned MHPMCOUNTER11_ADDR = 0xB0B;
constexpr unsigned MHPMCOUNTER12_ADDR = 0xB0C;
constexpr unsigned MHPMCOUNTER13_ADDR = 0xB0D;
constexpr unsigned MHPMCOUNTER14_ADDR = 0xB0E;
constexpr unsigned MHPMCOUNTER15_ADDR = 0xB0F;
constexpr unsigned MHPMCOUNTER16_ADDR = 0xB10;
constexpr unsigned MHPMCOUNTER17_ADDR = 0xB11;
constexpr unsigned MHPMCOUNTER18_ADDR = 0xB12;
constexpr unsigned MHPMCOUNTER19_ADDR = 0xB13;
constexpr unsigned MHPMCOUNTER20_ADDR = 0xB14;
constexpr unsigned MHPMCOUNTER21_ADDR = 0xB15;
constexpr unsigned MHPMCOUNTER22_ADDR = 0xB16;
constexpr unsigned MHPMCOUNTER23_ADDR = 0xB17;
constexpr unsigned MHPMCOUNTER24_ADDR = 0xB18;
constexpr unsigned MHPMCOUNTER25_ADDR = 0xB19;
constexpr unsigned MHPMCOUNTER26_ADDR = 0xB1A;
constexpr unsigned MHPMCOUNTER27_ADDR = 0xB1B;
constexpr unsigned MHPMCOUNTER28_ADDR = 0xB1C;
constexpr unsigned MHPMCOUNTER29_ADDR = 0xB1D;
constexpr unsigned MHPMCOUNTER30_ADDR = 0xB1E;
constexpr unsigned MHPMCOUNTER31_ADDR = 0xB1F;

constexpr unsigned MHPMCOUNTER3H_ADDR = 0xB83;
constexpr unsigned MHPMCOUNTER4H_ADDR = 0xB84;
constexpr unsigned MHPMCOUNTER5H_ADDR = 0xB85;
constexpr unsigned MHPMCOUNTER6H_ADDR = 0xB86;
constexpr unsigned MHPMCOUNTER7H_ADDR = 0xB87;
constexpr unsigned MHPMCOUNTER8H_ADDR = 0xB88;
constexpr unsigned MHPMCOUNTER9H_ADDR = 0xB89;
constexpr unsigned MHPMCOUNTER10H_ADDR = 0xB8A;
constexpr unsigned MHPMCOUNTER11H_ADDR = 0xB8B;
constexpr unsigned MHPMCOUNTER12H_ADDR = 0xB8C;
constexpr unsigned MHPMCOUNTER13H_ADDR = 0xB8D;
constexpr unsigned MHPMCOUNTER14H_ADDR = 0xB8E;
constexpr unsigned MHPMCOUNTER15H_ADDR = 0xB8F;
constexpr unsigned MHPMCOUNTER16H_ADDR = 0xB90;
constexpr unsigned MHPMCOUNTER17H_ADDR = 0xB91;
constexpr unsigned MHPMCOUNTER18H_ADDR = 0xB92;
constexpr unsigned MHPMCOUNTER19H_ADDR = 0xB93;
constexpr unsigned MHPMCOUNTER20H_ADDR = 0xB94;
constexpr unsigned MHPMCOUNTER21H_ADDR = 0xB95;
constexpr unsigned MHPMCOUNTER22H_ADDR = 0xB96;
constexpr unsigned MHPMCOUNTER23H_ADDR = 0xB97;
constexpr unsigned MHPMCOUNTER24H_ADDR = 0xB98;
constexpr unsigned MHPMCOUNTER25H_ADDR = 0xB99;
constexpr unsigned MHPMCOUNTER26H_ADDR = 0xB9A;
constexpr unsigned MHPMCOUNTER27H_ADDR = 0xB9B;
constexpr unsigned MHPMCOUNTER28H_ADDR = 0xB9C;
constexpr unsigned MHPMCOUNTER29H_ADDR = 0xB9D;
constexpr unsigned MHPMCOUNTER30H_ADDR = 0xB9E;
constexpr unsigned MHPMCOUNTER31H_ADDR = 0xB9F;

constexpr unsigned MHPMEVENT3_ADDR = 0x323;
constexpr unsigned MHPMEVENT4_ADDR = 0x324;
constexpr unsigned MHPMEVENT5_ADDR = 0x325;
constexpr unsigned MHPMEVENT6_ADDR = 0x326;
constexpr unsigned MHPMEVENT7_ADDR = 0x327;
constexpr unsigned MHPMEVENT8_ADDR = 0x328;
constexpr unsigned MHPMEVENT9_ADDR = 0x329;
constexpr unsigned MHPMEVENT10_ADDR = 0x32A;
constexpr unsigned MHPMEVENT11_ADDR = 0x32B;
constexpr unsigned MHPMEVENT12_ADDR = 0x32C;
constexpr unsigned MHPMEVENT13_ADDR = 0x32D;
constexpr unsigned MHPMEVENT14_ADDR = 0x32E;
constexpr unsigned MHPMEVENT15_ADDR = 0x32F;
constexpr unsigned MHPMEVENT16_ADDR = 0x330;
constexpr unsigned MHPMEVENT17_ADDR = 0x331;
constexpr unsigned MHPMEVENT18_ADDR = 0x332;
constexpr unsigned MHPMEVENT19_ADDR = 0x333;
constexpr unsigned MHPMEVENT20_ADDR = 0x334;
constexpr unsigned MHPMEVENT21_ADDR = 0x335;
constexpr unsigned MHPMEVENT22_ADDR = 0x336;
constexpr unsigned MHPMEVENT23_ADDR = 0x337;
constexpr unsigned MHPMEVENT24_ADDR = 0x338;
constexpr unsigned MHPMEVENT25_ADDR = 0x339;
constexpr unsigned MHPMEVENT26_ADDR = 0x33A;
constexpr unsigned MHPMEVENT27_ADDR = 0x33B;
constexpr unsigned MHPMEVENT28_ADDR = 0x33C;
constexpr unsigned MHPMEVENT29_ADDR = 0x33D;
constexpr unsigned MHPMEVENT30_ADDR = 0x33E;
constexpr unsigned MHPMEVENT31_ADDR = 0x33F;

constexpr unsigned MCONFIGPTR_ADDR = 0xf15;

// vector CSRs
constexpr unsigned VSTART_ADDR = 0x008;
constexpr unsigned VXSAT_ADDR = 0x009;
constexpr unsigned VXRM_ADDR = 0x00A;
constexpr unsigned VCSR_ADDR = 0x00F;
constexpr unsigned VL_ADDR = 0xC20;
constexpr unsigned VTYPE_ADDR = 0xC21;
constexpr unsigned VLENB_ADDR = 0xC22;

// RVY CSRs
constexpr unsigned DDC_ADDR = 0x416;
constexpr unsigned MTIDC_ADDR = 0x780;
constexpr unsigned STIDC_ADDR = 0x580;
constexpr unsigned UTIDC_ADDR = 0x480;
constexpr unsigned VSTIDC_ADDR = 0xa80;
}  // namespace csr

inline csr_ccsr legalizeCCSR(csr_ccsr ccsr, csr_reg_t v) {
	// 		  // write only the defined bits, leaving the other bits untouched
	//   // Technically, WPRI does not need a legalizer, since software is
	//   // assumed to legalize; so we could remove this function.
	//   let v = Mk_ccsr(v);
	//   /* For now the e bit is not really supported so hardwired to true */
	//   let c = update_e(c, 0b1);
	//   /* Read-only feature bits to allow for software to detect CHERI semantics. */
	//   let c = update_nr(c, bool_to_bits(not(have_cheri_relocation())));
	//   let c = update_tc(c, 0b1);
	csr_ccsr c;
	c.reg.val = v;
	c.reg.fields.e = 1;  // For now the e bit is not really supported so hardwired to true
	// Read-only feature bits to allow for software to detect CHERI semantics.
	c.reg.fields.nr = 1;
	c.reg.fields.tc = 1;
	return c;
}

struct csr_table {
	csr_64 cycle;
	csr_64 time;
	csr_64 instret;

	csr_mvendorid mvendorid;
	csr_64 marchid;
	csr_64 mimpid;
	csr_64 mhartid;

	csr_mstatus mstatus;
	csr_misa_64 misa;
	csr_64 medeleg;
	csr_64 mideleg;
	csr_mie mie;
	csr_mtvec_capability mtvecc;
	csr_mcounteren mcounteren;
	csr_mcountinhibit mcountinhibit;

	csr_capability mscratchc;
	csr_eepc_capability mepcc;
	csr_mcause mcause;
	csr_64 mtval;
	csr_mip mip;

	// Disabled for RVY to match QEMU's behavior with PMP disabled (for simpler TestRIG evaluation)
	// pmp configuration
	// std::array<csr_pmpaddr, 16> pmpaddr;
	// std::array<csr_pmpcfg, 2> pmpcfg;

	// supervisor csrs (please note: some are already covered by the machine mode csrs, i.e. sstatus, sie and sip, and
	// some are required but have the same fields, hence the machine mode classes are used)
	csr_64 sedeleg;
	csr_64 sideleg;
	csr_mtvec_capability stvecc;
	csr_mcounteren scounteren;
	csr_capability sscratchc;
	csr_eepc_capability sepcc;
	csr_mcause scause;
	csr_64 stval;
	csr_satp satp;

	// user csrs (see above comment)
	csr_mtvec_capability utvecc;
	csr_capability uscratchc;
	csr_eepc_capability uepcc;
	csr_mcause ucause;
	csr_64 utval;

	csr_fcsr fcsr;

	csr_vstart vstart;
	csr_vxsat vxsat;
	csr_vxrm vxrm;
	csr_vcsr vcsr;
	csr_vtype vtype;
	csr_vl vl;
	csr_vl vlenb;

	csr_64 mconfigptr;

	// Added for RVY
	csr_menvcfg menvcfg;
	csr_senvcfg senvcfg;
	Capability ddc;
	csr_capability mtidc;
	csr_capability stidc;
	csr_capability utidc;
	csr_capability vstidc;
	// Capability dddc;
	// Replaced orginal csrs:
	csr_mtvec &utvec;
	csr_mepc &uepc;
	csr_64 &uscratch;
	csr_mtvec &stvec;
	csr_mepc &sepc;
	csr_64 &sscratch;
	csr_mtvec &mtvec;
	csr_mepc &mepc;
	csr_64 &mscratch;

	std::unordered_map<unsigned, csr_reg_t *> register_mapping;
	std::unordered_map<unsigned, std::string> csr_name_mapping;

	csr_table()
	    : utvec(utvecc),
	      uepc(uepcc),
	      uscratch(uscratchc),
	      stvec(stvecc),
	      sepc(sepcc),
	      sscratch(sscratchc),
	      mtvec(mtvecc),
	      mepc(mepcc),
	      mscratch(mscratchc) {
		using namespace csr;

		register_mapping[CYCLE_ADDR] = &cycle.reg.val;
		csr_name_mapping[CYCLE_ADDR] = "cycle";
		register_mapping[TIME_ADDR] = &time.reg.val;
		csr_name_mapping[TIME_ADDR] = "time";
		register_mapping[INSTRET_ADDR] = &instret.reg.val;
		csr_name_mapping[INSTRET_ADDR] = "instret";
		register_mapping[MCYCLE_ADDR] = &cycle.reg.val;
		csr_name_mapping[MCYCLE_ADDR] = "mcycle";
		register_mapping[MINSTRET_ADDR] = &instret.reg.val;
		csr_name_mapping[MINSTRET_ADDR] = "minstret";

		register_mapping[MVENDORID_ADDR] = &mvendorid.reg.val;
		csr_name_mapping[MVENDORID_ADDR] = "mvendorid";
		register_mapping[MARCHID_ADDR] = &marchid.reg.val;
		csr_name_mapping[MARCHID_ADDR] = "marchid";
		register_mapping[MIMPID_ADDR] = &mimpid.reg.val;
		csr_name_mapping[MIMPID_ADDR] = "mimpid";
		register_mapping[MHARTID_ADDR] = &mhartid.reg.val;
		csr_name_mapping[MHARTID_ADDR] = "mhartid";

		register_mapping[MSTATUS_ADDR] = &mstatus.reg.val;
		csr_name_mapping[MSTATUS_ADDR] = "mstatus";
		register_mapping[MISA_ADDR] = &misa.reg.val;
		csr_name_mapping[MISA_ADDR] = "misa";
		register_mapping[MEDELEG_ADDR] = &medeleg.reg.val;
		csr_name_mapping[MEDELEG_ADDR] = "medeleg";
		register_mapping[MIDELEG_ADDR] = &mideleg.reg.val;
		csr_name_mapping[MIDELEG_ADDR] = "mideleg";
		register_mapping[MIE_ADDR] = &mie.reg.val;
		csr_name_mapping[MIE_ADDR] = "mie";
		register_mapping[MTVEC_ADDR] = &mtvec.reg.val;
		csr_name_mapping[MTVEC_ADDR] = "mtvec";
		register_mapping[MCOUNTEREN_ADDR] = &mcounteren.reg.val;
		csr_name_mapping[MCOUNTEREN_ADDR] = "mcounteren";
		register_mapping[MCOUNTINHIBIT_ADDR] = &mcountinhibit.reg.val;
		csr_name_mapping[MCOUNTINHIBIT_ADDR] = "mcountinhibit";

		register_mapping[MSCRATCH_ADDR] = &mscratch.reg.val;
		csr_name_mapping[MSCRATCH_ADDR] = "mscratch";
		register_mapping[MEPC_ADDR] = &mepc.reg.val;
		csr_name_mapping[MEPC_ADDR] = "mepc";
		register_mapping[MCAUSE_ADDR] = &mcause.reg.val;
		csr_name_mapping[MCAUSE_ADDR] = "mcause";
		register_mapping[MTVAL_ADDR] = &mtval.reg.val;
		csr_name_mapping[MTVAL_ADDR] = "mtval";
		register_mapping[MIP_ADDR] = &mip.reg.val;
		csr_name_mapping[MIP_ADDR] = "mip";

		// for (unsigned i = 0; i < pmpaddr.size(); ++i) {
		// 	register_mapping[PMPADDR0_ADDR + i] = &pmpaddr[i].reg.val;
		// }

		// for (unsigned i = 0; i < pmpcfg.size(); ++i) {
		// 	register_mapping[PMPCFG0_ADDR + i] = &pmpcfg[i].reg.val;
		// }

		register_mapping[STVEC_ADDR] = &stvec.reg.val;
		csr_name_mapping[STVEC_ADDR] = "stvec";
		register_mapping[SCOUNTEREN_ADDR] = &scounteren.reg.val;
		csr_name_mapping[SCOUNTEREN_ADDR] = "scounteren";
		register_mapping[SSCRATCH_ADDR] = &sscratch.reg.val;
		csr_name_mapping[SSCRATCH_ADDR] = "sscratch";
		register_mapping[SEPC_ADDR] = &sepc.reg.val;
		csr_name_mapping[SEPC_ADDR] = "sepc";
		register_mapping[SCAUSE_ADDR] = &scause.reg.val;
		csr_name_mapping[SCAUSE_ADDR] = "scause";
		register_mapping[STVAL_ADDR] = &stval.reg.val;
		csr_name_mapping[STVAL_ADDR] = "stval";
		register_mapping[SATP_ADDR] = &satp.reg.val;
		csr_name_mapping[SATP_ADDR] = "satp";

		csr_name_mapping[SSTATUS_ADDR] = "sstatus";
		csr_name_mapping[SIE_ADDR] = "sie";
		csr_name_mapping[SIP_ADDR] = "sip";

		register_mapping[UTVEC_ADDR] = &utvec.reg.val;
		csr_name_mapping[UTVEC_ADDR] = "utvec";
		register_mapping[USCRATCH_ADDR] = &uscratch.reg.val;
		csr_name_mapping[USCRATCH_ADDR] = "uscratch";
		register_mapping[UEPC_ADDR] = &uepc.reg.val;
		csr_name_mapping[UEPC_ADDR] = "uepc";
		register_mapping[UCAUSE_ADDR] = &ucause.reg.val;
		csr_name_mapping[UCAUSE_ADDR] = "ucause";
		register_mapping[UTVAL_ADDR] = &utval.reg.val;
		csr_name_mapping[UTVAL_ADDR] = "utval";

		register_mapping[FCSR_ADDR] = &fcsr.reg.val;

		register_mapping[VSTART_ADDR] = &vstart.reg.val;
		register_mapping[VXSAT_ADDR] = &vxsat.reg.val;
		register_mapping[VXRM_ADDR] = &vxrm.reg.val;
		register_mapping[VCSR_ADDR] = &vcsr.reg.val;
		register_mapping[VL_ADDR] = &vl.reg.val;
		register_mapping[VTYPE_ADDR] = &vtype.reg.val;
		register_mapping[VLENB_ADDR] = &vlenb.reg.val;

		register_mapping[MENVCFG_ADDR] = &menvcfg.reg.val;
		csr_name_mapping[MENVCFG_ADDR] = "menvcfg";
		register_mapping[SENVCFG_ADDR] = &senvcfg.reg.val;
		csr_name_mapping[SENVCFG_ADDR] = "senvcfg";
		register_mapping[MCONFIGPTR_ADDR] = &mconfigptr.reg.val;
		csr_name_mapping[MCONFIGPTR_ADDR] = "mconfigptr";

		register_mapping[DDC_ADDR] = (csr_reg_t *)&ddc.cap.fields.address;
		register_mapping[MTIDC_ADDR] = (csr_reg_t *)&mtidc.cap.cap.fields.address;
		register_mapping[STIDC_ADDR] = (csr_reg_t *)&stidc.cap.cap.fields.address;
		register_mapping[UTIDC_ADDR] = (csr_reg_t *)&utidc.cap.cap.fields.address;
		register_mapping[VSTIDC_ADDR] = (csr_reg_t *)&vstidc.cap.cap.fields.address;
		reset();
	}

	bool is_valid_csr64_addr(unsigned addr) {
		return register_mapping.find(addr) != register_mapping.end();
	}

	void default_write64(unsigned addr, csr_reg_t value) {
		auto it = register_mapping.find(addr);
		ensure((it != register_mapping.end()) && "validate address before calling this function");
		*it->second = value;
	}

	csr_reg_t default_read64(unsigned addr) {
		auto it = register_mapping.find(addr);
		ensure((it != register_mapping.end()) && "validate address before calling this function");
		return *it->second;
	}

	void reset() {
		// Set all registers to 0, except for hartid
		// Reset all CSRs to zero
		cycle.reg.val = 0;
		time.reg.val = 0;
		instret.reg.val = 0;

		mvendorid.reg.val = 0;
		marchid.reg.val = 0;
		mimpid.reg.val = 0;
		// Do NOT reset mhartid
		// mhartid = 0;

		mstatus = {};
		// Do NOT reset misa
		// misa = {};
		medeleg.reg.val = 0;
		mideleg.reg.val = 0;
		mie = {};
		mcounteren.reg.val = 0;
		mcountinhibit.reg.val = 0;

		// mscratch.reg.val = 0;
		// mepc = {};
		mcause = {};
		mtval.reg.val = 0;
		mip = {};

		// Reset pmp configuration
		// for (auto &pmp : pmpaddr) {
		// 	pmp = {};
		// }
		// for (auto &cfg : pmpcfg) {
		// 	cfg = {};
		// }

		// Reset supervisor CSRs
		sedeleg.reg.val = 0;
		sideleg.reg.val = 0;
		scounteren.reg.val = 0;
		scause = {};
		stval.reg.val = 0;
		satp = {};

		// Reset user CSRs
		ucause = {};
		utval.reg.val = 0;

		// Reset floating-point CSRs
		fcsr = {};

		// Reset vector CSRs
		vstart = {};
		vxsat = {};
		vxrm = {};
		vcsr = {};
		vtype = {};
		vl = {};
		vlenb = {};

		// Reset YLEN CSRs
		stvecc = cDefaultCap;
		sscratchc = cNullCap;
		sepcc = cDefaultCap;
		mtvecc = cDefaultCap;
		mscratchc = cNullCap;
		mepcc = cDefaultCap;
		utvecc = cDefaultCap;
		uscratchc = cNullCap;
		uepcc = cDefaultCap;

		ddc = cDefaultCap;
		mtidc = cNullCap;
		stidc = cNullCap;
		utidc = cNullCap;
		vstidc = cNullCap;

		// reset csrs added for rvy
		menvcfg.reg.val = 0;
		senvcfg.reg.val = 0;
		// TestRIG assumes Y to be enabled
		menvcfg.reg.fields.Y = 1;
		senvcfg.reg.fields.Y = 1;

		mconfigptr.reg.val = 0;
	}
};
enum class CsrType : uint8_t { Standard = 0, YlenExtended = 1, YlenNew = 2 };

constexpr CsrType csr_type(unsigned addr) noexcept {
	// 	// Added for RVY
	switch (addr) {
		case csr::MTVEC_ADDR:
		case csr::MEPC_ADDR:
		case csr::STVEC_ADDR:
		case csr::SEPC_ADDR:
		case csr::MSCRATCH_ADDR:
		case csr::SSCRATCH_ADDR:
		case csr::USCRATCH_ADDR:
		case csr::UTVEC_ADDR:
		case csr::UEPC_ADDR:
			return CsrType::YlenExtended;
		case csr::DDC_ADDR:
		case csr::MTIDC_ADDR:
		case csr::STIDC_ADDR:
		case csr::UTIDC_ADDR:
		case csr::VSTIDC_ADDR:
			return CsrType::YlenNew;
		default:
			return CsrType::Standard;
	}
}

constexpr bool is_ylen_csr(unsigned addr) noexcept {
	return csr_type(addr) != CsrType::Standard;
}

#define SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV64 \
	case HPMCOUNTER3_ADDR:                    \
	case HPMCOUNTER4_ADDR:                    \
	case HPMCOUNTER5_ADDR:                    \
	case HPMCOUNTER6_ADDR:                    \
	case HPMCOUNTER7_ADDR:                    \
	case HPMCOUNTER8_ADDR:                    \
	case HPMCOUNTER9_ADDR:                    \
	case HPMCOUNTER10_ADDR:                   \
	case HPMCOUNTER11_ADDR:                   \
	case HPMCOUNTER12_ADDR:                   \
	case HPMCOUNTER13_ADDR:                   \
	case HPMCOUNTER14_ADDR:                   \
	case HPMCOUNTER15_ADDR:                   \
	case HPMCOUNTER16_ADDR:                   \
	case HPMCOUNTER17_ADDR:                   \
	case HPMCOUNTER18_ADDR:                   \
	case HPMCOUNTER19_ADDR:                   \
	case HPMCOUNTER20_ADDR:                   \
	case HPMCOUNTER21_ADDR:                   \
	case HPMCOUNTER22_ADDR:                   \
	case HPMCOUNTER23_ADDR:                   \
	case HPMCOUNTER24_ADDR:                   \
	case HPMCOUNTER25_ADDR:                   \
	case HPMCOUNTER26_ADDR:                   \
	case HPMCOUNTER27_ADDR:                   \
	case HPMCOUNTER28_ADDR:                   \
	case HPMCOUNTER29_ADDR:                   \
	case HPMCOUNTER30_ADDR:                   \
	case HPMCOUNTER31_ADDR:                   \
	case MHPMCOUNTER3_ADDR:                   \
	case MHPMCOUNTER4_ADDR:                   \
	case MHPMCOUNTER5_ADDR:                   \
	case MHPMCOUNTER6_ADDR:                   \
	case MHPMCOUNTER7_ADDR:                   \
	case MHPMCOUNTER8_ADDR:                   \
	case MHPMCOUNTER9_ADDR:                   \
	case MHPMCOUNTER10_ADDR:                  \
	case MHPMCOUNTER11_ADDR:                  \
	case MHPMCOUNTER12_ADDR:                  \
	case MHPMCOUNTER13_ADDR:                  \
	case MHPMCOUNTER14_ADDR:                  \
	case MHPMCOUNTER15_ADDR:                  \
	case MHPMCOUNTER16_ADDR:                  \
	case MHPMCOUNTER17_ADDR:                  \
	case MHPMCOUNTER18_ADDR:                  \
	case MHPMCOUNTER19_ADDR:                  \
	case MHPMCOUNTER20_ADDR:                  \
	case MHPMCOUNTER21_ADDR:                  \
	case MHPMCOUNTER22_ADDR:                  \
	case MHPMCOUNTER23_ADDR:                  \
	case MHPMCOUNTER24_ADDR:                  \
	case MHPMCOUNTER25_ADDR:                  \
	case MHPMCOUNTER26_ADDR:                  \
	case MHPMCOUNTER27_ADDR:                  \
	case MHPMCOUNTER28_ADDR:                  \
	case MHPMCOUNTER29_ADDR:                  \
	case MHPMCOUNTER30_ADDR:                  \
	case MHPMCOUNTER31_ADDR:                  \
	case MHPMEVENT3_ADDR:                     \
	case MHPMEVENT4_ADDR:                     \
	case MHPMEVENT5_ADDR:                     \
	case MHPMEVENT6_ADDR:                     \
	case MHPMEVENT7_ADDR:                     \
	case MHPMEVENT8_ADDR:                     \
	case MHPMEVENT9_ADDR:                     \
	case MHPMEVENT10_ADDR:                    \
	case MHPMEVENT11_ADDR:                    \
	case MHPMEVENT12_ADDR:                    \
	case MHPMEVENT13_ADDR:                    \
	case MHPMEVENT14_ADDR:                    \
	case MHPMEVENT15_ADDR:                    \
	case MHPMEVENT16_ADDR:                    \
	case MHPMEVENT17_ADDR:                    \
	case MHPMEVENT18_ADDR:                    \
	case MHPMEVENT19_ADDR:                    \
	case MHPMEVENT20_ADDR:                    \
	case MHPMEVENT21_ADDR:                    \
	case MHPMEVENT22_ADDR:                    \
	case MHPMEVENT23_ADDR:                    \
	case MHPMEVENT24_ADDR:                    \
	case MHPMEVENT25_ADDR:                    \
	case MHPMEVENT26_ADDR:                    \
	case MHPMEVENT27_ADDR:                    \
	case MHPMEVENT28_ADDR:                    \
	case MHPMEVENT29_ADDR:                    \
	case MHPMEVENT30_ADDR:                    \
	case MHPMEVENT31_ADDR

} /* namespace rv64y */
