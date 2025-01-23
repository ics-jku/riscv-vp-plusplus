#include "nuclei_iss.h"

#include <boost/lexical_cast.hpp>

#include "nuclei_csr.h"

using namespace rv32;

uxlen_t NUCLEI_ISS::get_csr_value(uxlen_t addr) {
	auto read = [=](auto &x, uxlen_t mask) { return x.reg & mask; };

	using namespace csr;

	switch (addr) {
		case MTVEC_ADDR:
			// TODO
			return csrs.default_read32(addr);
			break;
		case MCAUSE_ADDR:
			return read(csrs.nuclei_mcause, MCAUSE_MASK);

		case MINTSTATUS_ADDR:
			return read(csrs.mintstatus, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return read(csrs.milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return read(csrs.mdlm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return read(csrs.mecc_code, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return read(csrs.msubm, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return read(csrs.mdcause, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return read(csrs.mcache_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return read(csrs.mmisc_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return read(csrs.msavestatus, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return read(csrs.mtlb_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return read(csrs.mecc_lock, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return read(csrs.mtvt2, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return read(csrs.mppicfg_info, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return read(csrs.mfiocfg_info, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return read(csrs.sleepvalue, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return read(csrs.txevt, TXEVT_MASK);
		case WFE_ADDR:
			return read(csrs.wfe, WFE_MASK);
		case MICFG_INFO_ADDR:
			return read(csrs.micfg_info, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return read(csrs.mdcfg_info, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return read(csrs.mcfg_info, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return read(csrs.mtlb_ctl, MTLBCFG_INFO_MASK);

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

				csrs.mstatus.fields.mie = 1;
				csrs.nuclei_mcause.fields.exccode = id;
				eclic->pending_interrupts.pop();
				eclic->clicintip[id] = 0;
				pc = instr_mem->load_instr(csrs.mtvt.reg + id * 4);
				return last_pc;
			} else {
				if (csrs.msubm.fields.typ == csrs.msubm.Interrupt)
					csrs.mstatus.fields.mie = 0;
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
			return csrs.default_read32(addr);
		default:
			return NUCLEI_ISS_BASE::get_csr_value(addr);
	}
}

void NUCLEI_ISS::set_csr_value(uxlen_t addr, uxlen_t value) {
	auto write = [=](auto &x, uxlen_t mask) { x.reg = (x.reg & ~mask) | (value & mask); };

	using namespace csr;

	switch (addr) {
		case MTVEC_ADDR:
			// TODO
			csrs.default_write32(addr, value);
			break;
		case MCAUSE_ADDR:
			return write(csrs.nuclei_mcause, MCAUSE_MASK);

		case MINTSTATUS_ADDR:
			return write(csrs.mintstatus, MINTSTATUS_MASK);
		case MILM_CTL_ADDR:
			return write(csrs.milm_ctl, MILM_CTL_MASK);
		case MDLM_CTL_ADDR:
			return write(csrs.mdlm_ctl, MDLM_CTL_MASK);
		case MECC_CODE_ADDR:
			return write(csrs.mecc_code, MECC_CODE_MASK);
		case MSUBM_ADDR:
			return write(csrs.msubm, MSUBM_MASK);
		case MDCAUSE_ADDR:
			return write(csrs.mdcause, MDCAUSE_MASK);
		case MCACHE_CTL_ADDR:
			return write(csrs.mcache_ctl, MCACHE_CTL_MASK);
		case MMISC_CTL_ADDR:
			return write(csrs.mmisc_ctl, MMISC_CTL_MASK);
		case MSAVESTATUS_ADDR:
			return write(csrs.msavestatus, MSAVESTATUS_MASK);
		case MTLB_CTL_ADDR:
			return write(csrs.mtlb_ctl, MTLB_CTL_MASK);
		case MECC_LOCK_ADDR:
			return write(csrs.mecc_lock, MECC_LOCK_MASK);
		case MTVT2_ADDR:
			return write(csrs.mtvt2, MTVT2_MASK);
		case MPPICFG_INFO_ADDR:
			return write(csrs.mppicfg_info, MPPICFG_INFO_MASK);
		case MFIOCFG_INFO_ADDR:
			return write(csrs.mfiocfg_info, MFIOCFG_INFO_MASK);
		case SLEEPVALUE_ADDR:
			return write(csrs.sleepvalue, SLEEPVALUE_MASK);
		case TXEVT_ADDR:
			return write(csrs.txevt, TXEVT_MASK);
		case WFE_ADDR:
			return write(csrs.wfe, WFE_MASK);
		case MICFG_INFO_ADDR:
			return write(csrs.micfg_info, MICFG_INFO_MASK);
		case MDCFG_INFO_ADDR:
			return write(csrs.mdcfg_info, MDCFG_INFO_MASK);
		case MCFG_INFO_ADDR:
			return write(csrs.mcfg_info, MCFG_INFO_MASK);
		case MTLBCFG_INFO_ADDR:
			return write(csrs.mtlb_ctl, MTLBCFG_INFO_MASK);

		case PUSHMCAUSE_ADDR: {
			const uxlen_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, csrs.nuclei_mcause.reg);
			break;
		}
		case PUSHMEPC_ADDR: {
			const uxlen_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, csrs.mepc.reg);
			break;
		}
		case PUSHMSUBM_ADDR: {
			const uxlen_t mem_addr = regs[RegFile::sp] + value * 4;
			trap_check_addr_alignment<4, false>(mem_addr);
			mem->store_word(mem_addr, csrs.msubm.reg);
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
			csrs.default_write32(addr, value);
			break;
		default:
			NUCLEI_ISS_BASE::set_csr_value(addr, value);
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
	if (prv == MachineMode || !(exc_bit & csrs.medeleg.reg)) {
		csrs.nuclei_mcause.fields.interrupt = 0;
		csrs.nuclei_mcause.fields.exccode = e.reason;
		csrs.mtval.reg = boost::lexical_cast<uxlen_t>(e.mtval);
		return;
	}

	// see above machine mode comment
	if (prv == SupervisorMode || !(exc_bit & csrs.sedeleg.reg)) {
		csrs.scause.fields.interrupt = 0;
		csrs.scause.fields.exception_code = e.reason;
		csrs.stval.reg = boost::lexical_cast<uxlen_t>(e.mtval);
		return;
	}

	assert(prv == UserMode && (exc_bit & csrs.medeleg.reg) && (exc_bit & csrs.sedeleg.reg));
	csrs.ucause.fields.interrupt = 0;
	csrs.ucause.fields.exception_code = e.reason;
	csrs.utval.reg = boost::lexical_cast<uxlen_t>(e.mtval);
	return;
}

void NUCLEI_ISS::return_from_trap_handler(PrivilegeLevel return_mode) {
	// update privlege mode
	prv = csrs.mstatus.fields.mpp;
	csrs.mstatus.fields.mpp = 0;  // not in the docs but real device seems to do that

	// update machine sub-mode
	csrs.msubm.fields.typ = csrs.msubm.fields.ptyp;

	// update mstatus
	csrs.mstatus.fields.mie = csrs.mstatus.fields.mpie;

	// mirror mcause/mstatus MPIE & MPP fields
	csrs.nuclei_mcause.fields.mpie = csrs.mstatus.fields.mpie;
	csrs.nuclei_mcause.fields.mpp = csrs.mstatus.fields.mpp;

	if (csrs.nuclei_mcause.fields.interrupt) {
		csrs.mintstatus.fields.mil = csrs.nuclei_mcause.fields.mpil;
	}
	// update pc
	pc = csrs.mepc.reg;
}

void NUCLEI_ISS::switch_to_trap_handler() {
	// update privlege mode
	const auto pp = prv;
	prv = MachineMode;

	// update mepc
	csrs.mepc.reg = pc;

	// update mstatus
	csrs.mstatus.fields.mpie = csrs.mstatus.fields.mie;
	csrs.mstatus.fields.mie = 0;
	csrs.mstatus.fields.mpp = pp;

	// mirror mcause/mstatus MPIE & MPP fields
	csrs.nuclei_mcause.fields.mpie = csrs.mstatus.fields.mpie;
	csrs.nuclei_mcause.fields.mpp = csrs.mstatus.fields.mpp;

	csrs.msubm.fields.ptyp = csrs.msubm.fields.typ;

	if (csrs.nuclei_mcause.fields.interrupt) {
		// Interrupt
		// update machine sub-mode
		csrs.msubm.fields.typ = csrs.msubm.Interrupt;

		// update mcause
		csrs.nuclei_mcause.fields.mpil = csrs.mintstatus.fields.mil;

		eclic->pending_interrupts_mutex.lock();
		const auto id = eclic->pending_interrupts.top().id;
		csrs.nuclei_mcause.fields.exccode = id;

		if (eclic->clicintattr[id] & 1) {
			// vectored
			if (!(eclic->clicintip[id] & 1)) {
				// check if interrupt is still pending, if not remove from queue
				eclic->pending_interrupts.pop();
				eclic->pending_interrupts_mutex.unlock();
				return return_from_trap_handler(MachineMode);
			}
			csrs.nuclei_mcause.fields.minhv = 1;
			pc = instr_mem->load_instr(csrs.mtvt.reg + id * 4);
			csrs.nuclei_mcause.fields.minhv = 0;
			eclic->pending_interrupts.pop();
		} else {
			// non-vectored
			if (csrs.mtvt2.fields.mtvt2en) {
				// use mtvt2
				pc = csrs.mtvt2.fields.cmmon_code_entry << 2;
			} else {
				// use mtvec
				pc = csrs.nuclei_mtvec.get_base_address();
			}
		}
		eclic->pending_interrupts_mutex.unlock();
	} else {
		// Exception
		// update machine sub-mode
		csrs.msubm.fields.typ = csrs.msubm.Exception;

		pc = csrs.nuclei_mtvec.get_base_address();
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

void NUCLEI_ISS::handle_interrupt() {
	bool pending = !eclic->pending_interrupts.empty() && csrs.mstatus.fields.mie;

	// Interrupt preemption. Only supported for non-vectored interrupts.
	// Current running interrupt will only be preempted by a higher non-vectored interrupt.
	if (pending && csrs.msubm.fields.typ == csrs.msubm.Interrupt) {
		const auto current_intr_id = csrs.nuclei_mcause.fields.exccode;
		const auto pending_intr = eclic->pending_interrupts.top();
		Interrupt current_intr =
		    Interrupt(current_intr_id, eclic->clicintctl[current_intr_id], eclic->clicinfo, eclic->cliccfg);
		pending = (eclic->clicintattr[pending_intr.id] & 1) == 0 && pending_intr.level > current_intr.level;
	}
	if (pending) {
		csrs.nuclei_mcause.fields.interrupt = 1;
		switch_to_trap_handler();
	}
}

void NUCLEI_ISS::handle_trap(SimulationTrap &e) {
	if (trace) {
		std::cout << "take trap " << e.reason << ", mtval=" << e.mtval << std::endl;
	}
	csrs.nuclei_mcause.fields.interrupt = 0;
	prepare_trap(e);
	switch_to_trap_handler();
}
