#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "gpio/gpiocommon.hpp"
#include "util/tlm_map.h"

#define LCD_CMD_ADDR 0x00000000
#define LCD_DAT_ADDR 0x03FFFFFE

typedef std::function<void(uint16_t)> ExmcWriteFunction;
typedef std::function<void(gpio::Tristate)> PinWriteFunction;

struct EXMC : public sc_core::sc_module {
	tlm_utils::simple_target_socket<EXMC> tsock_internal;
	tlm_utils::simple_target_socket<EXMC> tsock_external;

	ExmcWriteFunction writeFunctionEXMC;
	PinWriteFunction writeFunctionPIN;

	// memory mapped configuration registers
	uint32_t exmc_snctl0 = 0x000030DA;
	uint32_t exmc_sntcfg0 = 0x0FFFFFFF;

	enum {
		EXMC_SNCTL0_REG_ADDR = 0x00,
		EXMC_SNTCFG0_REG_ADDR = 0x04,
	};

	vp::map::LocalRouter router = {"EXMC"};

	EXMC(sc_core::sc_module_name) {
		tsock_internal.register_b_transport(this, &EXMC::transport_internal);
		tsock_external.register_b_transport(this, &EXMC::transport_external);

		router
		    .add_register_bank({
		        {EXMC_SNCTL0_REG_ADDR, &exmc_snctl0},
		        {EXMC_SNTCFG0_REG_ADDR, &exmc_sntcfg0},
		    })
		    .register_handler(this, &EXMC::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		// std::cout << "EXMC cb addr:" << r.addr << " read: " << r.read << " nv: " << r.nv << std::endl;
		r.fn();
	}

	void transport_internal(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		// std::cout << "EXMC t_int addr:" << trans.get_address() << " read: " << trans.is_read() << " nv: " <<
		// *trans.get_data_ptr() << std::endl;
		router.transport(trans, delay);
	}

	void transport_external(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		std::cout << "EXMC t_ext addr:" << trans.get_address() << " read: " << trans.is_read() << " nv: " << std::hex
		          << *trans.get_data_ptr() << std::endl;
		auto addr = trans.get_address();

		if (addr == LCD_CMD_ADDR)
			writeFunctionPIN(gpio::Tristate::LOW);
		else if (addr == LCD_DAT_ADDR)
			writeFunctionPIN(gpio::Tristate::HIGH);

		writeFunctionEXMC(*trans.get_data_ptr());
	}

	void connect(ExmcWriteFunction interface) {
		writeFunctionEXMC = interface;
	}
	void connect(PinWriteFunction interface) {
		writeFunctionPIN = interface;
	}
};
