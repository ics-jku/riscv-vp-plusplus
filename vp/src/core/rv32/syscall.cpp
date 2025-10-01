#include "syscall.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <stdexcept>

namespace rv32 {

// see: riscv-gnu-toolchain/riscv-newlib/libgloss/riscv/
// for syscall implementation in the risc-v C lib (many are ignored and just return -1)

/*
 * TODO: check parameters and return values (see sys_brk fix)
 * 1. do riscv and target types match (XLEN!)?
 * 2. are assumptions on signed/unsigned correct?
 * Cleanup: use types with explicit bit-widths
 */

typedef int32_t rv32_long;

typedef int32_t rv32_time_t;

struct rv32_timeval {
	rv32_time_t tv_sec;
	rv32_time_t tv_usec;
};

struct rv32_timespec {
	rv32_time_t tv_sec;
	rv32_time_t tv_nsec;
};

struct rv32_stat {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	uint32_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	uint64_t __pad1;
	int64_t st_size;
	int32_t st_blksize;
	int32_t __pad2;
	int64_t st_blocks;
	rv32_timespec st_atim;
	rv32_timespec st_mtim;
	rv32_timespec st_ctim;
	int32_t __glibc_reserved[2];
};

void _copy_timespec(rv32_timespec *dst, timespec *src) {
	dst->tv_sec = src->tv_sec;
	dst->tv_nsec = src->tv_nsec;
}

int sys_fstat(SyscallHandler *sys, int fd, rv32_stat *s_addr) {
	struct stat x;
	int ans = fstat(fd, &x);
	if (ans == 0) {
		rv32_stat *p = (rv32_stat *)sys->guest_to_host_pointer(s_addr);
		p->st_dev = x.st_dev;
		p->st_ino = x.st_ino;
		p->st_mode = x.st_mode;
		p->st_nlink = x.st_nlink;
		p->st_uid = x.st_uid;
		p->st_gid = x.st_gid;
		p->st_rdev = x.st_rdev;
		p->st_size = x.st_size;
		p->st_blksize = x.st_blksize;
		p->st_blocks = x.st_blocks;
		_copy_timespec(&p->st_atim, &x.st_atim);
		_copy_timespec(&p->st_mtim, &x.st_mtim);
		_copy_timespec(&p->st_ctim, &x.st_ctim);
	}
	return ans;
}

int sys_gettimeofday(SyscallHandler *sys, rv32_timeval *tp, void *tzp) {
	/*
	 * timeval is using a struct with two long arguments.
	 * The second argument tzp currently is not used by riscv code.
	 */
	assert(tzp == 0);

	struct timeval x;
	int ans = gettimeofday(&x, 0);

	rv32_timeval *p = (rv32_timeval *)sys->guest_to_host_pointer(tp);
	p->tv_sec = x.tv_sec;
	p->tv_usec = x.tv_usec;
	return ans;
}

int sys_time(SyscallHandler *sys, rv32_time_t *tloc) {
	time_t host_ans = time(0);

	rv32_time_t guest_ans = boost::lexical_cast<rv32_time_t>(host_ans);

	if (tloc != 0) {
		rv32_time_t *p = (rv32_time_t *)sys->guest_to_host_pointer(tloc);
		*p = guest_ans;
	}

	return boost::lexical_cast<int>(guest_ans);
}

namespace rv_sc {
// see: riscv-gnu-toolchain/riscv/riscv32-unknown-elf/include/sys/_default_fcntl.h
constexpr uint32_t RDONLY = 0x0000; /* +1 == FREAD */
constexpr uint32_t WRONLY = 0x0001; /* +1 == FWRITE */
constexpr uint32_t RDWR = 0x0002;   /* +1 == FREAD|FWRITE */
constexpr uint32_t APPEND = 0x0008;
constexpr uint32_t CREAT = 0x0200;
constexpr uint32_t TRUNC = 0x0400;
}  // namespace rv_sc

int translateRVFlagsToHost(const int flags) {
	int ret = 0;
	ret |= flags & rv_sc::RDONLY ? O_RDONLY : 0;
	ret |= flags & rv_sc::WRONLY ? O_WRONLY : 0;
	ret |= flags & rv_sc::RDWR ? O_RDWR : 0;
	ret |= flags & rv_sc::APPEND ? O_APPEND : 0;
	ret |= flags & rv_sc::CREAT ? O_CREAT : 0;
	ret |= flags & rv_sc::TRUNC ? O_TRUNC : 0;

	if (ret == 0 && flags != 0) {
		throw std::runtime_error("unsupported flag");
	}

	return ret;
}

// TODO: uint32_t would be sufficent here
uint64_t sys_brk(SyscallHandler *sys, uint64_t addr) {
	// riscv newlib expects brk to return current heap address when zero is passed in
	if (addr != 0) {
		// NOTE: can also shrink again
		sys->hp = addr;

		if (sys->hp > sys->max_heap) {
			sys->max_heap = sys->hp;
		}
	}

	return sys->hp;
}

int sys_write(SyscallHandler *sys, int fd, const void *buf, size_t count) {
	const char *p = (const char *)sys->guest_to_host_pointer((void *)buf);

	auto ans = write(fd, p, count);

	if (ans < 0) {
		std::cout << "WARNING [sys-write error]: " << strerror(errno) << std::endl;
		std::cout << "  fd = " << fd << std::endl;
		std::cout << "  count = " << count << std::endl;
		assert(false);
	}

	return ans;
}

int sys_read(SyscallHandler *sys, int fd, void *buf, size_t count) {
	char *p = (char *)sys->guest_to_host_pointer(buf);

	auto ans = read(fd, p, count);

	assert(ans >= 0);

	return ans;
}

int sys_lseek(int fd, off_t offset, int whence) {
	auto ans = lseek(fd, offset, whence);

	return ans;
}

int sys_open(SyscallHandler *sys, const char *pathname, int flags, mode_t mode) {
	const char *host_pathname = (char *)sys->guest_to_host_pointer((void *)pathname);

	auto ans = open(host_pathname, translateRVFlagsToHost(flags), mode);

	std::cout << "[sys_open] " << host_pathname << ", " << flags << " (translated to " << translateRVFlagsToHost(flags)
	          << "), " << mode << std::endl;

	return ans;
}

int sys_close(int fd) {
	if (fd == STDOUT_FILENO || fd == STDIN_FILENO || fd == STDERR_FILENO) {
		// ignore closing of std streams, just return success
		return 0;
	} else {
		return close(fd);
	}
}

/*
 *  TODO: Some parameters need to be aligned to the hosts word width (mostly 64 bit)
 *	Especially when coming from a 32 bit guest system.
 */

uint64_t SyscallHandler::execute_syscall(uint64_t n, uint64_t _a0, uint64_t _a1, uint64_t _a2, uint64_t) {
	// NOTE: when linking with CRT, the most basic example only calls *gettimeofday* and finally *exit*

	switch (n) {
		case SYS_fstat:
			return sys_fstat(this, _a0, (rv32_stat *)_a1);

		case SYS_gettimeofday:
			return sys_gettimeofday(this, (struct rv32_timeval *)_a0, (void *)_a1);

		case SYS_brk:
			return sys_brk(this, _a0);

		case SYS_time:
			return sys_time(this, (rv32_time_t *)_a0);

		case SYS_write:
			return sys_write(this, _a0, (void *)_a1, _a2);

		case SYS_read:
			return sys_read(this, _a0, (void *)_a1, _a2);

		case SYS_lseek:
			return sys_lseek(_a0, _a1, _a2);

		case SYS_open:
			return sys_open(this, (const char *)_a0, _a1, _a2);

		case SYS_close:
			return sys_close(_a0);

		case SYS_exit:
			// If the software requested a non-zero exit code then terminate directly.
			// Otherwise, stop the SystemC simulation and exit with a zero exit code.
			if (_a0)
				exit(_a0);

			shall_exit = true;
			return 0;

		case SYS_host_error:
			throw std::runtime_error("SYS_host_error");

		case SYS_host_test_pass:
			std::cout << "TEST_PASS" << std::endl;
			shall_exit = true;
			return 0;

		case SYS_host_test_fail:
			std::cout << "TEST_FAIL (testnum = " << _a0 << ")" << std::endl;
			shall_exit = true;
			return 0;
	}

	std::cerr << "unsupported syscall '" << n << "'" << std::endl;
	std::cerr << "is this perhaps a trap ExceptionCode? " << std::endl;
	throw std::runtime_error("unsupported syscall '" + std::to_string(n) + "'");
}
}  // namespace rv32
