#pragma once

#include "csr.h"

namespace rv32 {

namespace csr {
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
	// TODO
};

}  // namespace rv32
