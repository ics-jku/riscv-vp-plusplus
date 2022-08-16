#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

struct AFIO : public sc_core::sc_module {
	tlm_utils::simple_target_socket<AFIO> tsock;

	// memory mapped configuration registers
	uint32_t afio_ec = 0x00;
	uint32_t afio_pcf0 = 0x00;
	uint32_t afio_extiss0 = 0x00;
	uint32_t afio_extiss1 = 0x00;
	uint32_t afio_extiss2 = 0x00;
	uint32_t afio_extiss3 = 0x00;
	uint32_t afio_pcf1 = 0x00;

	enum {
		AFIO_EC_REG_ADDR = 0x00,
		AFIO_PCF0_REG_ADDR = 0x04,
		AFIO_EXTISS0_REG_ADDR = 0x08,
		AFIO_EXTISS1_REG_ADDR = 0x0C,
		AFIO_EXTISS2_REG_ADDR = 0x10,
		AFIO_EXTISS3_REG_ADDR = 0x14,
		AFIO_PCF1_REG_ADDR = 0x1C,
	};

	vp::map::LocalRouter router = {"AFIO"};

	AFIO(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &AFIO::transport);

		router
		    .add_register_bank({
		        {AFIO_EC_REG_ADDR, &afio_ec},
		        {AFIO_PCF0_REG_ADDR, &afio_pcf0},
		        {AFIO_EXTISS0_REG_ADDR, &afio_extiss0},
		        {AFIO_EXTISS1_REG_ADDR, &afio_extiss1},
		        {AFIO_EXTISS2_REG_ADDR, &afio_extiss2},
		        {AFIO_EXTISS3_REG_ADDR, &afio_extiss3},
		        {AFIO_PCF1_REG_ADDR, &afio_pcf1},
		    })
		    .register_handler(this, &AFIO::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		r.fn();
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};
