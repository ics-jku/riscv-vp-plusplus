#pragma once

#include <stdint.h>

/* The syscall handler is using this interface to access and control the ISS. */
struct iss_syscall_if {
	virtual ~iss_syscall_if() {}

	virtual void sys_exit() = 0;
	// virtual void sys_break() = 0;

	virtual unsigned get_syscall_register_index() = 0;

	virtual uint64_t read_register(unsigned idx) = 0;
	virtual void write_register(unsigned idx, uint64_t value) = 0;

	virtual uint64_t get_hart_id() = 0;
};

/* Using this interface, the ISS supports to intercept and delegate syscalls. */
struct syscall_emulator_if {
	virtual ~syscall_emulator_if() {}

	virtual void execute_syscall(iss_syscall_if *core) = 0;
};
