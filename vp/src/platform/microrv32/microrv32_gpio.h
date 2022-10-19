#pragma once

#include <cstdlib>
#include <cstring>

#include <systemc>

#include <tlm_utils/simple_target_socket.h>

struct MicroRV32GPIO : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MicroRV32GPIO> tsock;

	sc_core::sc_event run_event;
	// each bit represents a pin in each register
	uint8_t direction = 0; // direction of pins, 0: input (read pin), 1: output (write pin)
	uint8_t input = 0; // incomming, read pin values
	uint8_t output = 0; // outgoing pin values

	SC_HAS_PROCESS(MicroRV32GPIO);

	MicroRV32GPIO(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &MicroRV32GPIO::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		// auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();
		
		if (cmd == tlm::TLM_WRITE_COMMAND) {
		    if (addr == 0) {
		        direction = *ptr;
				// printf("\n[TLM] GPIO direction write: %X\n", *ptr);
		    } else if (addr == 4) {
		        output = *ptr;
				// printf("\n[TLM] GPIO output write: %X\n", *ptr);
		    }
		} else if (cmd == tlm::TLM_READ_COMMAND) {
		    if (addr == 0) {
		    	*((uint32_t*)ptr) = direction;
		    } else if (addr == 4) {
		        *((uint32_t*)ptr) = output;
		    } else if (addr == 8) {
		        *((uint32_t*)ptr) = input;
		    }
		}

		(void)delay;  // zero delay
	}
	/*
	 * TODO for future : generate data for GPIO input register 
	 * a) generate random data
	 * b) read file with timestamp and value for input register
	 * c) accept values for input register from socket/outside vp
	 */
};
