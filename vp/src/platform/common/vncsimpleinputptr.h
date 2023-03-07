#ifndef RISCV_VP_VNCSIMPLEINPUTPTR_H
#define RISCV_VP_VNCSIMPLEINPUTPTR_H

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
class VNCSimpleInputPtr : public sc_core::sc_module, public VNCInputPtr_if {
   public:
	tlm_utils::simple_target_socket<VNCSimpleInputPtr> tsock;

	VNCSimpleInputPtr(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq);

	SC_HAS_PROCESS(VNCSimpleInputPtr);

	/* callback (VNCInputPtr_if) */
	void doPtr(int buttonMask, int x, int y);

	interrupt_gateway *plic;

   private:
	VNCServer &vncServer;
	std::mutex mutex;
	std::queue<std::tuple<int, int, int>> ptrEvents;

	uint32_t irq;
	bool interrupt;

	uint32_t reg_ctrl = 0;
	uint32_t reg_width = 0;
	uint32_t reg_height = 0;
	uint32_t reg_x = 0;
	uint32_t reg_y = 0;
	uint32_t reg_buttonmask = 0;

	void register_access_callback(const vp::map::register_access_t &r);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	void updateProcess();

	vp::map::LocalRouter router = {"VNCSimpleInputPtr"};
};

#endif /* RISCV_VP_VNCSIMPLEINPUTPTR_H */
