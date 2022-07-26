#pragma once

#include "iss.h"

namespace rv32 {

struct NUCLEI_ISS : public ISS {
	NUCLEI_ISS(uint32_t hart_id, bool use_E_base_isa = false) : ISS(hart_id, use_E_base_isa){};

	uint32_t get_csr_value(uint32_t addr);
	void set_csr_value(uint32_t addr, uint32_t value);
};

}  // namespace rv32
