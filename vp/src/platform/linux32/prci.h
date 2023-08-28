#ifndef RISCV_VP_PRCI_H
#define RISCV_VP_PRCI_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

struct PRCI : public sc_core::sc_module {
	tlm_utils::simple_target_socket<PRCI> tsock;

	// memory mapped configuration registers
	uint32_t hfrosccfg = 0;
	uint32_t core_pllcfg0 = 0;
	uint32_t ddr_pllcfg0 = 0;
	uint32_t core_pllcfg1 = 0;
	uint32_t gemgxl_pllcfg0 = 0;
	uint32_t gemgxl_pllcfg1 = 0;
	uint32_t core_clksel = 0;
	uint32_t reset = 0;
	uint32_t clkmux_status = 0;

	enum {
		HFROSCCFG_REG_ADDR = 0x0,
		CORE_PLLCFG0_REG_ADDR = 0x4,
		DDR_PLLCFG0_REG_ADDR = 0x8,
		CORE_PLLCFG1_REG_ADDR = 0x10,
		GEMGXL_PLLCFG0_REG_ADDR = 0x1C,
		GEMGXL_PLLCFG1_REG_ADDR = 0x20,
		CORE_CLKSEL_REG_ADDR = 0x24,
		RESET_REG_ADDR = 0x28,
		CLKMUX_STATUS_REG_ADDR = 0x2C,
	};

	vp::map::LocalRouter router = {"PRCI"};

	PRCI(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &PRCI::transport);

		router
		    .add_register_bank({
		        {HFROSCCFG_REG_ADDR, &hfrosccfg},
		        {CORE_PLLCFG0_REG_ADDR, &core_pllcfg0},
		        {DDR_PLLCFG0_REG_ADDR, &ddr_pllcfg0},
		        {CORE_PLLCFG1_REG_ADDR, &core_pllcfg1},
		        {GEMGXL_PLLCFG0_REG_ADDR, &gemgxl_pllcfg0},
		        {GEMGXL_PLLCFG1_REG_ADDR, &gemgxl_pllcfg1},
		        {CORE_CLKSEL_REG_ADDR, &core_clksel},
		        {RESET_REG_ADDR, &reset},
		        {CLKMUX_STATUS_REG_ADDR, &clkmux_status},
		    })
		    .register_handler(this, &PRCI::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		r.fn();

		/* TODO: not implemented yet, this is a stub */
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};

#endif  // RISCV_VP_PRCI_H
