#include "vncserver.h"

/*
 * static c callback helpers
 */
static void c_clientGone(rfbClientPtr cl) {
	VNCServer *vncServer = (VNCServer *)cl->screen->screenData;
	if (vncServer == nullptr) {
		return;
	}
	vncServer->clientGone(cl);
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
	VNCServer *vncServer = (VNCServer *)cl->screen->screenData;
	if (vncServer == nullptr) {
		return;
	}
	vncServer->doPtr(cl, buttonMask, x, y);
}

static void c_doKbd(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	VNCServer *vncServer = (VNCServer *)cl->screen->screenData;
	if (vncServer == nullptr) {
		return;
	}
	vncServer->doKbd(cl, down, key);
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
	rfbScreen->desktopName = desktopName;
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
	return RFB_CLIENT_ACCEPT;
}

void VNCServer::clientGone(rfbClientPtr cl) {}

void VNCServer::doPtr(rfbClientPtr cl, int buttonMask, int x, int y) {
	if (vncInputPtr != nullptr) {
		vncInputPtr->doPtr(buttonMask, x, y);
	}
	rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
}

void VNCServer::doKbd(rfbClientPtr cl, rfbBool down, rfbKeySym key) {
	if (vncInputKbd == nullptr)
		return;
	vncInputKbd->doKbd(down, key);
}
