#include "vncsimplefb.h"

#define REFRESH_RATE 30 /* Hz */
#define WIDTH 800
#define HEIGHT 480
#define BPP 2 /* rgb565 */
#define SIZE (WIDTH * HEIGHT * BPP)

/* Area tracking is more costly to simulation than full update in rfb thread */
//#define TRACK_CHANGED_AREA
#undef TRACK_CHANGED_AREA

VNCSimpleFB::VNCSimpleFB(sc_core::sc_module_name, VNCServer &vncServer) : vncServer(vncServer) {
	tsock.register_b_transport(this, &VNCSimpleFB::transport);

	areaChangedReset();

	vncServer.setScreenProperties(WIDTH, HEIGHT, 5, 3, BPP);

	router.add_start_size_mapping(0x00, SIZE, vp::map::read_write)
	    .register_handler(this, &VNCSimpleFB::fb_access_callback);

	SC_THREAD(updateProcess);
}

void VNCSimpleFB::fb_access_callback(tlm::tlm_generic_payload &trans, sc_core::sc_time) {
	uint32_t len = trans.get_data_length();
	uint32_t addr = trans.get_address();

	if (addr + len > SIZE) {
		/* out-of-bound access: ignore write; read random */
		return;
	}

	if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
		mutex.lock();

		/* update */
		memcpy(&frameBuffer[addr], trans.get_data_ptr(), len);

		/* keep track of and signal changed area */
		areaChangedSet(addr, len);

		mutex.unlock();

	} else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
		memcpy(trans.get_data_ptr(), &frameBuffer[addr], len);

	} else {
		throw std::runtime_error("unsupported TLM command detected");
	}
}

void VNCSimpleFB::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void VNCSimpleFB::areaChangedReset() {
	areaChanged = false;
#ifdef TRACK_CHANGED_AREA
	xMin = WIDTH;
	yMin = HEIGHT;
	xMax = 0;
	yMax = 0;
#endif
}

void VNCSimpleFB::areaChangedSet(uint32_t addr, uint32_t len) {
#ifdef TRACK_CHANGED_AREA
	uint32_t y_start = addr / (WIDTH * BPP);
	uint32_t x_start = (addr - (y_start * WIDTH * BPP)) / BPP;
	uint32_t y_end = (addr + len + BPP - 1) / (WIDTH * BPP);
	uint32_t x_end = ((addr + len + BPP - 1) - (y_end * WIDTH * BPP)) / BPP;
	xMin = std::min(xMin, x_start);
	yMin = std::min(yMin, y_start);
	xMax = std::max(xMax, x_end);
	yMax = std::max(yMax, y_end);
#endif
	areaChanged = true;
}

void VNCSimpleFB::updateScreen() {
	mutex.lock();

	if (!areaChanged) {
		mutex.unlock();
		return;
	}

#ifdef TRACK_CHANGED_AREA
	/* trigger update for modified area */
	vncServer.markRectAsModified(xMin, yMin, xMax + 1, yMax + 1);
#else
	/* trigger update for full framebuffer */
	vncServer.markRectAsModified(0, 0, WIDTH, HEIGHT);
#endif

	areaChangedReset();

	mutex.unlock();
}

void VNCSimpleFB::updateProcess() {
	vncServer.start();

	/* match location of color bytes to linux simpleframebuffer (rgb565) */
	rfbScreenInfoPtr rfbScreen = vncServer.getScreen();
	rfbScreen->serverFormat.redShift = 11;
	rfbScreen->serverFormat.greenShift = 5;
	rfbScreen->serverFormat.blueShift = 0;
	rfbScreen->serverFormat.redMax = (1 << 5) - 1;
	rfbScreen->serverFormat.greenMax = (1 << 6) - 1;
	rfbScreen->serverFormat.blueMax = (1 << 5) - 1;
	rfbScreen->serverFormat.bitsPerPixel = BPP * 8;
	rfbScreen->serverFormat.bigEndian = false;

	frameBuffer = vncServer.getFrameBuffer();

	while (vncServer.isActive()) {
		updateScreen();
		wait(1000000L / REFRESH_RATE, sc_core::SC_US);
	}

	vncServer.stop();
}
