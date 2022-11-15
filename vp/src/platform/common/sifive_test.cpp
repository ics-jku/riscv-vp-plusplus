#include "sifive_test.h"

enum {
	REG_CTRL_ADDR = 0x0,
};

SIFIVE_Test::SIFIVE_Test(const sc_core::sc_module_name &) {
	tsock.register_b_transport(this, &SIFIVE_Test::transport);
	router
	    .add_register_bank({
	        {REG_CTRL_ADDR, &reg_ctrl},
	    })
	    .register_handler(this, &SIFIVE_Test::register_access_callback);
}

SIFIVE_Test::~SIFIVE_Test(void) {}

void SIFIVE_Test::register_access_callback(const vp::map::register_access_t &r) {
	if (r.write || r.read) {
		if (r.vptr == &reg_ctrl) {
			/* do nothing */
		} else {
			std::cerr << "invalid offset for SIFIVE_TEST " << std::endl;
		}
	}

	r.fn();

	if (r.write && r.vptr == &reg_ctrl) {
		if (reg_ctrl == 0x5555) {
			std::cout << "SIFIVE_Test: Received poweroff -> stop" << std::endl;
			sc_core::sc_stop();
		} else if (reg_ctrl == 0x7777) {
			std::cout << "SIFIVE_Test: Received reboot -> stop" << std::endl;
			/* reboot not implemented in vp -> stop */
			sc_core::sc_stop();
		} else {
			std::cerr << "invalid value for SIFIVE_TEST reg_ctrl: 0x" << std::hex << reg_ctrl << std::endl;
		}
	}
}

void SIFIVE_Test::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}
