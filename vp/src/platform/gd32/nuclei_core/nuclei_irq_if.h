#pragma once

#include <stdint.h>

struct eclic_interrupt_target {
	virtual ~eclic_interrupt_target() {}

	/* we only need to notify triggers (clears are not necessary for now) */
	virtual void trigger_eclic_interrupt() = 0;
};

struct nuclei_interrupt_gateway {
	virtual ~nuclei_interrupt_gateway() {}

	virtual void gateway_trigger_interrupt(uint32_t irq_id) = 0;
	virtual void gateway_clear_interrupt(uint32_t irq_id) = 0;
};
