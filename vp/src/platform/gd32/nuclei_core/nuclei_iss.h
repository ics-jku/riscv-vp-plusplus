#pragma once

#include "../eclic.h"
#include "iss.h"
#include "nuclei_csr.h"

namespace rv32 {

/* see bottom of vp/core/rv32/iss.cpp */
using NUCLEI_ISS_BASE = ISS_T<nuclei_csr_table>;

struct NUCLEI_ISS : public NUCLEI_ISS_BASE {
	ECLIC<NUMBER_INTERRUPTS, MAX_PRIORITY>* eclic = nullptr;

	NUCLEI_ISS(uxlen_t hart_id, bool use_E_base_isa = false) : NUCLEI_ISS_BASE(hart_id, use_E_base_isa){};

	uxlen_t get_csr_value(uxlen_t addr) override;
	void set_csr_value(uxlen_t addr, uxlen_t value) override;

	void prepare_trap(SimulationTrap& e);

	void return_from_trap_handler(PrivilegeLevel return_mode) override;
	void switch_to_trap_handler();

	void run_step() override;
};

}  // namespace rv32
