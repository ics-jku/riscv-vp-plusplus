#include "sevensegment.hpp"

void Sevensegment::draw(QPainter& p) {
	p.save();
	QPen segment(QColor("#f72727"), linewidth, Qt::PenStyle::SolidLine, Qt::PenCapStyle::RoundCap, Qt::RoundJoin);
	p.setPen(segment);

	//  0
	// 5   1
	//  6
	// 4   2
	//  3   7
	// printf(" %c\n", map & 1 ? '_' : ' ');
	// printf("%c %c\n", map & (1 << 5) ? '|' : ' ', map & (1 << 1) ? '|' : '
	// '); printf(" %c\n", map & (1 << 6) ? '-' : ' '); printf("%c %c\n", map &
	// (1 << 5) ? '|' : ' ', map & (1 << 1) ? '|' : ' '); printf(" %c  %c\n",
	// map & (1 << 3) ? '-' : ' ', map & (1 << 7) ? '.' : ' ');

	int xcol1 = 0;
	int xcol2 = 3 * (extent.x() / 4);
	int yrow1 = 0;
	int xrow1 = extent.x() / 4 - 2;
	int yrow2 = extent.y() / 2;
	int xrow2 = extent.x() / 8 - 1;
	int yrow3 = extent.y();
	int xrow3 = 0;

	if (map & 0b00000001)  // 0
		p.drawLine(offs + QPoint(xcol1 + xrow1, yrow1), offs + QPoint(xcol2 + xrow1, yrow1));
	if (map & 0b00000010)  // 1
		p.drawLine(offs + QPoint(xcol2 + xrow1, yrow1), offs + QPoint(xcol2 + xrow2, yrow2));
	if (map & 0b00000100)  // 2
		p.drawLine(offs + QPoint(xcol2 + xrow2, yrow2), offs + QPoint(xcol2 + xrow3, yrow3));
	if (map & 0b00001000)  // 3
		p.drawLine(offs + QPoint(xcol2 + xrow3, yrow3), offs + QPoint(xcol1 + xrow3, yrow3));
	if (map & 0b00010000)  // 4
		p.drawLine(offs + QPoint(xcol1 + xrow3, yrow3), offs + QPoint(xcol1 + xrow2, yrow2));
	if (map & 0b00100000)  // 5
		p.drawLine(offs + QPoint(xcol1 + xrow2, yrow2), offs + QPoint(xcol1 + xrow1, yrow1));
	if (map & 0b01000000)  // 6
		p.drawLine(offs + QPoint(xcol1 + xrow2, yrow2), offs + QPoint(xcol2 + xrow2, yrow2));
	if (map & 0b10000000)  // 7
		p.drawPoint(offs + extent);

	p.restore();
}
