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
			return read(get_csr_table()->mintstatus, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return read(get_csr_table()->milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return read(get_csr_table()->mdlm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return read(get_csr_table()->mecc_code, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return read(get_csr_table()->msubm, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return read(get_csr_table()->mdcause, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return read(get_csr_table()->mcache_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return read(get_csr_table()->mmisc_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return read(get_csr_table()->msavestatus, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return read(get_csr_table()->mtlb_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return read(get_csr_table()->mecc_lock, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return read(get_csr_table()->mtvt2, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return read(get_csr_table()->mppicfg_info, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return read(get_csr_table()->mfiocfg_info, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return read(get_csr_table()->sleepvalue, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return read(get_csr_table()->txevt, TXEVT_MASK);
		case WFE_ADDR:
			return read(get_csr_table()->wfe, WFE_MASK);
		case MICFG_INFO_ADDR:
			return read(get_csr_table()->micfg_info, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return read(get_csr_table()->mdcfg_info, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return read(get_csr_table()->mcfg_info, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return read(get_csr_table()->mtlb_ctl, MTLBCFG_INFO_MASK);

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
			return write(get_csr_table()->mintstatus, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return write(get_csr_table()->milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return write(get_csr_table()->mdlm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return write(get_csr_table()->mecc_code, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return write(get_csr_table()->msubm, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return write(get_csr_table()->mdcause, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return write(get_csr_table()->mcache_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return write(get_csr_table()->mmisc_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return write(get_csr_table()->msavestatus, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return write(get_csr_table()->mtlb_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return write(get_csr_table()->mecc_lock, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return write(get_csr_table()->mtvt2, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return write(get_csr_table()->mppicfg_info, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return write(get_csr_table()->mfiocfg_info, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return write(get_csr_table()->sleepvalue, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return write(get_csr_table()->txevt, TXEVT_MASK);
		case WFE_ADDR:
			return write(get_csr_table()->wfe, WFE_MASK);
		case MICFG_INFO_ADDR:
			return write(get_csr_table()->micfg_info, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return write(get_csr_table()->mdcfg_info, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return write(get_csr_table()->mcfg_info, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return write(get_csr_table()->mtlb_ctl, MTLBCFG_INFO_MASK);

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
			// TODO - Non-Vectored Interrupt
		case PUSHMCAUSE_ADDR:
			// TODO - Non-Vectored Interrupt
		case PUSHMEPC_ADDR:
			// TODO - Non-Vectored Interrupt
		case PUSHMSUBM_ADDR:
			// TODO - Non-Vectored Interrupt
			get_csr_table()->default_write32(addr, value);
			break;
		default:
			ISS::set_csr_value(addr, value);
	}
}

void NUCLEI_ISS::trigger_external_interrupt(uint32_t id) {
	clic_irq = true;
	irq_id = id;
}

void NUCLEI_ISS::clear_external_interrupt(uint32_t irq_id) {
	// TODO
}

void NUCLEI_ISS::return_from_trap_handler(PrivilegeLevel return_mode) {
	// TODO - still a few things missing

	get_csr_table()->msubm.fields.typ = get_csr_table()->msubm.fields.ptyp;

	get_csr_table()->mintstatus.fields.mil = get_csr_table()->nuclei_mcause.fields.mpil;

	prv = get_csr_table()->mstatus.fields.mpp;
	get_csr_table()->mstatus.fields.mie = get_csr_table()->mstatus.fields.mpie;
	get_csr_table()->mstatus.fields.mpie = 1;

	pc = get_csr_table()->mepc.reg;
}

void NUCLEI_ISS::switch_to_trap_handler() {
	// TODO - still a few things missing

	// update privlege mode
	auto pp = prv;
	prv = MachineMode;

	// update machine sub-mode
	get_csr_table()->msubm.fields.ptyp = get_csr_table()->msubm.fields.typ;
	get_csr_table()->msubm.fields.typ = get_csr_table()->msubm.Interrupt;

	// update mepc
	get_csr_table()->mepc.reg = pc;

	// update mstatus
	get_csr_table()->mstatus.fields.mpie = get_csr_table()->mstatus.fields.mie;
	get_csr_table()->mstatus.fields.mie = 0;
	get_csr_table()->mstatus.fields.mpp = pp;

	// update mcause
	get_csr_table()->nuclei_mcause.fields.mpil = get_csr_table()->mintstatus.fields.mil;

	auto mode = eclic->clicintattr[irq_id] & 1;
	if (mode == 0) {
		// non-vectored
		if (get_csr_table()->mtvt2.fields.mtvt2en) {
			// use mtvt2
			std::cout << "use mtvt2: " << (get_csr_table()->mtvt2.fields.cmmon_code_entry << 2) << std::endl;
			pc = get_csr_table()->mtvt2.fields.cmmon_code_entry << 2;
		} else {
			// use mtvec
			// pc = get_csr_table()->nuclei_mtvec.fields.addr; // not sure if right. docu is conflicting
		}
	} else {
		// vectored
	}

	if (pc == 0) {
		static bool once = true;
		if (once)
			std::cout << "[ISS] Warn: Taking trap handler in machine mode to 0x0, this is probably an error."
			          << std::endl;
		once = false;
	}
}

void NUCLEI_ISS::run_step() {
	assert(regs.read(0) == 0);

	// speeds up the execution performance (non debug mode) significantly by
	// checking the additional flag first
	if (debug_mode && (breakpoints.find(pc) != breakpoints.end())) {
		status = CoreExecStatus::HitBreakpoint;
		return;
	}

	last_pc = pc;
	try {
		exec_step();

		if (clic_irq) {
			clic_irq = false;
			switch_to_trap_handler();
		}
	} catch (SimulationTrap &e) {
		// TODO
		if (trace)
			std::cout << "take trap " << e.reason << ", mtval=" << e.mtval << std::endl;
	}

	// NOTE: writes to zero register are supposedly allowed but must be ignored
	// (reset it after every instruction, instead of checking *rd != zero*
	// before every register write)
	regs.regs[regs.zero] = 0;

	// Do not use a check *pc == last_pc* here. The reason is that due to
	// interrupts *pc* can be set to *last_pc* accidentally (when jumping back
	// to *mepc*).
	if (shall_exit)
		status = CoreExecStatus::Terminated;

	performance_and_sync_update(op);
}
