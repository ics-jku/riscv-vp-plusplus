#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "gpio/gpiocommon.hpp"
#include "util/tlm_map.h"

#define LCD_CMD_ADDR 0x00000000
#define LCD_DAT_ADDR 0x03FFFFFE

typedef std::function<uint16_t(uint16_t)> ExmcWriteFunction;
typedef std::function<void(gpio::Tristate)> PinWriteFunction;

struct EXMC : public sc_core::sc_module {
	tlm_utils::simple_target_socket<EXMC> tsock_internal;
	tlm_utils::simple_target_socket<EXMC> tsock_external;

	ExmcWriteFunction writeFunctionEXMC;
	PinWriteFunction writeFunctionPIN;

	sc_dt::uint64 addr_cache;

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
		r.fn();
	}

	void transport_internal(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}

	void transport_external(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		if (addr != addr_cache) {
			if (addr == LCD_CMD_ADDR)
				writeFunctionPIN(gpio::Tristate::LOW);
			else if (addr == LCD_DAT_ADDR)
				writeFunctionPIN(gpio::Tristate::HIGH);
			addr_cache = addr;
		}

		writeFunctionEXMC(*(uint16_t *)trans.get_data_ptr());
	}

	void connect(ExmcWriteFunction interface) {
		writeFunctionEXMC = interface;
	}
	void connect(PinWriteFunction interface) {
		writeFunctionPIN = interface;
	}
};
