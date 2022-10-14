#pragma once

#include <stdint.h>

struct nuclei_external_interrupt_target {
	virtual ~nuclei_external_interrupt_target() {}

	virtual void trigger_external_interrupt(uint32_t irq_id) = 0;
	virtual void clear_external_interrupt(uint32_t irq_id) = 0;
};
