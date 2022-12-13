#pragma once

#include "platform/common/async_event.h"
#include "core/common/irq_if.h"
#include <gpio-server.hpp>
#include "util/tlm_map.h"
#include "spi.h"
#include "tunnel-uart.hpp"

#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <thread>

struct GPIO : public sc_core::sc_module {
	tlm_utils::simple_target_socket<GPIO> tsock;

	// memory mapped configuration registers
	uint32_t value = 0;		// Current state of pin, input or output
	uint32_t input_en = 0;
	uint32_t output_en = 0;
	uint32_t port = 0;		// Desired output values of enabled pins
	uint32_t pullup_en = 0;
	uint32_t pin_drive_strength = 0;
	uint32_t rise_intr_en = 0;
	uint32_t rise_intr_pending = ~0l;
	uint32_t fall_intr_en = 0;
	uint32_t fall_intr_pending = ~0l;
	uint32_t high_intr_en = 0;
	uint32_t high_intr_pending = ~0l;
	uint32_t low_intr_en = 0;
	uint32_t low_intr_pending = ~0l;
	uint32_t iof_en = 0;
	uint32_t iof_sel = 0;
	uint32_t out_xor = 0;

	enum {
		PIN_VALUE_ADDR = 0x000,
		INPUT_EN_REG_ADDR = 0x004,
		OUTPUT_EN_REG_ADDR = 0x008,
		PORT_REG_ADDR = 0x00C,
		PULLUP_EN_ADDR = 0x010,
		PIN_DRIVE_STNGTH = 0x014,
		RISE_INTR_EN = 0x018,
		RISE_INTR_PEND = 0x01C,
		FALL_INTR_EN = 0x020,
		FALL_INTR_PEND = 0x024,
		HIGH_INTR_EN = 0x028,
		HIGH_INTR_PEND = 0x02C,
		LOW_INTR_EN = 0x030,
		LOW_INTR_PEND = 0x034,
		IOF_EN_REG_ADDR = 0x038,
		IOF_SEL_REG_ADDR = 0x03C,
		OUT_XOR_REG_ADDR = 0x040,
	};

	vp::map::LocalRouter router = {"GPIO"};
	interrupt_gateway *plic = nullptr;

	static constexpr gpio::PinNumber available_pins = 32;
	const unsigned int_gpio_base;
	GpioServer server;
	std::thread *serverThread;
	AsyncEvent asyncEvent;

	SC_HAS_PROCESS(GPIO);
	GPIO(sc_core::sc_module_name, unsigned int_gpio_base);
	~GPIO();

	void register_access_callback(const vp::map::register_access_t &r);

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

	bool isServerConnected();

	void asyncOnchange(gpio::PinNumber bit, gpio::Tristate val);
	void synchronousChange();

	UartTXFunction getUartTransmitFunction(gpio::PinNumber tx);
	void registerUartReceiveFunction(gpio::PinNumber rx, UartRXFunction);

	SpiWriteFunction getSPIwriteFunction(gpio::PinNumber cs);
};
