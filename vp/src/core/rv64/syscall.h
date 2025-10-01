#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/syscall.h"
#include "iss.h"

namespace rv64 {

struct SyscallHandler : public SyscallHandlerBase {
	SyscallHandler(sc_core::sc_module_name name) : SyscallHandlerBase(name){};
	uint64_t execute_syscall(uint64_t n, uint64_t _a0, uint64_t _a1, uint64_t _a2, uint64_t _a3) override;
};

}  // namespace rv64
