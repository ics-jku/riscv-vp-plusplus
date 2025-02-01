#pragma once

#include <assert.h>
#include <stdint.h>

#include <stdexcept>
#include <unordered_map>

#include "core/common/core_defs.h"
#include "core/common/trap.h"
#include "util/common.h"

namespace rv32 {

constexpr unsigned FS_OFF = 0b00;
constexpr unsigned FS_INITIAL = 0b01;
constexpr unsigned FS_CLEAN = 0b10;
constexpr unsigned FS_DIRTY = 0b11;

inline bool is_valid_privilege_level(PrivilegeLevel mode) {
	return mode == MachineMode || mode == SupervisorMode || mode == UserMode;
}

struct csr_32 {
	uint32_t reg = 0;
};

struct csr_misa_32 : public csr_misa {
	csr_misa_32() {
		init();
	}

	union {
		uint32_t reg = 0;
		struct {
			unsigned extensions : 26;
			unsigned wiri : 4;
			unsigned mxl : 2;
		} fields;
	};

	bool has_C_extension() {
		return fields.extensions & C;
	}

	bool has_E_base_isa() {
		return fields.extensions & E;
	}

	bool has_user_mode_extension() {
		return fields.extensions & U;
	}

	bool has_supervisor_mode_extension() {
		return fields.extensions & S;
	}

	void init() {
		// supported extensions will be set by the iss according to RV_ISA_Config
		fields.extensions = 0;
		fields.wiri = 0;
		// RV32
		fields.mxl = 1;
	}
};

struct csr_mvendorid {
	union {
		uint32_t reg = 0;
		struct {
			unsigned offset : 7;
			unsigned bank : 25;
		} fields;
	};
};

struct csr_mstatus {
	union {
		// uint32_t reg = 0;
		uint32_t reg = 0x600;  // TODO WA
		struct {
			unsigned uie : 1;
			unsigned sie : 1;
			unsigned wpri1 : 1;
			unsigned mie : 1;
			unsigned upie : 1;
			unsigned spie : 1;
			unsigned wpri2 : 1;
			unsigned mpie : 1;
			unsigned spp : 1;
			unsigned vs : 2;
			unsigned mpp : 2;
			unsigned fs : 2;
			unsigned xs : 2;
			unsigned mprv : 1;
			unsigned sum : 1;
			unsigned mxr : 1;
			unsigned tvm : 1;
			unsigned tw : 1;
			unsigned tsr : 1;
			unsigned wpri4 : 8;
			unsigned sd : 1;
		} fields;
	};
};

struct csr_mtvec {
	union {
		uint32_t reg = 0;
		struct {
			unsigned mode : 2;   // WARL
			unsigned base : 30;  // WARL
		} fields;
	};

	uint32_t get_base_address() {
		return fields.base << 2;
	}

	enum Mode { Direct = 0, Vectored = 1 };

	void checked_write(uint32_t val) {
		reg = val;
		if (fields.mode >= 1)
			fields.mode = 0;
	}
};

struct csr_mie {
	union {
		uint32_t reg = 0;
		struct {
			unsigned usie : 1;
			unsigned ssie : 1;
			unsigned wpri1 : 1;
			unsigned msie : 1;

			unsigned utie : 1;
			unsigned stie : 1;
			unsigned wpri2 : 1;
			unsigned mtie : 1;

			unsigned ueie : 1;
			unsigned seie : 1;
			unsigned wpri3 : 1;
			unsigned meie : 1;

			unsigned wpri4 : 20;
		} fields;
	};
};

struct csr_mip {
	union {
		uint32_t reg = 0;
		struct {
			unsigned usip : 1;
			unsigned ssip : 1;
			unsigned wiri1 : 1;
			unsigned msip : 1;

			unsigned utip : 1;
			unsigned stip : 1;
			unsigned wiri2 : 1;
			unsigned mtip : 1;

			unsigned ueip : 1;
			unsigned seip : 1;
			unsigned wiri3 : 1;
			unsigned meip : 1;

			unsigned wiri4 : 20;
		} fields;
	};
};

struct csr_mepc {
	union {
		uint32_t reg = 0;
	};
};

struct csr_mcause {
	union {
		uint32_t reg = 0;
		struct {
			unsigned exception_code : 31;  // WLRL
			unsigned interrupt : 1;
		} fields;
	};
};

struct csr_mcounteren {
	union {
		uint32_t reg = 0;
		struct {
			unsigned CY : 1;
			unsigned TM : 1;
			unsigned IR : 1;
			unsigned reserved : 29;
		} fields;
	};
};

struct csr_mcountinhibit {
	union {
		uint32_t reg = 0;
		struct {
			unsigned CY : 1;
			unsigned zero : 1;
			unsigned IR : 1;
			unsigned reserved : 29;
		} fields;
	};
};

struct csr_pmpcfg {
	union {
		uint32_t reg = 0;
		struct {
			unsigned UNIMPLEMENTED : 24;  // WARL
			unsigned L0 : 1;              // WARL
			unsigned _wiri0 : 2;          // WIRI
			unsigned A0 : 2;              // WARL
			unsigned X0 : 1;              // WARL
			unsigned W0 : 1;              // WARL
			unsigned R0 : 1;              // WARL
		} fields;
	};
};

struct csr_satp {
	union {
		uint32_t reg = 0;
		struct {
			unsigned ppn : 22;  // WARL
			unsigned asid : 9;  // WARL
			unsigned mode : 1;  // WARL
		} fields;
	};
};

struct csr_fcsr {
	union {
		uint32_t reg = 0;
		struct {
			unsigned fflags : 5;
			unsigned frm : 3;
			unsigned reserved : 24;
		} fields;
		// fflags accessed separately
		struct {
			unsigned NX : 1;  // invalid operation
			unsigned UF : 1;  // divide by zero
			unsigned OF : 1;  // overflow
			unsigned DZ : 1;  // underflow
			unsigned NV : 1;  // inexact
			unsigned _ : 27;
		} fflags;
	};
};

struct csr_vtype {
	union {
		uint32_t reg = 0x8000000;  // vill=1 at reset
		struct {
			unsigned vlmul : 3;
			unsigned vsew : 3;
			unsigned vta : 1;
			unsigned vma : 1;
			unsigned reserved : 23;
			unsigned vill : 1;
		} fields;
	};
};

struct csr_vl {
	union {
		uint32_t reg = 0;
	};
};

struct csr_vstart {
	union {
		uint32_t reg = 0;
	};
};

struct csr_vxrm {
	union {
		uint32_t reg = 0;
		struct {
			unsigned vxrm : 2;
			unsigned zero : 30;
		} fields;
	};
};

struct csr_vxsat {
	union {
		uint32_t reg = 0;
		struct {
			unsigned vxsat : 1;
			unsigned zero : 31;
		} fields;
	};
};

struct csr_vcsr {
	union {
		uint32_t reg = 0;
		struct {
			unsigned vxsat : 1;
			unsigned vxrm : 2;
			unsigned reserved : 29;
		} fields;
	};
};

struct csr_vlenb {
	union {
		uint32_t reg = 0;
	};
};

/*
 * Add new subclasses with specific consistency check (e.g. by adding virtual
 * write_low, write_high functions) if necessary.
 */
struct csr_64 {
	union {
		uint64_t reg = 0;
		struct {
			int32_t low;
			int32_t high;
		} words;
	};

	void increment() {
		++reg;
	}
};

namespace csr {
template <typename T>
inline bool is_bitset(T &csr, unsigned bitpos) {
	return csr.reg & (1 << bitpos);
}

constexpr uint32_t MIE_MASK = 0b101110111011;
constexpr uint32_t SIE_MASK = 0b001100110011;
constexpr uint32_t UIE_MASK = 0b000100010001;

constexpr uint32_t MIP_WRITE_MASK = 0b001100110011;
constexpr uint32_t MIP_READ_MASK = MIE_MASK;
constexpr uint32_t SIP_MASK = 0b11;
constexpr uint32_t UIP_MASK = 0b1;

constexpr uint32_t MEDELEG_MASK = 0b1011101111111111;
constexpr uint32_t MIDELEG_MASK = MIE_MASK;

constexpr uint32_t MTVEC_MASK = ~2;

constexpr uint32_t MCOUNTEREN_MASK = 0b111;
constexpr uint32_t MCOUNTINHIBIT_MASK = 0b101;

constexpr uint32_t SEDELEG_MASK = 0b1011000111111111;
constexpr uint32_t SIDELEG_MASK = MIDELEG_MASK;

constexpr uint32_t MSTATUS_MASK = 0b10000000011111111111111110111011;
constexpr uint32_t SSTATUS_MASK = 0b10000000000011011110011100110011;
constexpr uint32_t USTATUS_MASK = 0b00000000000000000000000000010001;

constexpr uint32_t SATP_MASK = 0b10000000001111111111111111111111;
constexpr uint32_t SATP_MODE = 0b10000000000000000000000000000000;

constexpr uint32_t FCSR_MASK = 0b11111111;

constexpr uint32_t VTYPE_MASK = 0b10000000000000000000000011111111;
constexpr uint32_t VXRM_MASK = 0b11;
constexpr uint64_t VXSAT_MASK = 0b1;
constexpr uint32_t VCSR_MASK = 0b111;

// 64 bit timer csrs
constexpr unsigned CYCLE_ADDR = 0xC00;
constexpr unsigned CYCLEH_ADDR = 0xC80;
constexpr unsigned TIME_ADDR = 0xC01;
constexpr unsigned TIMEH_ADDR = 0xC81;
constexpr unsigned INSTRET_ADDR = 0xC02;
constexpr unsigned INSTRETH_ADDR = 0xC82;

// shadows for the above CSRs
constexpr unsigned MCYCLE_ADDR = 0xB00;
constexpr unsigned MCYCLEH_ADDR = 0xB80;
constexpr unsigned MTIME_ADDR = 0xB01;
constexpr unsigned MTIMEH_ADDR = 0xB81;
constexpr unsigned MINSTRET_ADDR = 0xB02;
constexpr unsigned MINSTRETH_ADDR = 0xB82;

// debug CSRs
constexpr unsigned TSELECT_ADDR = 0x7A0;
constexpr unsigned TDATA1_ADDR = 0x7A1;
constexpr unsigned TDATA2_ADDR = 0x7A2;
constexpr unsigned TDATA3_ADDR = 0x7A3;
constexpr unsigned DCSR_ADDR = 0x7B0;
constexpr unsigned DPC_ADDR = 0x7B1;
constexpr unsigned DSCRATCH0_ADDR = 0x7B2;
constexpr unsigned DSCRATCH1_ADDR = 0x7B3;

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
constexpr unsigned MCOUNTINHIBIT_ADDR = 0x320;

constexpr unsigned MSCRATCH_ADDR = 0x340;
constexpr unsigned MEPC_ADDR = 0x341;
constexpr unsigned MCAUSE_ADDR = 0x342;
constexpr unsigned MTVAL_ADDR = 0x343;
constexpr unsigned MIP_ADDR = 0x344;

constexpr unsigned PMPCFG0_ADDR = 0x3A0;
constexpr unsigned PMPCFG1_ADDR = 0x3A1;
constexpr unsigned PMPCFG2_ADDR = 0x3A2;
constexpr unsigned PMPCFG3_ADDR = 0x3A3;

constexpr unsigned PMPADDR0_ADDR = 0x3B0;
constexpr unsigned PMPADDR1_ADDR = 0x3B1;
constexpr unsigned PMPADDR2_ADDR = 0x3B2;
constexpr unsigned PMPADDR3_ADDR = 0x3B3;
constexpr unsigned PMPADDR4_ADDR = 0x3B4;
constexpr unsigned PMPADDR5_ADDR = 0x3B5;
constexpr unsigned PMPADDR6_ADDR = 0x3B6;
constexpr unsigned PMPADDR7_ADDR = 0x3B7;
constexpr unsigned PMPADDR8_ADDR = 0x3B8;
constexpr unsigned PMPADDR9_ADDR = 0x3B9;
constexpr unsigned PMPADDR10_ADDR = 0x3BA;
constexpr unsigned PMPADDR11_ADDR = 0x3BB;
constexpr unsigned PMPADDR12_ADDR = 0x3BC;
constexpr unsigned PMPADDR13_ADDR = 0x3BD;
constexpr unsigned PMPADDR14_ADDR = 0x3BE;
constexpr unsigned PMPADDR15_ADDR = 0x3BF;

// 32 bit supervisor CSRs
constexpr unsigned SSTATUS_ADDR = 0x100;
constexpr unsigned SEDELEG_ADDR = 0x102;
constexpr unsigned SIDELEG_ADDR = 0x103;
constexpr unsigned SIE_ADDR = 0x104;
constexpr unsigned STVEC_ADDR = 0x105;
constexpr unsigned SCOUNTEREN_ADDR = 0x106;
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

// vector CSRs
constexpr unsigned VSTART_ADDR = 0x008;
constexpr unsigned VXSAT_ADDR = 0x009;
constexpr unsigned VXRM_ADDR = 0x00A;
constexpr unsigned VCSR_ADDR = 0x00F;
constexpr unsigned VL_ADDR = 0xC20;
constexpr unsigned VTYPE_ADDR = 0xC21;
constexpr unsigned VLENB_ADDR = 0xC22;
}  // namespace csr

struct csr_table {
	csr_64 cycle;
	csr_64 time;
	csr_64 instret;

	csr_mvendorid mvendorid;
	csr_32 marchid;
	csr_32 mimpid;
	csr_32 mhartid;

	csr_mstatus mstatus;
	csr_misa_32 misa;
	csr_32 medeleg;
	csr_32 mideleg;
	csr_mie mie;
	csr_mtvec mtvec;
	csr_mcounteren mcounteren;
	csr_mcountinhibit mcountinhibit;

	csr_32 mscratch;
	csr_mepc mepc;
	csr_mcause mcause;
	csr_32 mtval;
	csr_mip mip;

	// pmp configuration
	std::array<csr_32, 16> pmpaddr;
	std::array<csr_pmpcfg, 4> pmpcfg;

	// supervisor csrs (please note: some are already covered by the machine mode csrs, i.e. sstatus, sie and sip, and
	// some are required but have the same fields, hence the machine mode classes are used)
	csr_32 sedeleg;
	csr_32 sideleg;
	csr_mtvec stvec;
	csr_mcounteren scounteren;
	csr_32 sscratch;
	csr_mepc sepc;
	csr_mcause scause;
	csr_32 stval;
	csr_satp satp;

	// user csrs (see above comment)
	csr_mtvec utvec;
	csr_32 uscratch;
	csr_mepc uepc;
	csr_mcause ucause;
	csr_32 utval;

	csr_fcsr fcsr;

	csr_vstart vstart;
	csr_vxsat vxsat;
	csr_vxrm vxrm;
	csr_vcsr vcsr;
	csr_vtype vtype;
	csr_vl vl;
	csr_vl vlenb;

	std::unordered_map<unsigned, uint32_t *> register_mapping;

	csr_table() {
		using namespace csr;

		register_mapping[CYCLE_ADDR] = (uint32_t *)(&cycle.reg);
		register_mapping[CYCLEH_ADDR] = (uint32_t *)(&cycle.reg) + 1;
		register_mapping[TIME_ADDR] = (uint32_t *)(&time.reg);
		register_mapping[TIMEH_ADDR] = (uint32_t *)(&time.reg) + 1;
		register_mapping[INSTRET_ADDR] = (uint32_t *)(&instret.reg);
		register_mapping[INSTRETH_ADDR] = (uint32_t *)(&instret.reg) + 1;
		register_mapping[MCYCLE_ADDR] = (uint32_t *)(&cycle.reg);
		register_mapping[MCYCLEH_ADDR] = (uint32_t *)(&cycle.reg) + 1;
		register_mapping[MTIME_ADDR] = (uint32_t *)(&time.reg);
		register_mapping[MTIMEH_ADDR] = (uint32_t *)(&time.reg) + 1;
		register_mapping[MINSTRET_ADDR] = (uint32_t *)(&instret.reg);
		register_mapping[MINSTRETH_ADDR] = (uint32_t *)(&instret.reg) + 1;

		register_mapping[MVENDORID_ADDR] = &mvendorid.reg;
		register_mapping[MARCHID_ADDR] = &marchid.reg;
		register_mapping[MIMPID_ADDR] = &mimpid.reg;
		register_mapping[MHARTID_ADDR] = &mhartid.reg;

		register_mapping[MSTATUS_ADDR] = &mstatus.reg;
		register_mapping[MISA_ADDR] = &misa.reg;
		register_mapping[MEDELEG_ADDR] = &medeleg.reg;
		register_mapping[MIDELEG_ADDR] = &mideleg.reg;
		register_mapping[MIE_ADDR] = &mie.reg;
		register_mapping[MTVEC_ADDR] = &mtvec.reg;
		register_mapping[MCOUNTEREN_ADDR] = &mcounteren.reg;
		register_mapping[MCOUNTINHIBIT_ADDR] = &mcountinhibit.reg;

		register_mapping[MSCRATCH_ADDR] = &mscratch.reg;
		register_mapping[MEPC_ADDR] = &mepc.reg;
		register_mapping[MCAUSE_ADDR] = &mcause.reg;
		register_mapping[MTVAL_ADDR] = &mtval.reg;
		register_mapping[MIP_ADDR] = &mip.reg;

		for (unsigned i = 0; i < 16; ++i) register_mapping[PMPADDR0_ADDR + i] = &pmpaddr[i].reg;

		for (unsigned i = 0; i < 4; ++i) register_mapping[PMPCFG0_ADDR + i] = &pmpcfg[i].reg;

		register_mapping[SEDELEG_ADDR] = &sedeleg.reg;
		register_mapping[SIDELEG_ADDR] = &sideleg.reg;
		register_mapping[STVEC_ADDR] = &stvec.reg;
		register_mapping[SCOUNTEREN_ADDR] = &scounteren.reg;
		register_mapping[SSCRATCH_ADDR] = &sscratch.reg;
		register_mapping[SEPC_ADDR] = &sepc.reg;
		register_mapping[SCAUSE_ADDR] = &scause.reg;
		register_mapping[STVAL_ADDR] = &stval.reg;
		register_mapping[SATP_ADDR] = &satp.reg;

		register_mapping[UTVEC_ADDR] = &utvec.reg;
		register_mapping[USCRATCH_ADDR] = &uscratch.reg;
		register_mapping[UEPC_ADDR] = &uepc.reg;
		register_mapping[UCAUSE_ADDR] = &ucause.reg;
		register_mapping[UTVAL_ADDR] = &utval.reg;

		register_mapping[FCSR_ADDR] = &fcsr.reg;

		register_mapping[VSTART_ADDR] = &vstart.reg;
		register_mapping[VXSAT_ADDR] = &vxsat.reg;
		register_mapping[VXRM_ADDR] = &vxrm.reg;
		register_mapping[VCSR_ADDR] = &vcsr.reg;
		register_mapping[VL_ADDR] = &vl.reg;
		register_mapping[VTYPE_ADDR] = &vtype.reg;
		register_mapping[VLENB_ADDR] = &vlenb.reg;
	}

	bool is_valid_csr32_addr(unsigned addr) {
		return register_mapping.find(addr) != register_mapping.end();
	}

	void default_write32(unsigned addr, uint32_t value) {
		auto it = register_mapping.find(addr);
		ensure((it != register_mapping.end()) && "validate address before calling this function");
		*it->second = value;
	}

	uint32_t default_read32(unsigned addr) {
		auto it = register_mapping.find(addr);
		ensure((it != register_mapping.end()) && "validate address before calling this function");
		return *it->second;
	}
};

#define SWITCH_CASE_MATCH_ANY_HPMCOUNTER_RV32 \
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
	case HPMCOUNTER3H_ADDR:                   \
	case HPMCOUNTER4H_ADDR:                   \
	case HPMCOUNTER5H_ADDR:                   \
	case HPMCOUNTER6H_ADDR:                   \
	case HPMCOUNTER7H_ADDR:                   \
	case HPMCOUNTER8H_ADDR:                   \
	case HPMCOUNTER9H_ADDR:                   \
	case HPMCOUNTER10H_ADDR:                  \
	case HPMCOUNTER11H_ADDR:                  \
	case HPMCOUNTER12H_ADDR:                  \
	case HPMCOUNTER13H_ADDR:                  \
	case HPMCOUNTER14H_ADDR:                  \
	case HPMCOUNTER15H_ADDR:                  \
	case HPMCOUNTER16H_ADDR:                  \
	case HPMCOUNTER17H_ADDR:                  \
	case HPMCOUNTER18H_ADDR:                  \
	case HPMCOUNTER19H_ADDR:                  \
	case HPMCOUNTER20H_ADDR:                  \
	case HPMCOUNTER21H_ADDR:                  \
	case HPMCOUNTER22H_ADDR:                  \
	case HPMCOUNTER23H_ADDR:                  \
	case HPMCOUNTER24H_ADDR:                  \
	case HPMCOUNTER25H_ADDR:                  \
	case HPMCOUNTER26H_ADDR:                  \
	case HPMCOUNTER27H_ADDR:                  \
	case HPMCOUNTER28H_ADDR:                  \
	case HPMCOUNTER29H_ADDR:                  \
	case HPMCOUNTER30H_ADDR:                  \
	case HPMCOUNTER31H_ADDR:                  \
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
	case MHPMCOUNTER3H_ADDR:                  \
	case MHPMCOUNTER4H_ADDR:                  \
	case MHPMCOUNTER5H_ADDR:                  \
	case MHPMCOUNTER6H_ADDR:                  \
	case MHPMCOUNTER7H_ADDR:                  \
	case MHPMCOUNTER8H_ADDR:                  \
	case MHPMCOUNTER9H_ADDR:                  \
	case MHPMCOUNTER10H_ADDR:                 \
	case MHPMCOUNTER11H_ADDR:                 \
	case MHPMCOUNTER12H_ADDR:                 \
	case MHPMCOUNTER13H_ADDR:                 \
	case MHPMCOUNTER14H_ADDR:                 \
	case MHPMCOUNTER15H_ADDR:                 \
	case MHPMCOUNTER16H_ADDR:                 \
	case MHPMCOUNTER17H_ADDR:                 \
	case MHPMCOUNTER18H_ADDR:                 \
	case MHPMCOUNTER19H_ADDR:                 \
	case MHPMCOUNTER20H_ADDR:                 \
	case MHPMCOUNTER21H_ADDR:                 \
	case MHPMCOUNTER22H_ADDR:                 \
	case MHPMCOUNTER23H_ADDR:                 \
	case MHPMCOUNTER24H_ADDR:                 \
	case MHPMCOUNTER25H_ADDR:                 \
	case MHPMCOUNTER26H_ADDR:                 \
	case MHPMCOUNTER27H_ADDR:                 \
	case MHPMCOUNTER28H_ADDR:                 \
	case MHPMCOUNTER29H_ADDR:                 \
	case MHPMCOUNTER30H_ADDR:                 \
	case MHPMCOUNTER31H_ADDR:                 \
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

}  // namespace rv32
