#ifndef RISCV_VP_VNCSIMPLEPTRINPUT_H
#define RISCV_VP_VNCSIMPLEPTRINPUT_H

#include <tlm_utils/simple_target_socket.h>

#include <mutex>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"
#include "util/vncserver.h"

/*
 * Simple ptr (mouse) input module
 */
class VNCSimplePtrInput : public sc_core::sc_module, public VNCInputPtr_if {
   public:
	tlm_utils::simple_target_socket<VNCSimplePtrInput> tsock;

	VNCSimplePtrInput(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq);

	SC_HAS_PROCESS(VNCSimplePtrInput);

	/* callback (VNCInputPtr_if) */
	void doPtr(int buttonMask, int x, int y);

	interrupt_gateway *plic;

   private:
	VNCServer &vncServer;
	std::mutex mutex;
	std::queue<std::tuple<int, int, int>> ptrEvents;

	uint32_t irq;
	bool interrupt;

	uint32_t reg_ctrl_ptr = 0;
	uint32_t reg_width_ptr = 0;
	uint32_t reg_height_ptr = 0;
	uint32_t reg_x_ptr = 0;
	uint32_t reg_y_ptr = 0;
	uint32_t reg_buttonmask_ptr = 0;

	void register_access_callback(const vp::map::register_access_t &r);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	void updateProcess();

	vp::map::LocalRouter router = {"VNCSimplePtrInput"};
};

#endif /* RISCV_VP_VNCSIMPLEPTRINPUT_H */
