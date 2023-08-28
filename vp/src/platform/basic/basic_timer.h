#ifndef RISCV_ISA_BASIC_TIMER_H
#define RISCV_ISA_BASIC_TIMER_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"

struct BasicTimer : public sc_core::sc_module {
	interrupt_gateway *plic = 0;
	uint32_t irq_number = 0;

	SC_HAS_PROCESS(BasicTimer);

	BasicTimer(sc_core::sc_module_name, uint32_t irq_number) : irq_number(irq_number) {
		SC_THREAD(run);
	}

	void run() {
		while (true) {
			sc_core::wait(sc_core::sc_time(1, sc_core::SC_MS));

			plic->gateway_trigger_interrupt(irq_number);
		}
	}
};

#endif  // RISCV_ISA_BASIC_TIMER_H
