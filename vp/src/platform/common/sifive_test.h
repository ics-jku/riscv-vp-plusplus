#ifndef RISCV_VP_SIFIVE_TEST_H
#define RISCV_VP_SIFIVE_TEST_H

#include <stdint.h>

#include <systemc>

#include "util/tlm_map.h"

/*
 * Inspired by qemu-riscv
 * Simple which allows stopping the simulation from within
 */
class SIFIVE_Test : public sc_core::sc_module {
   public:
	tlm_utils::simple_target_socket<SIFIVE_Test> tsock;

	SIFIVE_Test(const sc_core::sc_module_name &);
	~SIFIVE_Test(void);

	SC_HAS_PROCESS(SIFIVE_Test);

   private:
	void register_access_callback(const vp::map::register_access_t &);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	uint32_t reg_ctrl = 0;

	vp::map::LocalRouter router = {"SIFIVE_TEST"};
};

#endif /* RISCV_VP_SIFIVE_TEST_H */
