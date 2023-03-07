#ifndef RISCV_VP_VNCSIMPLEKBDINPUT_H
#define RISCV_VP_VNCSIMPLEKBDINPUT_H

#include <tlm_utils/simple_target_socket.h>

#include <mutex>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"
#include "util/vncserver.h"

/*
 * Simple kbd input module
 */
class VNCSimpleKbdInput : public sc_core::sc_module, public VNCInputKbd_if {
   public:
	tlm_utils::simple_target_socket<VNCSimpleKbdInput> tsock;

	VNCSimpleKbdInput(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq);

	SC_HAS_PROCESS(VNCSimpleKbdInput);

	/* callbacks (VNCInputKbd_if) */
	void doKbd(rfbBool down, rfbKeySym key);

	interrupt_gateway *plic;

   private:
	VNCServer &vncServer;
	std::mutex mutex;
	std::queue<uint32_t> kbdEvents;

	uint32_t irq;
	bool interrupt;

	uint32_t reg_ctrl = 0;
	uint32_t reg_key = 0;

	void register_access_callback(const vp::map::register_access_t &r);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	void updateProcess();

	vp::map::LocalRouter router = {"VNCSimpleKbdInput"};
};

#endif /* RISCV_VP_VNCSIMPLEKBDINPUT_H */
