#ifndef RISCV_VP_USART_H
#define RISCV_VP_USART_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

class USART : public sc_core::sc_module {
   public:
	tlm_utils::simple_target_socket<USART> tsock;

	uint32_t usart_stat = 0xC0;
	uint32_t usart_data = 0x00;
	uint32_t usart_baud = 0x00;
	uint32_t usart_ctl0 = 0x00;
	uint32_t usart_ctl1 = 0x00;
	uint32_t usart_ctl2 = 0x00;
	uint32_t usart_gp = 0x00;

	enum {
		USART_STAT = 0x00,
		USART_DATA = 0x04,
		USART_BAUD = 0x08,
		USART_CTL0 = 0x0C,
		USART_CTL1 = 0x10,
		USART_CTL2 = 0x14,
		USART_GP = 0x18,
	};

	vp::map::LocalRouter router = {"USART"};
	bool initialized = false;

	USART(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &USART::transport);

		router
		    .add_register_bank({
		        {USART_STAT, &usart_stat},
		        {USART_DATA, &usart_data},
		        {USART_BAUD, &usart_baud},
		        {USART_CTL0, &usart_ctl0},
		        {USART_CTL1, &usart_ctl1},
		        {USART_CTL2, &usart_ctl2},
		        {USART_GP, &usart_gp},
		    })
		    .register_handler(this, &USART::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		r.fn();

		if (r.write && r.addr == USART_DATA) {
			uint8_t data = *r.vptr;
			if (!initialized) {
				initialized = true;
				if (data == 0)
					return;
			}
			std::cout << static_cast<char>(data);
			fflush(stdout);
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};

#endif  // RISCV_VP_USART_H
