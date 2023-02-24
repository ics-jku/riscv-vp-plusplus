#ifndef RISCV_ISA_IRQ_IF_H
#define RISCV_ISA_IRQ_IF_H

#include <stdint.h>

typedef uint32_t PrivilegeLevel;

constexpr uint32_t MachineMode = 0b11;
constexpr uint32_t HypervisorMode = 0b10;
constexpr uint32_t SupervisorMode = 0b01;
constexpr uint32_t UserMode = 0b00;
constexpr uint32_t NoneMode = -1;  // invalid sentinel to avoid passing a boolean alongside a privilege level

struct external_interrupt_target {
	virtual ~external_interrupt_target() {}

	virtual void trigger_external_interrupt(PrivilegeLevel level) = 0;
	virtual void clear_external_interrupt(PrivilegeLevel level) = 0;
};

struct clint_interrupt_target {
	virtual ~clint_interrupt_target() {}

	virtual void trigger_timer_interrupt() = 0;
	virtual void clear_timer_interrupt() = 0;
	virtual void trigger_software_interrupt() = 0;
	virtual void clear_software_interrupt() = 0;
};

struct interrupt_gateway {
	virtual ~interrupt_gateway() {}

	virtual void gateway_trigger_interrupt(uint32_t irq_id) = 0;
};

#endif  // RISCV_ISA_IRQ_IF_H
