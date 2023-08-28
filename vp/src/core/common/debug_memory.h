#ifndef RISCV_ISA_DEBUG_MEMORY_H
#define RISCV_ISA_DEBUG_MEMORY_H

#include <tlm_utils/simple_initiator_socket.h>

#include <string>
#include <systemc>
#include <type_traits>

#include "core_defs.h"
#include "trap.h"

struct DebugMemoryInterface : public sc_core::sc_module {
	tlm_utils::simple_initiator_socket<DebugMemoryInterface> isock;

	DebugMemoryInterface(sc_core::sc_module_name) {}

	unsigned _do_dbg_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes);

	std::string read_memory(uint64_t start, unsigned nbytes);

	void write_memory(uint64_t start, unsigned nbytes, const std::string &data);
};

#endif  // RISCV_ISA_GDB_STUB_H
