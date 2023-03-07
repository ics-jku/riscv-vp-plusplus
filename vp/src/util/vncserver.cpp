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
static const std::unordered_map<uint32_t, uint32_t> RFB2LINUX = {
    {XK_0, KEY_0},
    {XK_1, KEY_1},
    {XK_2, KEY_2},
    {XK_3, KEY_3},
    {XK_4, KEY_4},
    {XK_5, KEY_5},
    {XK_6, KEY_6},
    {XK_7, KEY_7},
    {XK_8, KEY_8},
    {XK_9, KEY_9},

    {XK_A, KEY_A},
    {XK_B, KEY_B},
    {XK_C, KEY_C},
    {XK_D, KEY_D},
    {XK_E, KEY_E},
    {XK_F, KEY_F},
    {XK_G, KEY_G},
    {XK_H, KEY_H},
    {XK_I, KEY_I},
    {XK_J, KEY_J},
    {XK_K, KEY_K},
    {XK_L, KEY_L},
    {XK_M, KEY_M},
    {XK_N, KEY_N},
    {XK_O, KEY_O},
    {XK_P, KEY_P},
    {XK_Q, KEY_Q},
    {XK_R, KEY_R},
    {XK_S, KEY_S},
    {XK_T, KEY_T},
    {XK_U, KEY_U},
    {XK_V, KEY_V},
    {XK_W, KEY_W},
    {XK_X, KEY_X},
    {XK_Y, KEY_Y},
    {XK_Z, KEY_Z},

    /* handled by shift */
    {XK_a, KEY_A},
    {XK_b, KEY_B},
    {XK_c, KEY_C},
    {XK_d, KEY_D},
    {XK_e, KEY_E},
    {XK_f, KEY_F},
    {XK_g, KEY_G},
    {XK_h, KEY_H},
    {XK_i, KEY_I},
    {XK_j, KEY_J},
    {XK_k, KEY_K},
    {XK_l, KEY_L},
    {XK_m, KEY_M},
    {XK_n, KEY_N},
    {XK_o, KEY_O},
    {XK_p, KEY_P},
    {XK_q, KEY_Q},
    {XK_r, KEY_R},
    {XK_s, KEY_S},
    {XK_t, KEY_T},
    {XK_u, KEY_U},
    {XK_v, KEY_V},
    {XK_w, KEY_W},
    {XK_x, KEY_X},
    {XK_y, KEY_Y},
    {XK_z, KEY_Z},

#if 0
    { XK_exclam, KEY_EXCLAIM },
    { XK_quotedbl, KEY_DBLQUOTE },
    { XK_numbersign, KEY_HASH },
#endif
    {XK_dollar, KEY_DOLLAR},
#if 0
    { XK_percent, KEY_PERCENT },
    { XK_ampersand, KEY_AMPERSAND },
    { XK_apostrophe, KEY_QUOTE },
    { XK_parenleft, KEY_LEFTPAR },
    { XK_parenright, KEY_RIGHTPAR },
    { XK_asterisk, KEY_ASTERISK },
    { XK_plus, KEY_PLUS },
#endif
    {XK_comma, KEY_COMMA},
    {XK_minus, KEY_MINUS},
    {XK_period, KEY_DOT},
    {XK_slash, KEY_SLASH},
#if 0
    { XK_colon, KEY_COLON },
#endif
    {XK_semicolon, KEY_SEMICOLON},
#if 0
    { XK_less, KEY_LESS },
#endif
    {XK_equal, KEY_EQUAL},
#if 0
    { XK_greater, KEY_GREATER },
#endif
    {XK_question, KEY_QUESTION},
#if 0
    { XK_at, KEY_AT },
    { XK_bracketleft, KEY_LEFTBRACKET },
#endif
    {XK_backslash, KEY_BACKSLASH},
#if 0
    { XK_bracketright, KEY_RIGHTBRACKET },
    { XK_asciicircum, KEY_CARET },
    { XK_underscore, KEY_UNDERSCORE },
    { XK_quoteleft, KEY_BACKQUOTE },
#endif
    {XK_braceleft, KEY_LEFTBRACE},
#if 0
    { XK_bar, KEY_PIPE },
#endif
    {XK_braceright, KEY_RIGHTBRACE},
#if 0
    { XK_asciitilde, KEY_TILDE },
#endif

    {XK_Escape, KEY_ESC},
    {XK_Return, KEY_ENTER},
    {XK_BackSpace, KEY_BACKSPACE},
    {XK_space, KEY_SPACE},
    {XK_Tab, KEY_TAB},
    {XK_Shift_L, KEY_LEFTSHIFT},
    {XK_Shift_R, KEY_RIGHTSHIFT},
    {XK_Control_L, KEY_LEFTCTRL},
    {XK_Control_R, KEY_RIGHTCTRL},
    {XK_Alt_L, KEY_LEFTALT},
    {XK_Alt_R, KEY_RIGHTALT},
    {XK_Meta_L, KEY_LEFTMETA},
    {XK_Meta_R, KEY_RIGHTMETA},
    {XK_Menu, KEY_MENU},
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
    {XK_F11, KEY_F11},
    {XK_F12, KEY_F12},

    {XK_Print, KEY_PRINT},
#if 0
    { XK_Scroll_Lock, KEY_SCROLLOCK },
#endif
    {XK_Pause, KEY_PAUSE},

    {XK_Insert, KEY_INSERT},
    {XK_Delete, KEY_DELETE},
    {XK_Home, KEY_HOME},
    {XK_End, KEY_END},
    {XK_Page_Up, KEY_PAGEUP},
    {XK_Page_Down, KEY_PAGEDOWN},

    {XK_Left, KEY_LEFT},
    {XK_Right, KEY_RIGHT},
    {XK_Up, KEY_UP},
    {XK_Down, KEY_DOWN},

    {XK_Num_Lock, KEY_NUMLOCK},
    {XK_KP_0, KEY_KP0},
    {XK_KP_1, KEY_KP1},
    {XK_KP_2, KEY_KP2},
    {XK_KP_3, KEY_KP3},
    {XK_KP_4, KEY_KP4},
    {XK_KP_5, KEY_KP5},
    {XK_KP_6, KEY_KP6},
    {XK_KP_7, KEY_KP7},
    {XK_KP_8, KEY_KP8},
    {XK_KP_9, KEY_KP9},
    {XK_KP_Enter, KEY_KPENTER},
    {XK_KP_Add, KEY_KPPLUS},
    {XK_KP_Subtract, KEY_KPMINUS},
#if 0
    { XK_KP_Multiply, KEY_KPMUL },
    { XK_KP_Divide, KEY_KPDIV },
    { XK_KP_Separator, KEY_KPDOT },
    { XK_KP_Up, KEY_KPUP },
    { XK_KP_Down, KEY_KPDOWN },
    { XK_KP_Left, KEY_KPLEFT },
    { XK_KP_Right, KEY_KPRIGHT },
    { XK_KP_Home, KEY_KPHOME },
    { XK_KP_End, KEY_KPEND },
    { XK_KP_Page_Up, KEY_KPPAGEUP },
    { XK_KP_Page_Down, KEY_KPPAGEDOWN },
    { XK_KP_Insert, KEY_KPINSERT },
    { XK_KP_Delete, KEY_KPDELETE },
#endif
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

uint32_t VNCServer::rfbKeyToLinux(uint32_t key) {
	auto ret = RFB2LINUX.find(key);
	if (ret == RFB2LINUX.end()) {
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
