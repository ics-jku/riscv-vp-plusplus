#include "vncserver.h"

#include <linux/input-event-codes.h>
#include <rfb/keysym.h>

#include <iostream>
#include <unordered_map>
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

/*
 * rfb keysyms (X11) to linux key codes
 */
static const std::unordered_map<uint32_t, uint32_t> rfbKeySym2LinuxKeyCode = {
    {XK_Super_L, KEY_LEFTMETA},
    {XK_Super_R, KEY_RIGHTMETA},
    {XK_Escape, KEY_ESC},
    {XK_1, KEY_1},
    {XK_2, KEY_2},
    {XK_3, KEY_3},
    {XK_4, KEY_4},
    {XK_5, KEY_5},
    {XK_6, KEY_6},
    {XK_7, KEY_7},
    {XK_8, KEY_8},
    {XK_9, KEY_9},
    {XK_0, KEY_0},
    {XK_exclam, KEY_1},
    {XK_at, KEY_2},
    {XK_numbersign, KEY_3},
    {XK_dollar, KEY_4},
    {XK_percent, KEY_5},
    {XK_asciicircum, KEY_6},
    {XK_ampersand, KEY_7},
    {XK_asterisk, KEY_8},
    {XK_parenleft, KEY_9},
    {XK_parenright, KEY_0},
    {XK_minus, KEY_MINUS},
    {XK_underscore, KEY_MINUS},
    {XK_equal, KEY_EQUAL},
    {XK_plus, KEY_EQUAL},
    {XK_BackSpace, KEY_BACKSPACE},
    {XK_Tab, KEY_TAB},
    {XK_q, KEY_Q},
    {XK_Q, KEY_Q},
    {XK_w, KEY_W},
    {XK_W, KEY_W},
    {XK_e, KEY_E},
    {XK_E, KEY_E},
    {XK_r, KEY_R},
    {XK_R, KEY_R},
    {XK_t, KEY_T},
    {XK_T, KEY_T},
    {XK_y, KEY_Y},
    {XK_Y, KEY_Y},
    {XK_u, KEY_U},
    {XK_U, KEY_U},
    {XK_i, KEY_I},
    {XK_I, KEY_I},
    {XK_o, KEY_O},
    {XK_O, KEY_O},
    {XK_p, KEY_P},
    {XK_P, KEY_P},
    {XK_braceleft, KEY_LEFTBRACE},
    {XK_braceright, KEY_RIGHTBRACE},
    {XK_bracketleft, KEY_LEFTBRACE},
    {XK_bracketright, KEY_RIGHTBRACE},
    {XK_Return, KEY_ENTER},
    {XK_Control_L, KEY_LEFTCTRL},
    {XK_a, KEY_A},
    {XK_A, KEY_A},
    {XK_s, KEY_S},
    {XK_S, KEY_S},
    {XK_d, KEY_D},
    {XK_D, KEY_D},
    {XK_f, KEY_F},
    {XK_F, KEY_F},
    {XK_g, KEY_G},
    {XK_G, KEY_G},
    {XK_h, KEY_H},
    {XK_H, KEY_H},
    {XK_j, KEY_J},
    {XK_J, KEY_J},
    {XK_k, KEY_K},
    {XK_K, KEY_K},
    {XK_l, KEY_L},
    {XK_L, KEY_L},
    {XK_semicolon, KEY_SEMICOLON},
    {XK_colon, KEY_SEMICOLON},
    {XK_apostrophe, KEY_APOSTROPHE},
    {XK_quotedbl, KEY_APOSTROPHE},
    {XK_grave, KEY_GRAVE},
    {XK_asciitilde, KEY_GRAVE},
    {XK_Shift_L, KEY_LEFTSHIFT},
    {XK_backslash, KEY_BACKSLASH},
    {XK_bar, KEY_BACKSLASH},
    {XK_z, KEY_Z},
    {XK_Z, KEY_Z},
    {XK_x, KEY_X},
    {XK_X, KEY_X},
    {XK_c, KEY_C},
    {XK_C, KEY_C},
    {XK_v, KEY_V},
    {XK_V, KEY_V},
    {XK_b, KEY_B},
    {XK_B, KEY_B},
    {XK_n, KEY_N},
    {XK_N, KEY_N},
    {XK_m, KEY_M},
    {XK_M, KEY_M},
    {XK_comma, KEY_COMMA},
    {XK_less, KEY_COMMA},
    {XK_period, KEY_DOT},
    {XK_greater, KEY_DOT},
    {XK_slash, KEY_SLASH},
    {XK_question, KEY_SLASH},
    {XK_Shift_R, KEY_RIGHTSHIFT},
    {XK_KP_Multiply, KEY_KPASTERISK},
    {XK_Alt_L, KEY_LEFTALT},
    {XK_space, KEY_SPACE},
    {XK_Caps_Lock, KEY_CAPSLOCK},
    {XK_F1, KEY_F1},
    {XK_F2, KEY_F2},
    {XK_F3, KEY_F3},
    {XK_F4, KEY_F4},
    {XK_F5, KEY_F5},
    {XK_F6, KEY_F6},
    {XK_F7, KEY_F7},
    {XK_F8, KEY_F8},
    {XK_F9, KEY_F9},
    {XK_F10, KEY_F10},
    {XK_Num_Lock, KEY_NUMLOCK},
    {XK_Scroll_Lock, KEY_SCROLLLOCK},
    {XK_KP_7, KEY_KP7},
    {XK_KP_8, KEY_KP8},
    {XK_KP_9, KEY_KP9},
    {XK_KP_Subtract, KEY_KPMINUS},
    {XK_KP_4, KEY_KP4},
    {XK_KP_5, KEY_KP5},
    {XK_KP_6, KEY_KP6},
    {XK_KP_Add, KEY_KPPLUS},
    {XK_KP_1, KEY_KP1},
    {XK_KP_2, KEY_KP2},
    {XK_KP_3, KEY_KP3},
    {XK_KP_0, KEY_KP0},
    {XK_KP_Decimal, KEY_KPDOT},
    {XK_F13, KEY_F13},
    {XK_F11, KEY_F11},
    {XK_F12, KEY_F12},
    {XK_F14, KEY_F14},
    {XK_F15, KEY_F15},
    {XK_F16, KEY_F16},
    {XK_F17, KEY_F17},
    {XK_F18, KEY_F18},
    {XK_F19, KEY_F19},
    {XK_F20, KEY_F20},
    {XK_KP_Enter, KEY_KPENTER},
    {XK_Control_R, KEY_RIGHTCTRL},
    {XK_KP_Divide, KEY_KPSLASH},
    {XK_Sys_Req, KEY_SYSRQ},
    {XK_Alt_R, KEY_RIGHTALT},
    {XK_Linefeed, KEY_LINEFEED},
    {XK_Home, KEY_HOME},
    {XK_Up, KEY_UP},
    {XK_Page_Up, KEY_PAGEUP},
    {XK_Left, KEY_LEFT},
    {XK_Right, KEY_RIGHT},
    {XK_End, KEY_END},
    {XK_Down, KEY_DOWN},
    {XK_Page_Down, KEY_PAGEDOWN},
    {XK_Insert, KEY_INSERT},
    {XK_Delete, KEY_DELETE},
    {XK_KP_Equal, KEY_KPEQUAL},
    {XK_Pause, KEY_PAUSE},
    {XK_F21, KEY_F21},
    {XK_F22, KEY_F22},
    {XK_F23, KEY_F23},
    {XK_F24, KEY_F24},
    {XK_KP_Separator, KEY_KPCOMMA},
    {XK_Meta_L, KEY_LEFTMETA},
    {XK_Meta_R, KEY_RIGHTMETA},
    {XK_Multi_key, KEY_COMPOSE},
};

/*
 * VNCServer
 */
void VNCServer::stop(void) {
	rfbShutdownServer(rfbScreen, true);
	delete[] rfbScreen->frameBuffer;
	rfbScreenCleanup(rfbScreen);
}

bool VNCServer::start(void) {
	rfbScreen = rfbGetScreen(nullptr, nullptr, width, height, bitsPerSample, samplesPerPixel, bytesPerPixel);
	if (!rfbScreen)
		return false;
	rfbScreen->port = vncPort;
	rfbScreen->ipv6port = vncPort;
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

uint32_t VNCServer::keySymToLinuxKeyCode(uint32_t keySym) {
	auto ret = rfbKeySym2LinuxKeyCode.find(keySym);
	if (ret == rfbKeySym2LinuxKeyCode.end()) {
		/* NOT SUPPORTED */
		return 0;
	}
	return ret->second;
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
