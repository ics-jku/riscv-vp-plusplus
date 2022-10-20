#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "clint_if.h"
#include "nuclei_core/nuclei_irq_if.h"
#include "util/memory_map.h"

#define TIMER_INT_NUM 7
#define SW_INT_NUM 3

class TIMER : public clint_if, public sc_core::sc_module {
	// Timer unit provides local timer interrupts with
	// memory mapped configuration)
	// similar to CLINT in HiFive

   public:
	TIMER(sc_core::sc_module_name);

	/*
	    This is a bit complicated. Still unsure what the right value is. The Nuclei Core uses a frequency of
	    108MHz stored in the variable "SystemCoreClock". The Nuclei SDK defines a function _premain_init()
	    (file "system_gd32vf103.c") which is called before the regular main function. In this funciton the
	    frequency is measured, (re)calculated (using mtime register, cycle count & the 108 MHz) & reassigned
	    to the variable. The "real" device uses an oscilator for the mtime ticks, the VP, however, uses the
	    SystemC simulation time - this results in wrong SystemCoreClock value. The scaler value should  correct
	    for this deviation but I'm unsure how to calculte the right value. What makes this further difficult
	    is that it seems like the advancing of time on the VP is influenced by the CPU load of the host machine.
	    More information on the clock is in the GD32 User Manual on page 62.
	*/
	static constexpr uint64_t scaler = 8000;  // seems about right for snake example

	tlm_utils::simple_target_socket<TIMER> tsock;

	sc_core::sc_event irq_event;
	nuclei_interrupt_gateway *eclic = nullptr;
	uint64_t base = 0;
	uint64_t pause = 0;
	bool reset_mtime = false;
	bool pause_mtime = false;

	/*
	    Regrading mtimectl register:
	    The documentation
	   (https://doc.nucleisys.com/nuclei_spec/isa/timer.html#control-the-timer-counter-through-mtimectl) is for Nuclei
	   Core version >= 0104. The GD32VF103 seems to have version 0100. In this version the register only has one
	   effective bit, the TIMESTOP bit. The other two bits CMPCLREN & CLKSRC are reserved. (The documentation itself is
	   actually conflicting)
	*/
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
	void post_write_mtime(RegisterRange::WriteInfo t);
	void post_write_mtimecmp(RegisterRange::WriteInfo t);
	void post_write_mtimectl(RegisterRange::WriteInfo t);
	void post_write_msip(RegisterRange::WriteInfo t);
	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
	uint64_t update_and_get_mtime(void) override;
	void notify_later();
	void run();
};
