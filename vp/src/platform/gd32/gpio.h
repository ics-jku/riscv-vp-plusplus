#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>
#include <thread>

#include "afio.h"
#include "exmc.h"
#include "exti.h"
#include "gpio/gpio-server.hpp"
#include "nuclei_core/nuclei_irq_if.h"
#include "platform/common/async_event.h"
#include "spi.h"
#include "util/tlm_map.h"

struct GPIO : public sc_core::sc_module {
	tlm_utils::simple_target_socket<GPIO> tsock;

	uint32_t gpio_ctl0 = 0x44444444;
	uint32_t gpio_ctl1 = 0x44444444;
	uint32_t gpio_istat = 0x0000FFFF;  // 0x0000XXXX Don't care?
	uint32_t gpio_octl = 0x00000000;
	uint32_t gpio_bop = 0x00000000;
	uint32_t gpio_bc = 0x00000000;
	uint32_t gpio_lock = 0x00000000;

	enum {
		GPIO_CTL0_REG_ADDR = 0x00,
		GPIO_CTL1_REG_ADDR = 0x04,
		GPIO_ISTAT_REG_ADDR = 0x08,
		GPIO_OCTL_REG_ADDR = 0x0C,
		GPIO_BOP_REG_ADDR = 0x10,
		GPIO_BC_REG_ADDR = 0x14,
		GPIO_LOCK_REG_ADDR = 0x18,
	};

	vp::map::LocalRouter router = {"GPIO"};
	AFIO *afio = nullptr;
	EXTI *exti = nullptr;
	nuclei_interrupt_gateway *eclic = nullptr;

	static constexpr gpio::PinNumber available_pins = 16;
	GpioServer server;
	std::thread *serverThread;
	AsyncEvent asyncEvent;

	gpio::Port port;

	SC_HAS_PROCESS(GPIO);
	GPIO(sc_core::sc_module_name, gpio::Port port);
	~GPIO();

	void asyncOnchange(gpio::PinNumber bit, gpio::Tristate val);
	void synchronousChange();

	void register_access_callback(const vp::map::register_access_t &r);

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

	bool isServerConnected();

	SpiWriteFunction getSPIwriteFunction(gpio::PinNumber cs);
	ExmcWriteFunction getEXMCwriteFunction(gpio::PinNumber cs);
	PinWriteFunction getPINwriteFunction(gpio::PinNumber pin);
};
