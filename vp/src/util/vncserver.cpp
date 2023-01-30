#include "vncserver.h"

/*
 * static c callback helpers
 */
static void c_clientGone(rfbClientPtr cl) {
	VNCServer *vncServer = (VNCServer *)cl->screen->screenData;
	if (vncServer != nullptr) {
		vncServer->clientGone(cl);
	}
	cl->clientData = NULL;
}

static enum rfbNewClientAction c_newClient(rfbClientPtr cl) {
	VNCServer *vncServer = (VNCServer *)cl->screen->screenData;
	if (vncServer == nullptr) {
		/* should never happen */
		return RFB_CLIENT_REFUSE;
	}
	cl->clientGoneHook = c_clientGone;
	return vncServer->newClient(cl);
}

static void c_doPtr(int buttonMask, int x, int y, rfbClientPtr cl) {
	VNCInput_if *vncInput = (VNCInput_if *)cl->clientData;
	if (vncInput != nullptr) {
		vncInput->doPtr(buttonMask, x, y);
	}
	rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
}

static void c_doKbd(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	VNCInput_if *vncInput = (VNCInput_if *)cl->clientData;
	if (vncInput != nullptr) {
		vncInput->doKbd(down, key);
	}
}

VNCServer::VNCServer()
    : width(0), height(0), bitsPerSample(0), samplesPerPixel(0), bytesPerPixel(0), vncInput(nullptr) {}

VNCServer::~VNCServer(void) {
	stop();
}

void VNCServer::stop(void) {
	rfbShutdownServer(rfbScreen, true);
	delete[] rfbScreen->frameBuffer;
	rfbScreenCleanup(rfbScreen);
}

bool VNCServer::start(void) {
	rfbScreen = rfbGetScreen(nullptr, nullptr, width, height, bitsPerSample, samplesPerPixel, bytesPerPixel);
	if (!rfbScreen)
		return false;
	rfbScreen->desktopName = "RISCV-VP VNC Server";
	rfbScreen->screenData = (void *)this;
	rfbScreen->newClientHook = c_newClient;
	rfbScreen->ptrAddEvent = c_doPtr;
	rfbScreen->kbdAddEvent = c_doKbd;
	rfbScreen->frameBuffer = new char[width * height * bytesPerPixel];
	rfbScreen->alwaysShared = true;

	rfbInitServer(rfbScreen);
	rfbRunEventLoop(rfbScreen, -1, true);

	return true;
}

enum rfbNewClientAction VNCServer::newClient(rfbClientPtr cl) {
	cl->clientData = (void *)vncInput;
	return RFB_CLIENT_ACCEPT;
}

void VNCServer::clientGone(rfbClientPtr cl) {}
