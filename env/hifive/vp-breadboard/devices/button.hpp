#pragma once
#include <QtCore>
#include <QtGui>

struct Button
{
	QRect area;
	uint8_t pin;
	QKeySequence keybinding;
	QString name;
	bool pressed;
Button(QRect area, uint8_t pin, QKeySequence keybinding, QString name = "") :
	area(area), pin(pin), keybinding(keybinding), name(name),
		pressed(false){};
};
