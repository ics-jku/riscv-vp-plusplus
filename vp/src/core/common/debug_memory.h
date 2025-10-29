#ifndef RISCV_ISA_DEBUG_MEMORY_H
#define RISCV_ISA_DEBUG_MEMORY_H

#include <tlm_utils/simple_initiator_socket.h>

#include <string>
#include <systemc>
#include <type_traits>

#include "core_defs.h"
#include "trap.h"
#include "util/tlm_ext_tag.h"

struct DebugMemoryInterface : public sc_core::sc_module {
	tlm_utils::simple_initiator_socket<DebugMemoryInterface> isock;
	tlm::tlm_generic_payload trans;
	tlm_ext_tag *trans_ext_tag;

	DebugMemoryInterface(sc_core::sc_module_name) {
		/*
		 * always use the tag extension and unset tag (see cheriv9)
		 * Note: tlm_generic_payload frees all extension objects in destructor, therefore dynamic allocation is needed
		 */
		trans_ext_tag = new tlm_ext_tag(false);
		trans.set_extension(trans_ext_tag);
	}

	unsigned _do_dbg_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes);

	std::string read_memory(uint64_t start, unsigned nbytes);

	void write_memory(uint64_t start, unsigned nbytes, const std::string &data);
};

#endif  // RISCV_ISA_GDB_STUB_H
