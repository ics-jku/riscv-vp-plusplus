#ifndef RISCV_VP_VNCSERVER_H
#define RISCV_VP_VNCSERVER_H

#include <rfb/rfb.h>
#include <stdint.h>

class VNCInput_if {
   public:
	/* callbacks */
	virtual void doPtr(int buttonMask, int x, int y) = 0;
	virtual void doKbd(rfbBool down, rfbKeySym key) = 0;
};

class VNCServer {
   public:
	VNCServer(void);
	~VNCServer(void);

	bool start(void);
	void stop(void);

	void setScreenProperties(int width, int height, int bitsPerSample, int samplesPerPixel, int bytesPerPixel) {
		this->width = width;
		this->height = height;
		this->bitsPerSample = bitsPerSample;
		this->samplesPerPixel = samplesPerPixel;
		this->bytesPerPixel = bytesPerPixel;
	}

	inline int getWidth(void) {
		return width;
	}

	inline int getHeight(void) {
		return height;
	}

	inline void setVNCInput(VNCInput_if *vncInput) {
		this->vncInput = vncInput;
	}

	inline rfbScreenInfoPtr getScreen(void) {
		return rfbScreen;
	}

	/* only allowed between start() and stop() */
	inline uint8_t *getFrameBuffer(void) {
		return (uint8_t *)rfbScreen->frameBuffer;
	}

	inline void markRectAsModified(int x1, int y1, int x2, int y2) {
		rfbMarkRectAsModified(rfbScreen, x1, y1, x2, y2);
	}

	inline bool isActive(void) {
		return rfbIsActive(rfbScreen);
	}

	/* callbacks */
	enum rfbNewClientAction newClient(rfbClientPtr cl);
	void clientGone(rfbClientPtr cl);
	void doPtr(rfbClientPtr cl, int buttonMask, int x, int y);
	void doKbd(rfbClientPtr cl, rfbBool down, rfbKeySym key);

   private:
	int width, height, bitsPerSample, samplesPerPixel, bytesPerPixel;
	rfbScreenInfoPtr rfbScreen;
	VNCInput_if *vncInput;
};

#endif /* RISCV_VP_VNCSERVER_H */
