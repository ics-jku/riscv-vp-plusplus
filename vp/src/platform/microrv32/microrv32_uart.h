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
	std::queue<char> txFIFO;
	bool empty, almostEmpty;

	std::fstream uart_stream;

	SC_HAS_PROCESS(MicroRV32UART);

	MicroRV32UART(sc_core::sc_module_name, const std::string uart_rx_fd_path) : uart_stream(uart_rx_fd_path, std::fstream::in | std::fstream::out) {
		tsock.register_b_transport(this, &MicroRV32UART::transport);

		SC_THREAD(run_rx);
		SC_THREAD(run_tx);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();

		if (cmd == tlm::TLM_WRITE_COMMAND) {
		    if (addr == 0) {
		        buf = *ptr;
				txFIFO.push(*ptr);
		    } else if (addr == 4) {
		        std::cout << buf;
				// unused
		    }
		} else if (cmd == tlm::TLM_READ_COMMAND) {
			const size_t size = rxFIFO.size();
		    if (addr == 0) {
		        // ignore
		    } else if (addr == 4) {
		        *((uint32_t*)ptr) = 1;
		    } else if (addr == 8) {
				*((uint32_t*)ptr) = rxFIFO.front();
				rxFIFO.pop();
			} else if(addr == 12) {
				*((uint32_t*)ptr) = size;
			} else if(addr == 16) {
				// almost empty
				*((uint32_t*)ptr) = size == 1;
			} else if(addr == 20) {
				// empty
				*((uint32_t*)ptr) = size == 0;
			}
		}

		(void)delay;  // zero delay
	}

	void event_loop(std::function<void(void)> f){
		while (true) {
			run_event.notify(sc_core::sc_time(200, sc_core::SC_NS));
			sc_core::wait(run_event);  // 40 times per second by default
			f();
		}
	}

	void run_rx() {
		if (uart_stream.is_open()){
			std::cout << "[info] Reading from UART file stream"  << std::endl;
			// read from uart
			event_loop([&](){
				char c;
				while (uart_stream.get(c)){
					rxFIFO.push(c);
				}
			});
			uart_stream.close();
		} else {
			std::cerr << "[error] Failed to open UART file stream" << std::endl;
			std::cout << "[info] Manually pushing random chars into RX buffer" << std::endl;

			event_loop([&](){
				char new_rx_char = rand() + 48;
				rxFIFO.push(new_rx_char);
			});
		}
	}

	void run_tx(){
		if (uart_stream.is_open()){
			event_loop([&](){
				char c = txFIFO.front();
				txFIFO.pop();

				while(!txFIFO.empty()){
					char c = txFIFO.front();
					txFIFO.pop();
					uart_stream.put(c);
				}
			});
		} else {
			// TODO implement
		}
	}
};
