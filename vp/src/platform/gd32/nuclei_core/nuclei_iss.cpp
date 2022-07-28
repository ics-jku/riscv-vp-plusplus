#include "nuclei_iss.h"

#include "nuclei_csr.h"

using namespace rv32;

nuclei_csr_table *NUCLEI_ISS::get_csr_table() {
	return &csrs;
}

uint32_t NUCLEI_ISS::get_csr_value(uint32_t addr) {
	auto read = [=](auto &x, uint32_t mask) { return x.reg & mask; };

	using namespace csr;

	switch (addr) {
		case MTVEC_ADDR:
			// TODO
			return get_csr_table()->default_read32(addr);
			break;
		case MCAUSE_ADDR:
			return read(get_csr_table()->nuclei_mcause, MCAUSE_MASK);

		case MINTSTATUS_ADDR:
			return read(get_csr_table()->milm_ctl, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return read(get_csr_table()->milm_ctl, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return read(get_csr_table()->milm_ctl, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return read(get_csr_table()->milm_ctl, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return read(get_csr_table()->milm_ctl, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return read(get_csr_table()->milm_ctl, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return read(get_csr_table()->milm_ctl, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return read(get_csr_table()->milm_ctl, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return read(get_csr_table()->milm_ctl, TXEVT_MASK);
		case WFE_ADDR:
			return read(get_csr_table()->milm_ctl, WFE_MASK);
		case MICFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return read(get_csr_table()->milm_ctl, MTLBCFG_INFO_MASK);

		case MTVT_ADDR:
		case MNXTI_ADDR:
		case MNVEC_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
		case JALMNXTI_ADDR:
		case PUSHMCAUSE_ADDR:
		case PUSHMEPC_ADDR:
		case PUSHMSUBM_ADDR:
			return get_csr_table()->default_read32(addr);
		default:
			return ISS::get_csr_value(addr);
	}
}

void NUCLEI_ISS::set_csr_value(uint32_t addr, uint32_t value) {
	auto write = [=](auto &x, uint32_t mask) { x.reg = (x.reg & ~mask) | (value & mask); };

	using namespace csr;

	switch (addr) {
		case MTVEC_ADDR:
			// TODO
			get_csr_table()->default_write32(addr, value);
			break;
		case MCAUSE_ADDR:
			return write(get_csr_table()->nuclei_mcause, MCAUSE_MASK);

		case MINTSTATUS_ADDR:
			return write(get_csr_table()->milm_ctl, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return write(get_csr_table()->milm_ctl, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return write(get_csr_table()->milm_ctl, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return write(get_csr_table()->milm_ctl, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return write(get_csr_table()->milm_ctl, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return write(get_csr_table()->milm_ctl, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return write(get_csr_table()->milm_ctl, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return write(get_csr_table()->milm_ctl, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return write(get_csr_table()->milm_ctl, TXEVT_MASK);
		case WFE_ADDR:
			return write(get_csr_table()->milm_ctl, WFE_MASK);
		case MICFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return write(get_csr_table()->milm_ctl, MTLBCFG_INFO_MASK);

		case MTVT_ADDR:
		case MNXTI_ADDR:
		case MNVEC_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
		case JALMNXTI_ADDR:
		case PUSHMCAUSE_ADDR:
		case PUSHMEPC_ADDR:
		case PUSHMSUBM_ADDR:
			get_csr_table()->default_write32(addr, value);
			break;
		default:
			ISS::set_csr_value(addr, value);
	}
}
