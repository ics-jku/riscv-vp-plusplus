#ifndef RISCV_ISA_RANDOM_SOURCE_H
#define RISCV_ISA_RANDOM_SOURCE_H

#include <tlm_utils/simple_target_socket.h>

#include <cstdlib>
#include <systemc>

struct RandomSource : public sc_core::sc_module {
	tlm_utils::simple_target_socket<RandomSource> tsock;

	RandomSource(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &RandomSource::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		assert(trans.get_command() == tlm::TLM_READ_COMMAND);

		auto *p = trans.get_data_ptr();
		for (unsigned i = 0; i < trans.get_data_length(); ++i) {
			*(p + i) = rand();
		}
	}
};

#endif  // RISCV_ISA_RANDOM_SOURCE_H
