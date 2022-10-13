#pragma once

#include "iss.h"
#include "nuclei_csr.h"

namespace rv32 {

struct NUCLEI_ISS : public ISS {
	nuclei_csr_table csrs;

	NUCLEI_ISS(uint32_t hart_id, bool use_E_base_isa = false) : ISS(hart_id, use_E_base_isa){};

	nuclei_csr_table* get_csr_table() override;
	uint32_t get_csr_value(uint32_t addr) override;
	void set_csr_value(uint32_t addr, uint32_t value) override;
	void trigger_external_interrupt(PrivilegeLevel level) override;
};

}  // namespace rv32
