#pragma once

#include <stdint.h>

struct nuclei_interrupt_gateway {
	virtual ~nuclei_interrupt_gateway() {}

	virtual void gateway_trigger_interrupt(uint32_t irq_id) = 0;
	virtual void gateway_clear_interrupt(uint32_t irq_id) = 0;
};
