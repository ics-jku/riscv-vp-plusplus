#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>

// see: newlib/libgloss/riscv @
// https://github.com/riscv/riscv-newlib/tree/riscv-newlib-2.5.0/libgloss/riscv

#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_getpid 172
#define SYS_kill 129
#define SYS_read 63
#define SYS_write 64
#define SYS_open 1024
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_brk 214
#define SYS_link 1025
#define SYS_unlink 1026
#define SYS_mkdir 1030
#define SYS_chdir 49
#define SYS_getcwd 17
#define SYS_stat 1038
#define SYS_fstat 80
#define SYS_lstat 1039
#define SYS_fstatat 79
#define SYS_access 1033
#define SYS_faccessat 48
#define SYS_pread 67
#define SYS_pwrite 68
#define SYS_uname 160
#define SYS_getuid 174
#define SYS_geteuid 175
#define SYS_getgid 176
#define SYS_getegid 177
#define SYS_mmap 222
#define SYS_munmap 215
#define SYS_mremap 216
#define SYS_time 1062
#define SYS_getmainvars 2011
#define SYS_rt_sigaction 134
#define SYS_writev 66
#define SYS_gettimeofday 169
#define SYS_times 153
#define SYS_fcntl 25
#define SYS_getdents 61
#define SYS_dup 23

// custom extensions
#define SYS_host_error \
	1  // indicate an error, i.e. this instruction should never be reached so something went wrong during exec.
#define SYS_host_test_pass 2  // RISC-V test execution successfully completed
#define SYS_host_test_fail 3  // RISC-V test execution failed

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "iss.h"
#include "syscall_if.h"

namespace rv64 {

struct SyscallHandler : public sc_core::sc_module, syscall_emulator_if {
	tlm_utils::simple_target_socket<SyscallHandler> tsock;
	std::unordered_map<uint32_t, iss_syscall_if *> cores;

	void register_core(ISS *core) {
		assert(cores.find(core->get_hart_id()) == cores.end());
		cores[core->get_hart_id()] = core;
	}

	SyscallHandler(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &SyscallHandler::transport);
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
		auto a7 = core->read_register(RegFile::a7);
		auto a3 = core->read_register(RegFile::a3);
		auto a2 = core->read_register(RegFile::a2);
		auto a1 = core->read_register(RegFile::a1);
		auto a0 = core->read_register(RegFile::a0);

		// printf("a7=%u, a0=%u, a1=%u, a2=%u, a3=%u\n", a7, a0, a1, a2, a3);

		auto ans = execute_syscall(a7, a0, a1, a2, a3);

		core->write_register(RegFile::a0, ans);

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
	int execute_syscall(uint64_t n, uint64_t _a0, uint64_t _a1, uint64_t _a2, uint64_t _a3);
};

}  // namespace rv64