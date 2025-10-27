#pragma once

#include "core/rv64/iss.h"
#include "rvfi_dii.h"

namespace cheriv9::rv64 {
template <class T_ISS>
struct RvfiDiiCoreRunner : public sc_core::sc_module {
	T_ISS& core;
	rvfi_dii_t rvfi_dii;
	std::string thread_name;
	TaggedMemory& mem;

	SC_HAS_PROCESS(RvfiDiiCoreRunner);
	RvfiDiiCoreRunner(T_ISS& core, uint16_t port, TaggedMemory& mem)
	    : sc_module(sc_core::sc_module_name(core.systemc_name.c_str())), core(core), rvfi_dii(port, mem), mem(mem) {
		thread_name = "run" + std::to_string(core.get_hart_id());
		SC_NAMED_THREAD(run, thread_name.c_str());
	}

	void run() {
		core.enable_debug();
		do {
			rvfi_dii.start(&core);
		} while (!rvfi_dii.quit);
	}
};
} /* namespace cheriv9::rv64 */
