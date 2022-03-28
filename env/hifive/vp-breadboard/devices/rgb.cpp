#include "rgb.hpp"

void RGBLed::draw(QPainter& p) {
	if (!map) {
		return;
	}
	p.save();
	QPen led(QColor(map & 1 ? 255 : 0, map & (1 << 1) ? 255 : 0, map & (1 << 2) ? 255 : 0, 0xC0), linewidth,
	         Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::RoundJoin);
	p.setPen(led);

	p.drawPoint(offs);

	p.restore();
}
