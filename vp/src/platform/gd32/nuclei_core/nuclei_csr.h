/*
This file is for adding the Nuclei Core specific CSRs.
See: https://doc.nucleisys.com/nuclei_spec/isa/core_csr.html
*/
#pragma once

#include "csr.h"

namespace rv32 {

// RISC-V Standard CSRs adapted by Nuclei
struct nuclei_csr_mtvec {
	union {
		uint32_t reg = 0;
		struct {
			unsigned mode : 6;
			unsigned addr : 26;
		} fields;
	};

	uint32_t get_base_address() {
		return fields.addr & ~0b11;
	}

	enum Mode { CLIC = 0b11, CLINT = 0 };
};

struct nuclei_csr_mcause {
	union {
		uint32_t reg = 0;
		struct {
			unsigned exccode : 12;
			unsigned reserved1 : 4;
			unsigned mpil : 8;
			unsigned reserved2 : 3;
			unsigned mpie : 1;
			unsigned mpp : 2;
			unsigned minhv : 1;
			unsigned interrupt : 1;
		} fields;
	};
};

// Nuclei core specific CSRS
struct nuclei_csr_milm_ctl {
	union {
		uint32_t reg = 0;
		struct {
			unsigned ilm_enable : 1;
			unsigned reserved1 : 9;
			unsigned ilm_bpa : 22;
		} fields;
	};
};

struct nuclei_csr_mdlm_ctl {
	union {
		uint32_t reg = 0;
		struct {
			unsigned dlm_enable : 1;
			unsigned reserved1 : 9;
			unsigned dlm_bpa : 22;
		} fields;
	};
};

struct nuclei_csr_msubm {
	union {
		uint32_t reg = 0;
		struct {
			unsigned reserved1 : 6;
			unsigned typ : 2;
			unsigned ptyp : 2;
			unsigned reserved2 : 22;
		} fields;
	};

	enum Mode { Normal = 0, Interrupt = 1, Exception = 2, NMI = 3 };
};

struct nuclei_csr_mdcause {
	union {
		uint32_t reg = 0;
		struct {
			unsigned mdcause : 2;
			unsigned reserved1 : 30;
		} fields;
	};
};

struct nuclei_csr_mcache_ctl {
	union {
		uint32_t reg = 0;
		struct {
			unsigned reserved2 : 11;
			unsigned dc_rwdecc : 1;
			unsigned dc_rwtecc : 1;
			unsigned dc_ecc_excp_en : 1;
			unsigned dc_ecc_en : 1;
			unsigned dc_en : 1;
			unsigned reserved1 : 10;
			unsigned ic_rwdecc : 1;
			unsigned ic_rwtecc : 1;
			unsigned ic_ecc_excp_en : 1;
			unsigned ic_ecc_en : 1;
			unsigned ic_scpd_mod : 1;
			unsigned ic_en : 1;
		} fields;
	};
};

struct nuclei_csr_mmisc_ctl {
	union {
		uint32_t reg = 0;
		struct {
			unsigned reserved1 : 3;
			unsigned bpu_enable : 1;
			unsigned reserved2 : 2;
			unsigned unalgn_enable : 1;
			unsigned reserved3 : 2;
			unsigned nmi_cause_ff : 1;
			unsigned reserved4 : 22;
		} fields;
	};
};

struct nuclei_csr_msavestatus {
	union {
		uint32_t reg = 0;
		struct {
			unsigned mpie1 : 1;
			unsigned mpp1 : 2;
			unsigned reserved1 : 3;
			unsigned ptyp1 : 2;
			unsigned mpie2 : 1;
			unsigned mpp2 : 2;
			unsigned reserved2 : 3;
			unsigned ptyp2 : 2;
			unsigned reserved3 : 16;
		} fields;
	};
};

struct nuclei_csr_mintstatus {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned uil : 8;
			unsigned reserved1 : 16;
			unsigned mil : 8;
		} fields;
	};
};

struct nuclei_csr_mtvt2 {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned mtvt2en : 1;
			unsigned reserved1 : 1;
			unsigned cmmon_code_entry : 30;
		} fields;
	};
};

struct nuclei_csr_sleepvalue {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned sleepvalue : 1;
			unsigned reserved1 : 31;
		} fields;
	};
};

struct nuclei_csr_txevt {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned txevt : 1;
			unsigned reserved1 : 31;
		} fields;
	};
};

struct nuclei_csr_wfe {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned wfe : 1;
			unsigned reserved1 : 31;
		} fields;
	};
};

struct nuclei_csr_ucode {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned ov : 1;
			unsigned reserved1 : 31;
		} fields;
	};
};

struct nuclei_csr_mcfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned tee : 1;
			unsigned ecc : 1;
			unsigned clic : 1;
			unsigned plic : 1;
			unsigned fio : 1;
			unsigned ppi : 1;
			unsigned nice : 1;
			unsigned ilm : 1;
			unsigned dlm : 1;
			unsigned icache : 1;
			unsigned dcache : 1;
			unsigned reserved1 : 21;
		} fields;
	};
};

struct nuclei_csr_micfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned ic_set : 4;
			unsigned ic_way : 3;
			unsigned ic_lsize : 3;
			unsigned reserved1 : 6;
			unsigned ilm_size : 5;
			unsigned ilm_xonly : 1;
			unsigned reserved2 : 10;
		} fields;
	};
};

struct nuclei_csr_mdcfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned dc_set : 4;
			unsigned dc_way : 3;
			unsigned dc_lsize : 3;
			unsigned reserved1 : 6;
			unsigned dlm_size : 5;
			unsigned reserved2 : 11;
		} fields;
	};
};

struct nuclei_csr_mtlbcfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned mtlb_set : 4;
			unsigned mtlb_way : 3;
			unsigned mtlb_lsize : 3;
			unsigned mtlb_ecc : 1;
			unsigned reserved1 : 5;
			unsigned itlb_size : 3;
			unsigned dtlb_size : 3;
			unsigned reserved2 : 10;
		} fields;
	};
};

struct nuclei_csr_mppicfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned reserved1 : 1;
			unsigned ppi_size : 5;
			unsigned reserved2 : 4;
			unsigned ppi_bpa : 22;
		} fields;
	};
};

struct nuclei_csr_mfiocfg_info {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned reserved1 : 1;
			unsigned fio_size : 5;
			unsigned reserved2 : 4;
			unsigned fio_bpa : 22;
		} fields;
	};
};

struct nuclei_csr_mecc_lock {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned ecc_lock : 1;
			unsigned reserved1 : 31;
		} fields;
	};
};

struct nuclei_csr_mecc_code {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned code : 7;       // or 8 or 9
			unsigned reserved1 : 9;  // or 10 or 11
			unsigned ramid : 5;
			unsigned reserved2 : 3;
			unsigned sramid : 5;
			unsigned reserved3 : 3;
		} fields;
	};
};

struct nuclei_csr_mtlb_ctl {
	union {
		u_int32_t reg = 0;
		struct {
			unsigned tlb_ecc_en : 1;
			unsigned tlb_ecc_excp_en : 1;
			unsigned tlb_rwtecc : 1;
			unsigned tlb_rwdecc : 1;
			unsigned reserved1 : 28;
		} fields;
	};
};

namespace csr {
constexpr uint32_t MCAUSE_MASK = 0b11111000111111110000111111111111;
constexpr uint32_t MILM_CTL_MASK = 0b11111111111111111111110000000001;
constexpr uint32_t MDLM_CTL_MASK = MILM_CTL_MASK;
constexpr uint32_t MSUBM_MASK = 0b1111000000;
constexpr uint32_t MDCAUSE_MASK = 0b11;
constexpr uint32_t MCACHE_CTL_MASK = 0b111110000000000111111;
constexpr uint32_t MMISC_CTL_MASK = 0b1001001000;
constexpr uint32_t MSAVESTATUS_MASK = 0b1100011111000111;
constexpr uint32_t MINTSTATUS_MASK = 0b11111111000000000000000011111111;
constexpr uint32_t MTVT2_MASK = ~0b10;
constexpr uint32_t SLEEPVALUE_MASK = 0b1;
constexpr uint32_t TXEVT_MASK = 0b1;
constexpr uint32_t WFE_MASK = 0b1;
constexpr uint32_t UCODE_MASK = 0b1;
constexpr uint32_t MCFG_INFO_MASK = 0b11111111111;
constexpr uint32_t MICFG_INFO_MASK = 0b1111110000001111111111;
constexpr uint32_t MDCFG_INFO_MASK = 0b111110000001111111111;
constexpr uint32_t MTLBCFG_INFO_MASK = 0b1111110000011111111111;
constexpr uint32_t MPPICFG_INFO_MASK = 0b11111111111111111111110000111110;
constexpr uint32_t MFIOCFG_INFO_MASK = MPPICFG_INFO_MASK;
constexpr uint32_t MECC_LOCK_MASK = 0b1;
constexpr uint32_t MECC_CODE_MASK = 0b11111000111110000000111111111;
constexpr uint32_t MTLB_CTL_MASK = 0b1111;

constexpr unsigned MTVT_ADDR = 0x307;
constexpr unsigned MNXTI_ADDR = 0x345;
constexpr unsigned MINTSTATUS_ADDR = 0x346;
constexpr unsigned MSCRATCHCSW_ADDR = 0x348;
constexpr unsigned MSCRATCHCSWL_ADDR = 0x349;
constexpr unsigned MILM_CTL_ADDR = 0x7C0;
constexpr unsigned MDLM_CTL_ADDR = 0x7C1;
constexpr unsigned MECC_CODE_ADDR = 0x7C2;
constexpr unsigned MNVEC_ADDR = 0x7C3;
constexpr unsigned MSUBM_ADDR = 0x7C4;
constexpr unsigned MDCAUSE_ADDR = 0x7C9;
constexpr unsigned MCACHE_CTL_ADDR = 0x7CA;
constexpr unsigned MMISC_CTL_ADDR = 0x7D0;
constexpr unsigned MSAVESTATUS_ADDR = 0x7D6;
constexpr unsigned MSAVEEPC1_ADDR = 0x7D7;
constexpr unsigned MSAVECAUSE1_ADDR = 0x7D8;
constexpr unsigned MSAVEEPC2_ADDR = 0x7D9;
constexpr unsigned MSAVECAUSE2_ADDR = 0x7DA;
constexpr unsigned MTLB_CTL_ADDR = 0x7DD;
constexpr unsigned MECC_LOCK_ADDR = 0x7DE;
constexpr unsigned PUSHMSUBM_ADDR = 0x7EB;
constexpr unsigned MTVT2_ADDR = 0x7EC;
constexpr unsigned JALMNXTI_ADDR = 0x7ED;
constexpr unsigned PUSHMCAUSE_ADDR = 0x7EE;
constexpr unsigned PUSHMEPC_ADDR = 0x7EF;
constexpr unsigned MPPICFG_INFO_ADDR = 0x7F0;
constexpr unsigned MFIOCFG_INFO_ADDR = 0x7F1;
constexpr unsigned SLEEPVALUE_ADDR = 0x811;
constexpr unsigned TXEVT_ADDR = 0x812;
constexpr unsigned WFE_ADDR = 0x810;
constexpr unsigned MICFG_INFO_ADDR = 0xFC0;
constexpr unsigned MDCFG_INFO_ADDR = 0xFC1;
constexpr unsigned MCFG_INFO_ADDR = 0xFC2;
constexpr unsigned MTLBCFG_INFO_ADDR = 0xFC3;

}  // namespace csr

struct nuclei_csr_table : public csr_table {
	nuclei_csr_mtvec nuclei_mtvec;
	nuclei_csr_mcause nuclei_mcause;

	nuclei_csr_milm_ctl milm_ctl;
	nuclei_csr_milm_ctl mdlm_ctl;
	csr_32 mnvec;
	nuclei_csr_msubm msubm;
	nuclei_csr_mdcause mdcause;
	nuclei_csr_mcache_ctl mcache_ctl;
	nuclei_csr_mmisc_ctl mmisc_ctl;
	nuclei_csr_msavestatus msavestatus;
	csr_32 msaveepc1;
	csr_32 msaveepc2;
	csr_32 msavecause1;
	csr_32 msavecause2;
	// csr_32 msavedcause1; // documentation mentions no address for this register
	// csr_32 msavedcause2; // documentation mentions no address for this register
	csr_32 mtvt;
	csr_32 mnxti;
	nuclei_csr_mintstatus mintstatus;
	nuclei_csr_mtvt2 mtvt2;
	csr_32 jalmnxti;
	csr_32 pushmsubm;
	csr_32 pushmcause;
	csr_32 pushmepc;
	csr_32 mscratchcsw;
	csr_32 mscratchcswl;
	nuclei_csr_sleepvalue sleepvalue;
	nuclei_csr_txevt txevt;
	nuclei_csr_wfe wfe;
	// nuclei_csr_ucode ucode; // documentation mentions no address for this register
	nuclei_csr_mcfg_info mcfg_info;
	nuclei_csr_micfg_info micfg_info;
	nuclei_csr_mdcfg_info mdcfg_info;
	nuclei_csr_mtlbcfg_info mtlbcfg_info;
	nuclei_csr_mppicfg_info mppicfg_info;
	nuclei_csr_mfiocfg_info mfiocfg_info;
	nuclei_csr_mecc_lock mecc_lock;
	nuclei_csr_mecc_code mecc_code;
	nuclei_csr_mtlb_ctl mtlb_ctl;

	nuclei_csr_table() : csr_table() {
		using namespace csr;
		// replace some of the standard CSRs
		register_mapping[MTVEC_ADDR] = &nuclei_mtvec.reg;
		register_mapping[MCAUSE_ADDR] = &nuclei_mcause.reg;

		// add Nuclei CSRs
		register_mapping[MILM_CTL_ADDR] = &milm_ctl.reg;
		register_mapping[MDLM_CTL_ADDR] = &mdlm_ctl.reg;
		register_mapping[MNVEC_ADDR] = &mnvec.reg;
		register_mapping[MSUBM_ADDR] = &msubm.reg;
		register_mapping[MDCAUSE_ADDR] = &mdcause.reg;
		register_mapping[MCACHE_CTL_ADDR] = &mcache_ctl.reg;
		register_mapping[MMISC_CTL_ADDR] = &mmisc_ctl.reg;
		register_mapping[MSAVESTATUS_ADDR] = &msavestatus.reg;
		register_mapping[MSAVEEPC1_ADDR] = &msaveepc1.reg;
		register_mapping[MSAVECAUSE1_ADDR] = &msaveepc2.reg;
		register_mapping[MSAVEEPC2_ADDR] = &msavecause1.reg;
		register_mapping[MSAVECAUSE2_ADDR] = &msavecause2.reg;
		register_mapping[MTVT_ADDR] = &mtvt.reg;
		register_mapping[MNXTI_ADDR] = &mnxti.reg;
		register_mapping[MINTSTATUS_ADDR] = &mintstatus.reg;
		register_mapping[MTVT2_ADDR] = &mtvt2.reg;
		register_mapping[JALMNXTI_ADDR] = &jalmnxti.reg;
		register_mapping[PUSHMSUBM_ADDR] = &pushmsubm.reg;
		register_mapping[PUSHMCAUSE_ADDR] = &pushmcause.reg;
		register_mapping[PUSHMEPC_ADDR] = &pushmepc.reg;
		register_mapping[MSCRATCHCSW_ADDR] = &mscratchcsw.reg;
		register_mapping[MSCRATCHCSWL_ADDR] = &mscratchcswl.reg;
		register_mapping[SLEEPVALUE_ADDR] = &sleepvalue.reg;
		register_mapping[TXEVT_ADDR] = &txevt.reg;
		register_mapping[WFE_ADDR] = &wfe.reg;
		register_mapping[MCFG_INFO_ADDR] = &mcfg_info.reg;
		register_mapping[MICFG_INFO_ADDR] = &micfg_info.reg;
		register_mapping[MDCFG_INFO_ADDR] = &mdcfg_info.reg;
		register_mapping[MTLBCFG_INFO_ADDR] = &mtlbcfg_info.reg;
		register_mapping[MPPICFG_INFO_ADDR] = &mppicfg_info.reg;
		register_mapping[MFIOCFG_INFO_ADDR] = &mfiocfg_info.reg;
		register_mapping[MECC_LOCK_ADDR] = &mecc_lock.reg;
		register_mapping[MECC_CODE_ADDR] = &mecc_code.reg;
		register_mapping[MTLB_CTL_ADDR] = &mtlb_ctl.reg;
	}
};

}  // namespace rv32
