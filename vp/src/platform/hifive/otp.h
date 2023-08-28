#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "irq_if.h"
#include "tlm_map.h"

struct OTP : public sc_core::sc_module {
	tlm_utils::simple_target_socket<OTP> tsock;

	OTP(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &OTP::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		assert(false && "OTP not yet implemented");
		return;
	}
};
