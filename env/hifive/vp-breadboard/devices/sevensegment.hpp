#pragma once
#include <QtCore>
#include <QtGui>

struct Sevensegment {
	QPoint offs;
	QPoint extent;
	uint8_t linewidth;
	uint8_t map;
	void draw(QPainter& p);
	Sevensegment() : offs(50, 50), extent(100, 100), linewidth(10), map(0){};
	Sevensegment(QPoint offs, QPoint extent, uint8_t linewidth)
	    : offs(offs), extent(extent), linewidth(linewidth), map(0){};
};
