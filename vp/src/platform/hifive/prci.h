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
	uint32_t hfxosccfg = 0;
	uint32_t pllcfg = 0;
	uint32_t plloutdiv = 0;

	enum {
		HFROSCCFG_REG_ADDR = 0x0,
		HFXOSCCFG_REG_ADDR = 0x4,
		PLLCFG_REG_ADDR = 0x8,
		PLLOUTDIV_REG_ADDR = 0xC,
	};

	vp::map::LocalRouter router = {"PRCI"};

	PRCI(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &PRCI::transport);

		router
		    .add_register_bank({
		        {HFROSCCFG_REG_ADDR, &hfrosccfg},
		        {HFXOSCCFG_REG_ADDR, &hfxosccfg},
		        {PLLCFG_REG_ADDR, &pllcfg},
		        {PLLOUTDIV_REG_ADDR, &plloutdiv},
		    })
		    .register_handler(this, &PRCI::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		/* Pretend that the crystal oscillator output is always ready. */
		if (r.read && r.vptr == &hfxosccfg)
			hfxosccfg = 1 << 31;

		r.fn();

		if ((r.vptr == &hfrosccfg) && r.nv)
			hfrosccfg |= 1 << 31;

		if ((r.vptr == &pllcfg) && r.nv)
			pllcfg |= 1 << 31;
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};

#endif  // RISCV_VP_PRCI_H
