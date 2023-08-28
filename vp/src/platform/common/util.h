#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/common.h"

/* A simple memory block that provides arbitrary read/write access. */
struct BlankDevice : public sc_core::sc_module {
	tlm_utils::simple_target_socket<BlankDevice> tsock;

	unsigned size;
	uint8_t *data;

	BlankDevice(sc_core::sc_module_name, unsigned size) : size(size), data(new uint8_t[size]) {
		tsock.register_b_transport(this, &BlankDevice::transport);
	}

	~BlankDevice() {
		delete[] data;
		data = 0;
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		UNUSED(delay);

		assert(data);
		assert(trans.get_address() + trans.get_data_length() <= size);

		if (trans.get_command() == tlm::TLM_READ_COMMAND) {
			memcpy(trans.get_data_ptr(), data + trans.get_address(), trans.get_data_length());
		} else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
			memcpy(data + trans.get_address(), trans.get_data_ptr(), trans.get_data_length());
		} else {
			throw std::runtime_error("unsupported TLM command");
		}
	}
};

/* Ignores all write accesses and always returns zero on any read access. */
struct ZeroDevice : public sc_core::sc_module {
	tlm_utils::simple_target_socket<ZeroDevice> tsock;

	ZeroDevice(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &ZeroDevice::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		UNUSED(delay);

		if (trans.get_command() == tlm::TLM_READ_COMMAND) {
			memset(trans.get_data_ptr(), 0, trans.get_data_length());
		}
	}
};

/* Simple UART/terminal, e.g. this implementation matches the HiFive1 board write address and is already sufficient to
 * print characters in Zephyr OS examples. */
struct SimpleUART : public sc_core::sc_module {
	tlm_utils::simple_target_socket<SimpleUART> tsock;

	SimpleUART(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &SimpleUART::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		UNUSED(delay);

		if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
			if (trans.get_address() == 0x0) {
				std::cout << static_cast<char>(*trans.get_data_ptr());
				fflush(stdout);
			}
		}
	}
};