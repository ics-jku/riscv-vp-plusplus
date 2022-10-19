#pragma once

#include <cstdlib>
#include <cstring>

#include <systemc>

#include <tlm_utils/simple_target_socket.h>

struct MicroRV32LED : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MicroRV32LED> tsock;

	sc_core::sc_event run_event;
	uint8_t led_vals = 0;

	SC_HAS_PROCESS(MicroRV32LED);

	MicroRV32LED(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &MicroRV32LED::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		// auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();
		
		if (cmd == tlm::TLM_WRITE_COMMAND) {
		    if (addr == 0) {
		        led_vals = *ptr;
				// printf("\n[TLM] LED write : %X \n", *ptr);
		    }
		} else if (cmd == tlm::TLM_READ_COMMAND) {
		    if (addr == 0) {
		    	 *((uint32_t*)ptr) = led_vals;
		    }
		}

		(void)delay;  // zero delay
	}
};
