#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "regfile_base.h"
#include "syscall_if.h"

namespace Syscall {
// based on newlib/libgloss/riscv @
// https://github.com/riscvarchive/riscv-newlib/blob/riscv-newlib-2.5.0/libgloss/riscv/machine/syscall.h
enum Nr {
	exit = 93,
	exit_group = 94,
	getpid = 172,
	kill = 129,
	read = 63,
	write = 64,
	open = 1024,
	openat = 56,
	close = 57,
	lseek = 62,
	brk = 214,
	link = 1025,
	unlink = 1026,
	mkdir = 1030,
	chdir = 49,
	getcwd = 17,
	stat = 1038,
	fstat = 80,
	lstat = 1039,
	fstatat = 79,
	access = 1033,
	faccessat = 48,
	pread = 67,
	pwrite = 68,
	uname = 160,
	getuid = 174,
	geteuid = 175,
	getgid = 176,
	getegid = 177,
	mmap = 222,
	munmap = 215,
	mremap = 216,
	time = 1062,
	getmainvars = 2011,
	rt_sigaction = 134,
	writev = 66,
	gettimeofday = 169,
	times = 153,
	fcntl = 25,
	getdents = 61,
	dup = 23,

	// custom extensions
	// indicate an error, i.e. this instruction should never be reached so something went wrong during exec
	host_error = 1,
	// RISC-V test execution successfully completed
	host_test_pass = 2,
	// RISC-V test execution failed
	host_test_fail = 3,
};
}  // namespace Syscall

/*
 * abstract implementation -> see rv32/rv64 syscall
 */
struct SyscallHandlerBase : public sc_core::sc_module, syscall_emulator_if {
	tlm_utils::simple_target_socket<SyscallHandlerBase> tsock;
	std::unordered_map<uint64_t, iss_syscall_if *> cores;

	SyscallHandlerBase(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &SyscallHandlerBase::transport);
	}

	void register_core(iss_syscall_if *core) {
		assert(cores.find(core->get_hart_id()) == cores.end());
		cores[core->get_hart_id()] = core;
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		(void)delay;

		auto addr = trans.get_address();
		assert(addr % 4 == 0);
		assert(trans.get_data_length() == 4);
		auto hart_id = *((uint32_t *)trans.get_data_ptr());

		assert((cores.find(hart_id) != cores.end()) && "core not registered in syscall handler");

		execute_syscall(cores[hart_id]);
	}

	virtual void execute_syscall(iss_syscall_if *core) override {
		auto syscall = core->read_register(core->get_syscall_register_index());
		auto a3 = core->read_register(RegFileBase::a3);
		auto a2 = core->read_register(RegFileBase::a2);
		auto a1 = core->read_register(RegFileBase::a1);
		auto a0 = core->read_register(RegFileBase::a0);

		// printf("a7=%u, a0=%u, a1=%u, a2=%u, a3=%u\n", a7, a0, a1, a2, a3);

		auto ans = execute_syscall(syscall, a0, a1, a2, a3);

		core->write_register(RegFileBase::a0, ans);

		if (shall_exit)
			core->sys_exit();
	}

	uint8_t *mem = 0;     // direct pointer to start of guest memory in host memory
	uint64_t mem_offset;  // start address of the memory as mapped into the
	                      // address space
	uint64_t hp = 0;      // heap pointer
	bool shall_exit = false;
	bool shall_break = false;

	// only for memory consumption evaluation
	uint64_t start_heap = 0;
	uint64_t max_heap = 0;

	uint64_t get_max_heap_memory_consumption() {
		return max_heap - start_heap;
	}

	void init(uint8_t *host_memory_pointer, uint64_t mem_start_address, uint64_t heap_pointer_address) {
		mem = host_memory_pointer;
		mem_offset = mem_start_address;
		hp = heap_pointer_address;

		start_heap = hp;
		max_heap = hp;
	}

	uint8_t *guest_address_to_host_pointer(uintptr_t addr) {
		assert(mem != nullptr);

		return mem + (addr - mem_offset);
	}

	uint8_t *guest_to_host_pointer(void *p) {
		return guest_address_to_host_pointer((uintptr_t)p);
	}

	/*
	 * Syscalls are implemented to work directly on guest memory (represented in
	 * host as byte array). Note: the data structures on the host system might
	 * not be binary compatible with those on the guest system.
	 */
	virtual uint64_t execute_syscall(uint64_t n, uint64_t _a0, uint64_t _a1, uint64_t _a2, uint64_t _a3) = 0;
};
