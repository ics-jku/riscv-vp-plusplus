#pragma once
#include <oled/common.hpp>
#include <QtCore>
#include <QtGui>

struct OLED_mmap
{
	ss1106::State* state;
	QPoint offs;
	QPoint margin;
	QImage image;
	float scale;
	void draw(QPainter& p);
	OLED_mmap(QPoint offs, unsigned margin, float scale = 1) : offs(offs),
			margin(QPoint(margin, margin)), scale(scale),
			image(ss1106::width - 2*ss1106::padding_lr, ss1106::height, QImage::Format_Grayscale8)
	{
		state = ss1106::getSharedState();
		state->changed = 1;
	};
};
