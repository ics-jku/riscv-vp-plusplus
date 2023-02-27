#ifndef RISCV_VP_VNCSIMPLEINPUT_H
#define RISCV_VP_VNCSIMPLEINPUT_H

#include <tlm_utils/simple_target_socket.h>

#include <mutex>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"
#include "util/vncserver.h"

/*
 * Simple input module
 * (TODO: only pointer events yet; no keyboard)
 */
class VNCSimpleInput : public sc_core::sc_module, public VNCInput_if {
   public:
	tlm_utils::simple_target_socket<VNCSimpleInput> tsock;

	VNCSimpleInput(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq);

	SC_HAS_PROCESS(VNCSimpleInput);

	/* callbacks (VNCInput_if) */
	void doPtr(int buttonMask, int x, int y);
	void doKbd(rfbBool down, rfbKeySym key);

	interrupt_gateway *plic;

   private:
	VNCServer &vncServer;
	std::mutex mutex;
	std::queue<std::tuple<int, int, int>> ptrEvents;

	uint32_t irq;

	uint32_t reg_ctrl_ptr = 0;
	uint32_t reg_width_ptr = 0;
	uint32_t reg_height_ptr = 0;
	uint32_t reg_x_ptr = 0;
	uint32_t reg_y_ptr = 0;
	uint32_t reg_buttonmask_ptr = 0;

	void register_access_callback(const vp::map::register_access_t &r);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	void updateProcess();

	vp::map::LocalRouter router = {"VNCSimpleInput"};
};

#endif /* RISCV_VP_VNCSIMPLEINPUT_H */
