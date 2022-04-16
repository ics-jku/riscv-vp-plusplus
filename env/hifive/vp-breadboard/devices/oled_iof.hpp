#pragma once
#include <oled/oled.hpp>
#include <QtCore>
#include <QtGui>

struct OLED_iof : public SS1106
{
	ss1106::State state;
	QPoint offs;
	QPoint margin;
	QImage image;
	float scale;
	bool data_command_pin;
	void draw(QPainter& p);
	OLED_iof(QPoint offs, unsigned margin, float scale = 1) :
		SS1106([this](){return data_command_pin;}, &state),
		offs(offs), margin(QPoint(margin, margin)), scale(scale), data_command_pin(false),
		image(ss1106::width - 2*ss1106::padding_lr, ss1106::height, QImage::Format_Grayscale8)
	{};
};
