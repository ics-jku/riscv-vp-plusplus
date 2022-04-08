#pragma once

#include <tlm_utils/simple_target_socket.h>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <boost/system/error_code.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <queue>
#include <systemc>

#include "simple_serial.h"

namespace po = boost::program_options;

struct MicroRV32UART : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MicroRV32UART> tsock;

	sc_core::sc_event run_event_rx;
	sc_core::sc_event run_event_tx;

	char buf = 0;

	std::queue<char> rxFIFO;
	std::queue<char> txFIFO;
	bool empty, almostEmpty;

	SimpleSerial serial;

	SC_HAS_PROCESS(MicroRV32UART);

	bool serial_opened;

	MicroRV32UART(sc_core::sc_module_name, const std::string port, const unsigned int baud_rate) try
	    : serial(port, baud_rate) {
		tsock.register_b_transport(this, &MicroRV32UART::transport);
		printf("Test UART \n");
		std::cout << "UART @ " << port << std::endl;
		SC_THREAD(run_rx);
		SC_THREAD(run_tx);
	} catch (boost::system::system_error e) {
		boost::system::error_code ec = e.code();
		std::cerr << ec.value() << std::endl;
		;
		std::cerr << ec.category().name() << std::endl;
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();
		//std::cout << "[UART]TLM Transport" << std::endl;

		if (cmd == tlm::TLM_WRITE_COMMAND) {
			if (addr == 0) {
				buf = *ptr;
				txFIFO.push(*ptr);
			} else if (addr == 4) {
				// std::cout << buf;
				// unused
			}
		} else if (cmd == tlm::TLM_READ_COMMAND) {
			const size_t size = rxFIFO.size();
			if (addr == 0) {
				// ignore
			} else if (addr == 4) {
				*((uint32_t *)ptr) = 1;
			} else if (addr == 8) {
				*((uint32_t *)ptr) = rxFIFO.front();
				rxFIFO.pop();
			} else if (addr == 12) {
				*((uint32_t *)ptr) = size;
			} else if (addr == 16) {
				// almost empty
				*((uint32_t *)ptr) = size == 1;
			} else if (addr == 20) {
				// empty
				*((uint32_t *)ptr) = size == 0;
			}
		}

		(void)delay;  // zero delay
	}

	//~ void event_loop(std::function<void(void)> f){
	//~ while (true) {
	//~ run_event.notify(sc_core::sc_time(200, sc_core::SC_NS));
	//~ sc_core::wait(run_event);  // 40 times per second by default
	//~ f();
	//~ }
	//~ }

	void run_rx() {
		std::cout << "[UART-TLM] RX run_rx called" << std::endl;
		if (serial.is_open()) {
			while (true) {
				run_event_rx.notify(sc_core::sc_time(50, sc_core::SC_US));
				sc_core::wait(run_event_rx);  // 40 times per second by default
				std::cout << "[UART-TLM] Reading from UART file stream" << std::endl;
				// read from uart
				char c;
				while (c = serial.read()) {
					std::cout << "[UART] RX rx_run(): c =" << c << std::endl;
					rxFIFO.push(c);
					run_event_tx.notify(sc_core::sc_time(50, sc_core::SC_US));
					sc_core::wait(run_event_tx);  // 40 times per second by default
				}
			}
		} else {
			std::cerr << "[UART-TLM] Failed to open UART file stream" << std::endl;
			std::cout << "[UART-TLM] Manually pushing random chars into RX buffer" << std::endl;
			while (true) {
				run_event_rx.notify(sc_core::sc_time(50, sc_core::SC_US));
				sc_core::wait(run_event_rx);  // 40 times per second by default
				std::cout << "[UART-TLM] Reading random chars from UART" << std::endl;
				// put rng chars into rxfifo
				char new_rx_char = rand() + 48;
				rxFIFO.push(new_rx_char);
			}
		}
	}

	void run_tx() {
		std::cout << "[UART-TLM] TX run_tx called" << std::endl;
		if (serial.is_open()) {
			while (true) {
				run_event_tx.notify(sc_core::sc_time(50, sc_core::SC_US));
				sc_core::wait(run_event_tx);  // 40 times per second by default
				while (!txFIFO.empty()) {
					char c = txFIFO.front();
					txFIFO.pop();
					serial.write(c);
					std::cout << "[UART-TLM] TX run_tx() wrote c=" << c << std::endl;
					run_event_tx.notify(sc_core::sc_time(50, sc_core::SC_US));
					sc_core::wait(run_event_tx);  // 40 times per second by default
				}
			}
		} else {
			std::cout << "[UART-TLM] run_tx serial is not open!" << std::endl;
			// TODO implement
		}
	}
};
