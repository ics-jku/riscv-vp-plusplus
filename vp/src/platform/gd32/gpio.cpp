#include "gpio.h"

GPIO::GPIO(sc_core::sc_module_name) {
	tsock.register_b_transport(this, &GPIO::transport);

	router
	    .add_register_bank({
	        {GPIO_CTL0_REG_ADDR, &gpio_ctl0},
	        {GPIO_CTL1_REG_ADDR, &gpio_ctl1},
	        {GPIO_ISTAT_REG_ADDR, &gpio_istat},
	        {GPIO_OSTAT_REG_ADDR, &gpio_ostat},
	        {GPIO_BOP_REG_ADDR, &gpio_bop},
	        {GPIO_BC_REG_ADDR, &gpio_bc},
	        {GPIO_LOCK_REG_ADDR, &gpio_lock},
	    })
	    .register_handler(this, &GPIO::register_access_callback);
}

void GPIO::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void GPIO::register_access_callback(const vp::map::register_access_t &r) {}
