#pragma once
#include <QtCore>
#include <QtGui>

struct RGBLed {
	QPoint offs;
	uint8_t linewidth;
	uint8_t map;
	void draw(QPainter& p);
	RGBLed(QPoint offs, uint8_t linewidth) : offs(offs), linewidth(linewidth), map(0){};
};
