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

VNCServer::VNCServer() : width(0), height(0), bitsPerSample(0), samplesPerPixel(0), bytesPerPixel(0) {}

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
	rfbScreen->frameBuffer = new char[width * height * bytesPerPixel];
	rfbScreen->alwaysShared = true;

	rfbInitServer(rfbScreen);
	rfbRunEventLoop(rfbScreen, -1, true);

	return true;
}

enum rfbNewClientAction VNCServer::newClient(rfbClientPtr cl) {
	return RFB_CLIENT_ACCEPT;
}

void VNCServer::clientGone(rfbClientPtr cl) {}
