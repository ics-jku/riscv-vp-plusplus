#pragma once

#include <queue>

#include <cstdlib>
#include <cstring>

#include <systemc>

#include <boost/program_options.hpp>

#include <tlm_utils/simple_target_socket.h>

#include <fstream>

#include <unistd.h>


namespace po = boost::program_options;


struct MicroRV32UART : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MicroRV32UART> tsock;

	sc_core::sc_event run_event;
	char buf = 0;

	std::queue<char> rxFIFO;
	bool empty, almostEmpty;

	std::string uart_rx_fd_path_;

	SC_HAS_PROCESS(MicroRV32UART);

	MicroRV32UART(sc_core::sc_module_name, const std::string uart_rx_fd_path) {
		tsock.register_b_transport(this, &MicroRV32UART::transport);
		uart_rx_fd_path_ = uart_rx_fd_path;
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
				*((uint32_t*)ptr) = rxFIFO.front();
				rxFIFO.pop();
			} else if(addr == 12) {
				*((uint32_t*)ptr) = rxFIFO.size();
			}
		}

		(void)delay;  // zero delay
	}

	void event_loop(std::function<void(void)> f){
		while (true) {
			run_event.notify(sc_core::sc_time(200, sc_core::SC_MS));
			sc_core::wait(run_event);  // 40 times per second by default
			f();
		}
	}

	// untested
	void run() {
		std::ifstream uart_stream;
	    uart_stream.open(uart_rx_fd_path_, std::ifstream::in);
		if (uart_stream.is_open()){
			// read from uart
			event_loop([&](){
				char new_rx_char = rand() + 48;
				rxFIFO.push(new_rx_char);
			});
		} else {
			std::cerr << "Failed to open file " << uart_rx_fd_path_ << std::endl;

			event_loop([&](){
				char new_rx_char = rand() + 48;
				rxFIFO.push(new_rx_char);
			});
		}
	}
};
