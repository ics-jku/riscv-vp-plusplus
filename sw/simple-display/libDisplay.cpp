/*
 * libDisplay.cpp
 *
 *  Created on: Oct 5, 2018
 *      Author: dwd
 */

#include "libDisplay.hpp"

#include <math.h>

#include <algorithm>

typedef Framebuffer::Point Point;
typedef Framebuffer::PointF PointF;
typedef Framebuffer::Color Color;

namespace display {

void setPixel(Framebuffer::Type frame, Point pixel, Color color) {
	framebuffer->getFrame(frame).raw[pixel.y][pixel.x] = color;
}

void drawLine(Framebuffer::Type frame, PointF from, PointF to, Color color) {
	framebuffer->parameter.line.frame = frame;
	framebuffer->parameter.line.from = from;
	framebuffer->parameter.line.to = to;
	framebuffer->parameter.line.color = color;
	framebuffer->command = Framebuffer::Command::drawLine;
}

void drawRect(Framebuffer::Type frame, PointF ol, PointF ur, Color color) {
	if (ol.x > ur.x) {
		std::swap(ol.x, ur.x);
	}
	if (ol.y > ur.y) {
		std::swap(ol.y, ur.y);
	}
	drawLine(frame, ol, PointF(ur.x, ol.y), color);
	drawLine(frame, PointF(ur.x, ol.y), ur, color);
	drawLine(frame, ur, PointF(ol.x, ur.y), color);
	drawLine(frame, PointF(ol.x, ur.y), ol, color);
}

void fillRect(Framebuffer::Type frame, PointF ol, PointF ur, Color color) {
	if (ol.x == ur.x || ol.y == ur.y) {  // No dimension
		return;
	}
	if (ol.x > ur.x) {
		std::swap(ol.x, ur.x);
	}
	if (ol.y > ur.y) {
		std::swap(ol.y, ur.y);
	}
	if (ur.x - ol.x > ur.y - ol.y) {
		// Horizontal
		for (uint16_t y = ol.y; y <= ur.y; y++) {
			drawLine(frame, PointF(ol.x, y), PointF(ur.x, y), color);
		}
	} else {
		// Vertical
		for (uint16_t x = ol.x; x <= ur.x; x++) {
			drawLine(frame, PointF(x, ol.y), PointF(x, ur.y), color);
		}
	}
}

void applyFrame() {
	framebuffer->command = Framebuffer::Command::applyFrame;
}

void fillFrame(Framebuffer::Type frame, Color color) {
	framebuffer->parameter.fill.frame = frame;
	framebuffer->parameter.fill.color = color;
	framebuffer->command = Framebuffer::Command::fillFrame;
}

}  // namespace display
