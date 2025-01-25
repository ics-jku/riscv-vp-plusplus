#pragma once

#include "../eclic.h"
#include "iss.h"
#include "nuclei_csr.h"

namespace rv32 {

struct NUCLEI_ISS : public NUCLEI_ISS_BASE, public eclic_interrupt_target {
	ECLIC<NUMBER_INTERRUPTS, MAX_PRIORITY>* eclic = nullptr;

	NUCLEI_ISS(uxlen_t hart_id, bool use_E_base_isa = false) : NUCLEI_ISS_BASE(hart_id, use_E_base_isa){};

	void trigger_eclic_interrupt() override;

	uxlen_t get_csr_value(uxlen_t addr) override;
	void set_csr_value(uxlen_t addr, uxlen_t value) override;

	void prepare_trap(SimulationTrap& e);

	virtual bool has_local_pending_enabled_interrupts() {
		return csrs.mstatus.fields.mie && (!eclic->pending_interrupts.empty());
	}

	void return_from_trap_handler(PrivilegeLevel return_mode) override;
	void switch_to_trap_handler();

	void handle_interrupt() override;
	void handle_trap(SimulationTrap& e) override;
};

}  // namespace rv32
