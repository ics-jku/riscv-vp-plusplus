#pragma once

#include "devices/all_devices.hpp"
#include <QtWidgets/QMainWindow>
#include <cassert>
#include <gpio/gpio-client.hpp>

namespace Ui {
class VPBreadboard;
}

static constexpr unsigned max_num_buttons = 7;

class VPBreadboard : public QWidget {
	Q_OBJECT
	GpioClient gpio;
	Sevensegment* sevensegment;
	RGBLed* rgbLed;
	OLED* oled;
	Button* buttons[max_num_buttons];
	const char* host;
	const char* port;

	bool debugmode = false;
	unsigned moving_button = 0;
	bool inited = false;

	uint64_t translateGpioToExtPin(gpio::State reg);
	uint8_t translatePinNumberToSevensegment(uint64_t pinmap);
	uint8_t translatePinNumberToRGBLed(uint64_t pinmap);
	uint8_t translatePinToGpioOffs(uint8_t pin);

	bool loadConfigFile(const char* file);

public:
	VPBreadboard(const char* configfile, const char* host, const char* port, QWidget* mparent = 0);
	~VPBreadboard();
	void showConnectionErrorOverlay(QPainter& p);
	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

	void notifyChange(bool success);
};
