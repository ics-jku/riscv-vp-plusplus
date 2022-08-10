#ifndef RISCV_VP_ECLIC_H
#define RISCV_VP_ECLIC_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/memory_map.h"
#include "util/nuclei_memory_map.h"
#include "util/tlm_map.h"

template <unsigned NumberInterrupts, uint32_t MaxPriority>
class ECLIC : public sc_core::sc_module {
   public:
	tlm_utils::simple_target_socket<ECLIC> tsock;

	RegisterRange regs_cliccfg{0x0, 1};
	IntegerView<uint8_t> cliccfg{regs_cliccfg};

	RegisterRange regs_clicinfo{0x4, 4};
	IntegerView<uint32_t> clicinfo{regs_clicinfo};

	RegisterRange regs_mth{0xb, 1};
	IntegerView<uint8_t> mth{regs_mth};

	ModRegisterRange<4> regs_clicintip{0x1000, NumberInterrupts};
	ArrayView<uint8_t> clicintip{regs_clicintip};

	ModRegisterRange<4> regs_clicintie{0x1001, NumberInterrupts};
	ArrayView<uint8_t> clicintie{regs_clicintie};

	ModRegisterRange<4> regs_clicintattr{0x1002, NumberInterrupts};
	ArrayView<uint8_t> clicintattr{regs_clicintattr};

	ModRegisterRange<4> regs_clicintctl{0x1003, NumberInterrupts};
	ArrayView<uint8_t> clicintctl{regs_clicintctl};

	std::vector<RegisterRange *> register_ranges{&regs_cliccfg,   &regs_clicinfo,    &regs_mth,       &regs_clicintip,
	                                             &regs_clicintie, &regs_clicintattr, &regs_clicintctl};

	ECLIC(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &ECLIC::transport);

		regs_clicinfo.readonly = true;
		clicinfo.write(clicinfo.read() | NumberInterrupts);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		vp::mm::route("ECLIC", register_ranges, trans, delay);
	}
};

#endif  // RISCV_VP_ECLIC_H
