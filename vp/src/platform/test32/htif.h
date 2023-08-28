#pragma once

#include <systemc>

struct HTIF : public sc_core::sc_module {
	uint64_t *to_host;
	uint64_t *num_instr;
	uint64_t max_instrs;
	std::function<void(uint64_t)> to_host_callback;

	SC_HAS_PROCESS(HTIF);
	HTIF(sc_core::sc_module_name, uint64_t *to_host, uint64_t *num_instr, uint64_t max_instrs,
	     std::function<void(uint64_t)> to_host_callback) {
		this->to_host = to_host;
		this->num_instr = num_instr;
		this->max_instrs = max_instrs;
		this->to_host_callback = to_host_callback;
		SC_THREAD(run);
	}

	void run() {
		while (true) {
			sc_core::wait(10, sc_core::SC_US);
			auto x = *to_host;
			to_host_callback(x);
			x = *num_instr;
			if (x >= max_instrs)
				throw std::runtime_error("reached >= max #instrs: " + std::to_string(x));
		}
	}
};
