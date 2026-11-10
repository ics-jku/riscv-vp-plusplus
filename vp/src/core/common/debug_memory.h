#ifndef RISCV_ISA_DEBUG_MEMORY_H
#define RISCV_ISA_DEBUG_MEMORY_H

#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <systemc>
#include <type_traits>

#include "core_defs.h"
#include "trap.h"
#include "util/tlm_ext_tag.h"

struct DebugTagMemoryInterface {
	virtual std::vector<uint8_t> read_tag_bytes(uint64_t paddr, uint64_t length) = 0;
	virtual void write_tag_bytes(uint64_t paddr, uint64_t length, const std::vector<uint8_t> &tags) = 0;
	virtual ~DebugTagMemoryInterface() = default;
};

struct DebugMemoryInterface : public sc_core::sc_module {
	tlm_utils::simple_initiator_socket<DebugMemoryInterface> isock;
	tlm::tlm_generic_payload trans;
	tlm_ext_tag *trans_ext_tag;
	DebugTagMemoryInterface *tag_memory = nullptr;

	DebugMemoryInterface(sc_core::sc_module_name) {
		/*
		 * always use the tag extension and unset tag
		 * Note: tlm_generic_payload frees all extension objects in destructor, therefore dynamic allocation is needed
		 */
		trans_ext_tag = new tlm_ext_tag(false);
		trans.set_extension(trans_ext_tag);
	}

	unsigned _do_dbg_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes);

	std::vector<uint8_t> read_memory_bytes(uint64_t start, unsigned nbytes);

	std::string read_memory(uint64_t start, unsigned nbytes);

	void write_memory(uint64_t start, unsigned nbytes, const std::string &data);

	void set_tag_memory(DebugTagMemoryInterface *tag_mem) {
		tag_memory = tag_mem;
	}
	std::vector<uint8_t> read_memory_tags(uint64_t paddr, uint64_t length) {
		if (!tag_memory)
			throw std::runtime_error("No tag memory interface set");
		return tag_memory->read_tag_bytes(paddr, length);
	}
};

#endif  // RISCV_ISA_GDB_STUB_H
