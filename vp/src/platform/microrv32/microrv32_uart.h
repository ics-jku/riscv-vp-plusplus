#pragma once

#include <cstdlib>
#include <cstring>

#include <systemc>

#include <tlm_utils/simple_target_socket.h>

struct MicroRV32UART : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MicroRV32UART> tsock;

	sc_core::sc_event run_event;
	char buf = 0;

	// untested
	Vector<char, 16> rxFifo;
	bool empty, almostEmpty;

	SC_HAS_PROCESS(MicroRV32UART);

	MicroRV32UART(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &MicroRV32UART::transport);
		SC_THREAD(run);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();
		
		if (cmd == tlm::TLM_WRITE_COMMAND) {
		    if (addr == 0) {
		        buf = *ptr;
		    } else if (addr == 4) {
		        std::cout << buf;
		    }
		} else if (cmd == tlm::TLM_READ_COMMAND) {
		    if (addr == 0) {
		        // ignore
		    } else if (addr == 4) {
		        *((uint32_t*)ptr) = 1;
			// untested
		    } else if (addr == 8) {
				*((uint32_t*)ptr) = rxFifo->pop();
			} else if(addr == 12) {
				*((uint32_t*)ptr) = rxFifo->occupancy();
			}
		}

		(void)delay;  // zero delay
	}

	// untested
	void run() {
		while (true) {
			run_event.notify(sc_core::sc_time(200, sc_core::SC_MS));
			sc_core::wait(run_event);  // 40 times per second by default
			char newRXChar = rand() + 48;
			rxFifo.push(newRXChar);
		}
	}

};
