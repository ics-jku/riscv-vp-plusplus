#include "options.h"

#include <unistd.h>

#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

Options::Options(void) {
	// clang-format off
	add_options()
		("help", "produce help message")
		("use-E-base-isa", po::bool_switch(&use_E_base_isa), "use the E instead of the I integer base ISA")
		("en-ext-Zfh", po::bool_switch(&en_ext_Zfh), "enable the half-precision floating point extension (Zfh)")
		("intercept-syscalls", po::bool_switch(&intercept_syscalls), "directly intercept and handle syscalls in the ISS (testing mode)")
		("error-on-zero-traphandler", po::value<bool>(&error_on_zero_traphandler), "Assume that taking an unset (zero) trap handler in machine mode is an error condition (which it usually is)")
		("debug-mode", po::bool_switch(&use_debug_runner), "start execution in debugger (using gdb rsp interface)")
		("debug-port", po::value<unsigned int>(&debug_port), "select port number to connect with GDB")
		("trace-mode", po::bool_switch(&trace_mode), "enable instruction tracing")
		("tlm-global-quantum", po::value<unsigned int>(&tlm_global_quantum), "set global tlm quantum (in NS)")
		("use-dbbcache", po::bool_switch(&use_dbbcache), "use the Dynamic Basic Block Cache (DBBCache) to speed up execution")
		("use-lscache", po::bool_switch(&use_lscache), "use the Load/Store Cache (LSCache) to speed up dmi access (automatically enables data-dmi, if not set)")
		("use-instr-dmi", po::bool_switch(&use_instr_dmi), "use dmi to fetch instructions")
		("use-data-dmi", po::bool_switch(&use_data_dmi), "use dmi to execute load/store operations")
		("use-dmi", po::bool_switch(), "use instr and data dmi")
		("debug-bus-mode", po::bool_switch(&use_debug_bus), "dump tlm transaction data via TCP connection")
		("debug-bus-port", po::value<unsigned int>(&debug_bus_port),"select port number for tlm transaction data")
		("break-on-transaction", po::bool_switch(&break_on_transaction),"break on every transaction when in --debug-mode")
		("input-file", po::value<std::string>(&input_program)->required(), "input file to use for execution");
	// clang-format on

	pos.add("input-file", 1);
}

Options::~Options() {};

void Options::parse(int argc, char **argv) {
	try {
		auto parser = po::command_line_parser(argc, argv);
		parser.options(*this).positional(pos);

		po::store(parser.run(), vm);

		if (vm.count("help")) {
			std::cout << *this << std::endl;
			exit(0);
		}

		po::notify(vm);

		if (vm["use-lscache"].as<bool>() && !vm["use-dmi"].as<bool>() && !vm["use-data-dmi"].as<bool>()) {
			std::cerr << "[Options] Info: switch 'use-lscache' also activates 'use-data-dmi' if unset." << std::endl;
			use_data_dmi = true;
		}
		if (vm["use-dmi"].as<bool>()) {
			use_data_dmi = true;
			use_instr_dmi = true;
		}
		if (vm["break-on-transaction"].as<bool>() && !vm["debug-mode"].as<bool>()) {
			std::cerr << "[Options] Error: switch 'break-on-transaction' can only be used if 'debug-mode' is set."
			          << std::endl;
			exit(1);
		}
		if (vm["intercept-syscalls"].as<bool>() && vm.count("error-on-zero-traphandler") == 0) {
			// intercept syscalls active, but no overriding error-on-zero-traphandler switch
			std::cerr
			    << "[Options] Info: switch 'intercept-syscalls' also activates 'error-on-zero-traphandler' if unset."
			    << std::endl;
			error_on_zero_traphandler = true;
		}
	} catch (po::error &e) {
		std::cerr << "Error parsing command line options: " << e.what() << std::endl;

		std::cout << *this << std::endl;
		exit(1);
	}
}

void Options::printValues(std::ostream &os) const {
	os << std::dec;
	os << "intercept_syscalls: " << intercept_syscalls << std::endl;
	os << "error-on-zero-traphandler: " << error_on_zero_traphandler << std::endl;
	os << "use_debug_runner: " << use_debug_runner << std::endl;
	os << "debug_port: " << debug_port << std::endl;
	os << "trace_mode: " << trace_mode << std::endl;
	os << "tlm_global_quantum: " << tlm_global_quantum << std::endl;
	os << "use_instr_dmi: " << use_instr_dmi << std::endl;
	os << "use_data_dmi: " << use_data_dmi << std::endl;
}
