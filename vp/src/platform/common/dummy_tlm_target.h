/*
 * Copyright (C) 2024 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Dummy TLM Target
 *
 * Provides a dummy tlm-target than can for example be used in
 * platforms as placeholder for peripherals that don't yet exist.

 * The module ignores all write operations and returns 0(zero) on all
 * reads.

 * If enabled (see constructor, the module prints debug messages on read and write accesses.
 * Format:
 * <classname>(<sc_name>): @<startaddr>+<transaction offset> <read|write> <transaction value>(<transaction length>)
 * read: "->"; write: "<-"
 *
 * Examples (read):
 * DUMMY_TLM_TARGET(pci): @0x30000000+0xf8010 -> 0x0(4)
 * DUMMY_TLM_TARGET(pci): @0x30000000+0xf8004 -> 0x0(2)
 * Examples (write):
 * DUMMY_TLM_TARGET(pci): @0x30000000+0xf8004 <- 0x0(2)
 * DUMMY_TLM_TARGET(pci): @0x30000000+0xf8010 <- 0xffffffff(4)
 */

#ifndef RISCV_VP_DUMMY_TLM_TARGET_H
#define RISCV_VP_DUMMY_TLM_TARGET_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

class DUMMY_TLM_TARGET : public sc_core::sc_module {
	uint64_t startaddr = 0x0;
	bool debug = false;

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		transport_dbg(trans);
	}

	unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
		auto len = trans.get_data_length();
		uint64_t val = 0;

		if (debug) {
			std::cout << "DUMMY_TLM_TARGET(" << name() << "): @" << std::hex << "0x" << startaddr << "+0x"
			          << trans.get_address();
		}

		if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
			if (debug) {
				memcpy(&val, trans.get_data_ptr(), len);
				std::cout << " <- ";
			}
			// ignore write

		} else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
			// always read 0
			memcpy(trans.get_data_ptr(), &val, len);

			if (debug) {
				std::cout << " -> ";
			}
		} else {
			throw std::runtime_error("unsupported TLM command detected");
		}

		if (debug) {
			std::cout << "0x" << val << "(" << std::dec << len << ")" << std::endl;
		}

		return len;
	}

   public:
	tlm_utils::simple_target_socket<DUMMY_TLM_TARGET> tsock;

	DUMMY_TLM_TARGET(sc_core::sc_module_name, uint64_t startaddr, bool debug = false)
	    : startaddr(startaddr), debug(debug) {
		tsock.register_b_transport(this, &DUMMY_TLM_TARGET::transport);
		tsock.register_transport_dbg(this, &DUMMY_TLM_TARGET::transport_dbg);
	}
};

#endif  // RISCV_VP_DUMMY_TLM_TARGET_H
