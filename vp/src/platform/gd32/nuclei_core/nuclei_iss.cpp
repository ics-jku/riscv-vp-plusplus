#include "nuclei_iss.h"

#include <boost/lexical_cast.hpp>

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

		case JALMNXTI_ADDR: {
			std::lock_guard<std::mutex> guard(eclic->pending_interrupts_mutex);
			if (!eclic->pending_interrupts.empty()) {
				const auto id = eclic->pending_interrupts.top().id;
				if (eclic->clicintattr[id] & 1)  // vectored interrupt must not be handled here
					return 0;
				if (!(eclic->clicintip[id] & 1)) {
					// check if interrupt is still pending, if not remove from queue
					eclic->pending_interrupts.pop();
					return 0;
				}

				get_csr_table()->mstatus.fields.mie = 1;
				get_csr_table()->nuclei_mcause.fields.exccode = id;
				eclic->pending_interrupts.pop();
				eclic->clicintip[id] = 0;
				pc = instr_mem->load_instr(get_csr_table()->mtvt.reg + id * 4);
				return last_pc;
			} else {
				if (get_csr_table()->msubm.fields.typ == get_csr_table()->msubm.Interrupt)
					get_csr_table()->mstatus.fields.mie = 0;
				return 0;
			}
		}

		case MNXTI_ADDR:
			/* A read of the mnxti CSR returns either zero,
			    indicating there is no suitable interrupt to service
			    or that the highest ranked interrupt is SHV
			    or that the system is not in a CLIC mode,
			or returns a non-zero address
			    of the entry in the trap handler table for software trap vectoring. */
			// TODO

		case MTVT_ADDR:
		case MNVEC_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
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

		case PUSHMCAUSE_ADDR: {
			const uint32_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, get_csr_table()->nuclei_mcause.reg);
			break;
		}
		case PUSHMEPC_ADDR: {
			const uint32_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, get_csr_table()->mepc.reg);
			break;
		}
		case PUSHMSUBM_ADDR: {
			const uint32_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, get_csr_table()->msubm.reg);
			break;
		}

		case JALMNXTI_ADDR:
			return;

		case MNXTI_ADDR:
			// TODO

		case MTVT_ADDR:
		case MNVEC_ADDR:
		case MSCRATCHCSW_ADDR:
		case MSCRATCHCSWL_ADDR:
		case MSAVEEPC1_ADDR:
		case MSAVECAUSE1_ADDR:
		case MSAVEEPC2_ADDR:
		case MSAVECAUSE2_ADDR:
			get_csr_table()->default_write32(addr, value);
			break;
		default:
			ISS::set_csr_value(addr, value);
	}
}

// this is more or less just copy and paste from "iss.cpp" -> Code Duplication
// but i think still a better solution then dealing with complicated inheritance
// for the mcause/nuclei_mcause register
void NUCLEI_ISS::prepare_trap(SimulationTrap &e) {
	// undo any potential pc update (for traps the pc should point to the originating instruction and not it's
	// successor)
	pc = last_pc;
	unsigned exc_bit = (1 << e.reason);

	// 1) machine mode execution takes any traps, independent of delegation setting
	// 2) non-delegated traps are processed in machine mode, independent of current execution mode
	if (prv == MachineMode || !(exc_bit & get_csr_table()->medeleg.reg)) {
		get_csr_table()->nuclei_mcause.fields.interrupt = 0;
		get_csr_table()->nuclei_mcause.fields.exccode = e.reason;
		get_csr_table()->mtval.reg = boost::lexical_cast<uint32_t>(e.mtval);
		return;
	}

	// see above machine mode comment
	if (prv == SupervisorMode || !(exc_bit & get_csr_table()->sedeleg.reg)) {
		get_csr_table()->scause.fields.interrupt = 0;
		get_csr_table()->scause.fields.exception_code = e.reason;
		get_csr_table()->stval.reg = boost::lexical_cast<uint32_t>(e.mtval);
		return;
	}

	assert(prv == UserMode && (exc_bit & get_csr_table()->medeleg.reg) && (exc_bit & get_csr_table()->sedeleg.reg));
	get_csr_table()->ucause.fields.interrupt = 0;
	get_csr_table()->ucause.fields.exception_code = e.reason;
	get_csr_table()->utval.reg = boost::lexical_cast<uint32_t>(e.mtval);
	return;
}

void NUCLEI_ISS::return_from_trap_handler(PrivilegeLevel return_mode) {
	// update privlege mode
	prv = get_csr_table()->mstatus.fields.mpp;
	get_csr_table()->mstatus.fields.mpp = 0;  // not in the docs but real device seems to do that

	// update machine sub-mode
	get_csr_table()->msubm.fields.typ = get_csr_table()->msubm.fields.ptyp;

	// update mstatus
	get_csr_table()->mstatus.fields.mie = get_csr_table()->mstatus.fields.mpie;

	// mirror mcause/mstatus MPIE & MPP fields
	get_csr_table()->nuclei_mcause.fields.mpie = get_csr_table()->mstatus.fields.mpie;
	get_csr_table()->nuclei_mcause.fields.mpp = get_csr_table()->mstatus.fields.mpp;

	if (get_csr_table()->nuclei_mcause.fields.interrupt) {
		get_csr_table()->mintstatus.fields.mil = get_csr_table()->nuclei_mcause.fields.mpil;
	}
	// update pc
	pc = get_csr_table()->mepc.reg;
}

void NUCLEI_ISS::switch_to_trap_handler() {
	// update privlege mode
	const auto pp = prv;
	prv = MachineMode;

	// update mepc
	get_csr_table()->mepc.reg = pc;

	// update mstatus
	get_csr_table()->mstatus.fields.mpie = get_csr_table()->mstatus.fields.mie;
	get_csr_table()->mstatus.fields.mie = 0;
	get_csr_table()->mstatus.fields.mpp = pp;

	// mirror mcause/mstatus MPIE & MPP fields
	get_csr_table()->nuclei_mcause.fields.mpie = get_csr_table()->mstatus.fields.mpie;
	get_csr_table()->nuclei_mcause.fields.mpp = get_csr_table()->mstatus.fields.mpp;

	get_csr_table()->msubm.fields.ptyp = get_csr_table()->msubm.fields.typ;

	if (get_csr_table()->nuclei_mcause.fields.interrupt) {
		// Interrupt
		// update machine sub-mode
		get_csr_table()->msubm.fields.typ = get_csr_table()->msubm.Interrupt;

		// update mcause
		get_csr_table()->nuclei_mcause.fields.mpil = get_csr_table()->mintstatus.fields.mil;

		eclic->pending_interrupts_mutex.lock();
		const auto id = eclic->pending_interrupts.top().id;
		get_csr_table()->nuclei_mcause.fields.exccode = id;

		if (eclic->clicintattr[id] & 1) {
			// vectored
			if (!(eclic->clicintip[id] & 1)) {
				// check if interrupt is still pending, if not remove from queue
				eclic->pending_interrupts.pop();
				eclic->pending_interrupts_mutex.unlock();
				return return_from_trap_handler(MachineMode);
			}
			get_csr_table()->nuclei_mcause.fields.minhv = 1;
			pc = instr_mem->load_instr(get_csr_table()->mtvt.reg + id * 4);
			get_csr_table()->nuclei_mcause.fields.minhv = 0;
			eclic->pending_interrupts.pop();
		} else {
			// non-vectored
			if (get_csr_table()->mtvt2.fields.mtvt2en) {
				// use mtvt2
				pc = get_csr_table()->mtvt2.fields.cmmon_code_entry << 2;
			} else {
				// use mtvec
				pc = get_csr_table()->nuclei_mtvec.get_base_address();
			}
		}
		eclic->pending_interrupts_mutex.unlock();
	} else {
		// Exception
		// update machine sub-mode
		get_csr_table()->msubm.fields.typ = get_csr_table()->msubm.Exception;

		pc = get_csr_table()->nuclei_mtvec.get_base_address();
	}

	if (pc == 0) {
		static bool once = true;
		if (once)
			std::cout << "[ISS] Warn: Taking trap handler in machine mode to 0x0, this is probably an error."
			          << std::endl;
		once = false;
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

		bool pending = !eclic->pending_interrupts.empty() && get_csr_table()->mstatus.fields.mie;

		// Interrupt preemption. Only supported for non-vectored interrupts.
		// Current running interrupt will only be preempted by a higher non-vectored interrupt.
		if (pending && get_csr_table()->msubm.fields.typ == get_csr_table()->msubm.Interrupt) {
			const auto current_intr_id = get_csr_table()->nuclei_mcause.fields.exccode;
			const auto pending_intr = eclic->pending_interrupts.top();
			Interrupt current_intr =
			    Interrupt(current_intr_id, eclic->clicintctl[current_intr_id], eclic->clicinfo, eclic->cliccfg);
			pending = (eclic->clicintattr[pending_intr.id] & 1) == 0 && pending_intr.level > current_intr.level;
		}
		if (pending) {
			get_csr_table()->nuclei_mcause.fields.interrupt = 1;
			switch_to_trap_handler();
		}
	} catch (SimulationTrap &e) {
		if (trace)
			std::cout << "take trap " << e.reason << ", mtval=" << e.mtval << std::endl;
		get_csr_table()->nuclei_mcause.fields.interrupt = 0;
		prepare_trap(e);
		switch_to_trap_handler();
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
