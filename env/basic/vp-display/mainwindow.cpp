#include "mainwindow.h"

#include <qpainter.h>

#include <cassert>

#include "framebuffer.h"
#include "ui_mainwindow.h"

VPDisplay::VPDisplay(QWidget* mparent) : QWidget(mparent) {
	framebuffer = server.createSM();
	frame = new QImage(Framebuffer::screenWidth, Framebuffer::screenHeight,
	                   QImage::Format_RGB444);  // two bytes per pixel
	resize(Framebuffer::screenWidth, Framebuffer::screenHeight);
	setFixedSize(size());
	server.startListening(std::bind(&VPDisplay::notifyChange, this, std::placeholders::_1));
}

VPDisplay::~VPDisplay() {
	delete frame;
}

void VPDisplay::drawMainPage(QImage* mem) {
	Framebuffer::Frame activeFrame = framebuffer->getActiveFrame();
	Framebuffer::Frame background = framebuffer->getBackground();
	for (int row = 0; row < mem->height(); row++) {
		uint16_t* line = reinterpret_cast<uint16_t*>(mem->scanLine(row));  // Two bytes per pixel
		for (int x = 0; x < mem->width(); x++) {
			line[x] = activeFrame.raw[row][x] == 0 ? background.raw[row][x] : activeFrame.raw[row][x];
		}
	}
}

void VPDisplay::paintEvent(QPaintEvent*) {
	QPainter painter(this);

	// painter.scale(size_factor, size_factor);

	// Draw Header
	// QPainter mempaint(&memory);

	drawMainPage(frame);
	painter.drawImage(QPoint(0, 0), *frame);
	painter.end();
}

void VPDisplay::notifyChange(bool success) {
	assert(success);
	update();
}
