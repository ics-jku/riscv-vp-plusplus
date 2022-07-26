#include "nuclei_iss.h"

#include "nuclei_csr.h"

using namespace rv32;

uint32_t NUCLEI_ISS::get_csr_value(uint32_t addr) {
	// validate_csr_counter_read_access_rights(addr);
	// auto read = [=](auto &x, uint32_t mask) { return x.reg & mask; };

	using namespace csr;

	switch (addr) {
		case MTVT_ADDR:
		case MNXTI_ADDR:
		case MINTSTATUS_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MILM_CTL_ADDR:
		case MDLM_CTL_ADDR:
		case MECC_CODE_ADDR:
		case MNVEC_ADDR:
		case MSUBM_ADDR:
		case MDCAUSE_ADDR:
		case MCACHE_CTL_ADDR:
		case MMISC_CTL_ADDR:
		case MSAVESTATUS_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
		case MTLB_CTL_ADDR:
		case MECC_LOCK_ADDR:
		case PUSHMSUBM_ADDR:
		case MTVT2_ADDR:
		case JALMNXTI_ADDR:
		case PUSHMCAUSE_ADDR:
		case PUSHMEPC_ADDR:
		case MPPICFG_INFO_ADDR:
		case MFIOCFG_INFO_ADDR:
		case SLEEPVALUE_ADDR:
		case TXEVT_ADDR:
		case WFE_ADDR:
		case MICFG_INFO_ADDR:
		case MDCFG_INFO_ADDR:
		case MCFG_INFO_ADDR:
		case MTLBCFG_INFO_ADDR:
			return 0;
		default:
			return ISS::get_csr_value(addr);
	}
}

void NUCLEI_ISS::set_csr_value(uint32_t addr, uint32_t value) {
	// auto read = [=](auto &x, uint32_t mask) { return x.reg & mask; };

	using namespace csr;

	switch (addr) {
		case MTVT_ADDR:
		case MNXTI_ADDR:
		case MINTSTATUS_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MILM_CTL_ADDR:
		case MDLM_CTL_ADDR:
		case MECC_CODE_ADDR:
		case MNVEC_ADDR:
		case MSUBM_ADDR:
		case MDCAUSE_ADDR:
		case MCACHE_CTL_ADDR:
		case MMISC_CTL_ADDR:
		case MSAVESTATUS_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
		case MTLB_CTL_ADDR:
		case MECC_LOCK_ADDR:
		case PUSHMSUBM_ADDR:
		case MTVT2_ADDR:
		case JALMNXTI_ADDR:
		case PUSHMCAUSE_ADDR:
		case PUSHMEPC_ADDR:
		case MPPICFG_INFO_ADDR:
		case MFIOCFG_INFO_ADDR:
		case SLEEPVALUE_ADDR:
		case TXEVT_ADDR:
		case WFE_ADDR:
		case MICFG_INFO_ADDR:
		case MDCFG_INFO_ADDR:
		case MCFG_INFO_ADDR:
		case MTLBCFG_INFO_ADDR:
			break;
		default:
			ISS::set_csr_value(addr, value);
	}
}
