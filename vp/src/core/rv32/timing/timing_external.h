#pragma once

#include <dlfcn.h>

#include <boost/io/ios_state.hpp>
#include <iomanip>

#include "../iss.h"

std::string RISCV_TIMING_SIM_LIB = "riscv-timing-sim.so";
std::string RISCV_TIMING_DB = "riscv-timing-db.xml";

// NOTE: the interface inside the library has to match this one exactly, don't
// forget the *virtual* attribute
struct SimTimingInterface {
	virtual uint64_t get_cycles_for_instruction(uint64_t pc);

	// only to detect errors in loading the shared library
	virtual uint64_t get_magic_number();
};

struct ExternalTimingDecorator : public timing_if {
	SimTimingInterface *timing_sim = 0;
	void *lib_handle = 0;
	SimTimingInterface *(*create)(const char *) = 0;
	void (*destroy)(SimTimingInterface *) = 0;

	void initialize() {
		lib_handle = dlopen(RISCV_TIMING_SIM_LIB.c_str(), RTLD_LAZY);
		if (!lib_handle)
			throw std::runtime_error("unable to open shared library '" + RISCV_TIMING_SIM_LIB + "'");

		create = (SimTimingInterface * (*)(const char *)) dlsym(lib_handle, "create_riscv_vp_timing_interface");
		if (!create)
			throw std::runtime_error("unable to load 'create_riscv_vp_timing_interface' function");

		destroy = (void (*)(SimTimingInterface *))dlsym(lib_handle, "destroy_riscv_vp_timing_interface");
		if (!destroy)
			throw std::runtime_error("unable to load 'destroy_riscv_vp_timing_interface' function");

		timing_sim = (SimTimingInterface *)create(RISCV_TIMING_DB.c_str());
	}

	ExternalTimingDecorator() {
		initialize();
	}

	~ExternalTimingDecorator() {
		assert(timing_sim != 0);
		destroy(timing_sim);
		assert(lib_handle != 0);
		dlclose(lib_handle);
	}

	void on_begin_exec_step(Instruction instr, Opcode::mapping op, ISS &iss) override {
		uint64_t cycles = timing_sim->get_cycles_for_instruction(iss.last_pc);

		assert(timing_sim->get_magic_number() == 0x5E5E5E5E5E5E5E5E);

		sc_core::sc_time delay = iss.cycle_time * cycles;

		iss.quantum_keeper.inc(delay);
	}
};
