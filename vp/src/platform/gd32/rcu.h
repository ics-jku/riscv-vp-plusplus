#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

struct RCU : public sc_core::sc_module {
	tlm_utils::simple_target_socket<RCU> tsock;

	// memory mapped configuration registers
	uint32_t rcu_ctl = 0x83;
	uint32_t rcu_cfg0 = 0x00;
	uint32_t rcu_int = 0x00;
	uint32_t rcu_apb2rst = 0x00;
	uint32_t rcu_apb1rst = 0x00;
	uint32_t rcu_ahben = 0x14;
	uint32_t rcu_apb2en = 0x00;
	uint32_t rcu_apb1en = 0x00;
	uint32_t rcu_bdctl = 0x18;
	uint32_t rcu_rstsck = 0x0C000000;
	uint32_t rcu_ahbrst = 0x00;
	uint32_t rcu_cfg1 = 0x00;
	uint32_t rcu_dsv = 0x00;

	enum {
		RCU_CTL_REG_ADDR = 0x00,
		RCU_CFG0_REG_ADDR = 0x04,
		RCU_INT_REG_ADDR = 0x08,
		RCU_APB2RST_REG_ADDR = 0x0C,
		RCU_APB1RST_REG_ADDR = 0x10,
		RCU_AHBEN_REG_ADDR = 0x14,
		RCU_APB2EN_REG_ADDR = 0x18,
		RCU_APB1EN_REG_ADDR = 0x1C,
		RCU_BDCTL_REG_ADDR = 0x20,
		RCU_RSTSCK_REG_ADDR = 0x24,
		RCU_AHBRST_REG_ADDR = 0x28,
		RCU_CFG1_REG_ADDR = 0x2C,
		RCU_DSV_REG_ADDR = 0x34,
	};

	vp::map::LocalRouter router = {"RCU"};

	RCU(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &RCU::transport);

		router
		    .add_register_bank({
		        {RCU_CTL_REG_ADDR, &rcu_ctl},
		        {RCU_CFG0_REG_ADDR, &rcu_cfg0},
		        {RCU_INT_REG_ADDR, &rcu_int},
		        {RCU_APB2RST_REG_ADDR, &rcu_apb2rst},
		        {RCU_APB1RST_REG_ADDR, &rcu_apb1rst},
		        {RCU_AHBEN_REG_ADDR, &rcu_ahben},
		        {RCU_APB2EN_REG_ADDR, &rcu_apb2en},
		        {RCU_APB1EN_REG_ADDR, &rcu_apb1en},
		        {RCU_BDCTL_REG_ADDR, &rcu_bdctl},
		        {RCU_RSTSCK_REG_ADDR, &rcu_rstsck},
		        {RCU_AHBRST_REG_ADDR, &rcu_ahbrst},
		        {RCU_CFG1_REG_ADDR, &rcu_cfg1},
		        {RCU_DSV_REG_ADDR, &rcu_dsv},
		    })
		    .register_handler(this, &RCU::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		/* Pretend that the IRC8M oscillator, the HXTAL oscillator,
		and the PLL output clocks are always stable and ready for use */
		if (r.read && r.vptr == &rcu_ctl) {
			rcu_ctl |= 1 << 1;   // IRC8M oscillator
			rcu_ctl |= 1 << 17;  // HXTAL oscillator
			rcu_ctl |= 1 << 25;  // PLL output clock
			rcu_ctl |= 1 << 27;  // PLL1 output clock
			rcu_ctl |= 1 << 29;  // PLL2 output clock
		}

		/* select CK_PLL as the CK_SYS source */
		if (r.read && r.vptr == &rcu_cfg0) {
			rcu_cfg0 |= 2 << 2;  // CK_PLL
		}

		r.fn();
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};
