/*
 * Copyright (C) 2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * A module that provides small miscellaneous functionality
 * Currently implemented:
 *  * reg 0 .. hardware random number generator (e.g. for linux timeriomem_rng)
 *
 * TODO:
 *  * Make the random number generation configurable to deliver real random
 *    numbers (like now) OR reproducable pseudo random numbers (for reproducable
 *    simulations)
 */

#ifndef RISCV_VP_MISCDEV_H
#define RISCV_VP_MISCDEV_H

extern "C" {
#include <sys/random.h>
}

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

struct MiscDev : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MiscDev> tsock;

	// memory mapped registers
	uint32_t hwrand_reg = 0;

	enum {
		HWRAND_REG_ADDR = 0x0,
	};

	vp::map::LocalRouter router = {"MiscDev"};

	MiscDev(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &MiscDev::transport);

		router
		    .add_register_bank({
		        {HWRAND_REG_ADDR, &hwrand_reg},
		    })
		    .register_handler(this, &MiscDev::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		if (r.read && r.vptr == &hwrand_reg) {
			// load a random value from the real Linux rng (/dev/urandom) to hwrand_reg
			if (getrandom(&hwrand_reg, sizeof(hwrand_reg), GRND_RANDOM) < 0) {
				throw std::system_error(errno, std::generic_category());
			}
		}

		r.fn();
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};

#endif  // RISCV_VP_MISCDEV_H
