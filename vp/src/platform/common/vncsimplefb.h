#ifndef RISCV_VP_VNCSIMPLEFB_H
#define RISCV_VP_VNCSIMPLEFB_H

#include <rfb/rfb.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "util/tlm_map.h"

/*
 * Simple framebuffer modules using libvncserver
 * (use with linux simple-framebuffer
 */
class VNCSimpleFB : public sc_core::sc_module {
   public:
	tlm_utils::simple_target_socket<VNCSimpleFB> tsock;

	VNCSimpleFB(sc_core::sc_module_name);
	~VNCSimpleFB(void);

	SC_HAS_PROCESS(VNCSimpleFB);

   private:
	rfbScreenInfoPtr rfbScreen;
	uint8_t *frameBuffer;
	sc_core::sc_mutex mutex;
	bool areaChanged;
	uint32_t xMin, yMin, xMax, yMax;

	void fb_access_callback(tlm::tlm_generic_payload &trans, sc_core::sc_time);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	void areaChangedReset();
	void areaChangedSet(uint32_t addr, uint32_t len);

	void updateRfbScreen();
	void updateProcess();

	vp::map::LocalRouter router = {"VNCSimpleFB"};
};

#endif /* RISCV_VP_VNCSIMPLEFB_H */
