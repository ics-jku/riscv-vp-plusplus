#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

struct EXTI : public sc_core::sc_module {
	tlm_utils::simple_target_socket<EXTI> tsock;

	// memory mapped configuration registers
	uint32_t exti_inten = 0x00;
	uint32_t exti_even = 0x00;
	uint32_t exti_rten = 0x00;
	uint32_t exti_ften = 0x00;
	uint32_t exti_swiev = 0x00;
	uint32_t exti_pd = 0x00;

	enum {
		EXTI_INTEN_REG_ADDR = 0x00,
		EXTI_EVEN_REG_ADDR = 0x04,
		EXTI_RTEN_REG_ADDR = 0x08,
		EXTI_FTEN_REG_ADDR = 0x0C,
		EXTI_SWIEV_REG_ADDR = 0x10,
		EXTI_PD_REG_ADDR = 0x14,
	};

	vp::map::LocalRouter router = {"EXTI"};

	EXTI(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &EXTI::transport);

		router
		    .add_register_bank({
		        {EXTI_INTEN_REG_ADDR, &exti_inten},
		        {EXTI_EVEN_REG_ADDR, &exti_even},
		        {EXTI_RTEN_REG_ADDR, &exti_rten},
		        {EXTI_FTEN_REG_ADDR, &exti_ften},
		        {EXTI_SWIEV_REG_ADDR, &exti_swiev},
		        {EXTI_PD_REG_ADDR, &exti_pd},
		    })
		    .register_handler(this, &EXTI::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		if (r.write) {
			if (r.vptr == &exti_pd) {
				exti_pd &= ~r.nv;  // writting 1 to a bit in the pd registers, clears the interrupt
				return;
			}
		}
		r.fn();
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};
