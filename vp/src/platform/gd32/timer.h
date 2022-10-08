#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "clint_if.h"
#include "util/memory_map.h"

class TIMER : public clint_if, public sc_core::sc_module {
	// Timer unit provides local timer interrupts with
	// memory mapped configuration)
	// similar to CLINT in HiFive

   public:
	TIMER(sc_core::sc_module_name);

	// TODO - what is the right value here? how to calculate?
	// maybe 1000000/(27000000/32768) = 1214
	static constexpr uint64_t scaler = 8000;  // seems about right for snake example

	tlm_utils::simple_target_socket<TIMER> tsock;

	sc_core::sc_event irq_event;

	// according to https://doc.nucleisys.com/nuclei_spec/isa/timer.html
	// mtime and mtimecmp is split in low and high register - maybe modle that too

	RegisterRange regs_mtime;
	RegisterRange regs_mtimecmp;
	RegisterRange regs_msftrst;
	RegisterRange regs_mtimectl;
	RegisterRange regs_msip;
	RegisterRange regs_msip_hart0;
	RegisterRange regs_msip_hart1;
	RegisterRange regs_msip_hart2;
	RegisterRange regs_msip_hart3;
	RegisterRange regs_mtimecmp_hart0;
	RegisterRange regs_mtimecmp_hart1;
	RegisterRange regs_mtimecmp_hart2;
	RegisterRange regs_mtimecmp_hart3;
	RegisterRange regs_mtime_clint;

	IntegerView<uint64_t> mtime;
	IntegerView<uint64_t> mtimecmp;
	IntegerView<uint32_t> msftrst;
	IntegerView<uint32_t> mtimectl;
	IntegerView<uint32_t> msip;
	IntegerView<uint32_t> msip_hart0;
	IntegerView<uint32_t> msip_hart1;
	IntegerView<uint32_t> msip_hart2;
	IntegerView<uint32_t> msip_hart3;
	IntegerView<uint64_t> mtimecmp_hart0;
	IntegerView<uint64_t> mtimecmp_hart1;
	IntegerView<uint64_t> mtimecmp_hart2;
	IntegerView<uint64_t> mtimecmp_hart3;
	IntegerView<uint64_t> mtime_clint;

	std::vector<RegisterRange *> register_ranges{
	    &regs_mtime,          &regs_mtimecmp,       &regs_msftrst,        &regs_mtimectl,   &regs_msip,
	    &regs_msip_hart0,     &regs_msip_hart1,     &regs_msip_hart2,     &regs_msip_hart3, &regs_mtimecmp_hart0,
	    &regs_mtimecmp_hart1, &regs_mtimecmp_hart2, &regs_mtimecmp_hart3, &regs_mtime_clint};

	SC_HAS_PROCESS(TIMER);

	bool pre_read_mtime(RegisterRange::ReadInfo t);
	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
	uint64_t update_and_get_mtime(void) override;
	void run();
};
