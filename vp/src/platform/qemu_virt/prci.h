#ifndef RISCV_VP_PRCI_H
#define RISCV_VP_PRCI_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

/*
 * only registers - no function
 * based on SiFive FU540-C000 Manual v1p4
 */
struct PRCI : public sc_core::sc_module {
	tlm_utils::simple_target_socket<PRCI> tsock;

	// memory mapped configuration registers
	uint32_t hfxosccfg = 0;
	uint32_t corepllcfg0 = 0;
	uint32_t ddrpllcfg0 = 0;
	uint32_t ddrpllcfg1 = 0;
	uint32_t gemgxlpllcfg0 = 0;
	uint32_t gemgxlpllcfg1 = 0;
	uint32_t coreclksel = 0;
	uint32_t devicesresetreg = 0;
	/* reserved */
	uint32_t clkmuxstatusreg = 0;
	uint32_t procmoncfg = 0;

	enum {
		HFXOSCCFG_REG_ADDR = 0x0,
		COREPLLCFG0_REG_ADDR = 0x4,
		DDRPLLCFG0_REG_ADDR = 0xC,
		DDRPLLCFG1_REG_ADDR = 0x10,
		GEMGXLPLLCFG0_REG_ADDR = 0x1C,
		GEMGXLPLLCFG1_REG_ADDR = 0x20,
		CORECLKSEL_REG_ADDR = 0x24,
		DEVICESRESETREG_REG_ADDR = 0x28,
		CLKMUXSTATUSREG_REG_ADDR = 0x2C,
		PROCMONCFG_REG_ADDR = 0xF0
	};

	vp::map::LocalRouter router = {"PRCI"};

	PRCI(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &PRCI::transport);

		router
		    .add_register_bank({
		        {HFXOSCCFG_REG_ADDR, &hfxosccfg},
		        {COREPLLCFG0_REG_ADDR, &corepllcfg0},
		        {DDRPLLCFG0_REG_ADDR, &ddrpllcfg0},
		        {DDRPLLCFG1_REG_ADDR, &ddrpllcfg1},
		        {GEMGXLPLLCFG0_REG_ADDR, &gemgxlpllcfg0},
		        {GEMGXLPLLCFG1_REG_ADDR, &gemgxlpllcfg1},
		        {CORECLKSEL_REG_ADDR, &coreclksel},
		        {DEVICESRESETREG_REG_ADDR, &devicesresetreg},
		        {CLKMUXSTATUSREG_REG_ADDR, &clkmuxstatusreg},
		        {PROCMONCFG_REG_ADDR, &procmoncfg},
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
