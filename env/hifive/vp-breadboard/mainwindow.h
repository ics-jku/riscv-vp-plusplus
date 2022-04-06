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
	OLED_mmap* oled_mmap;
	OLED_iof* oled_iof;
	gpio::PinNumber oled_cs_pin;	// FIXME: Ugly
	Button* buttons[max_num_buttons];
	const char* host;
	const char* port;

	bool debugmode = false;
	unsigned moving_button = 0;
	bool inited = false;

	static uint64_t translateGpioToExtPin(gpio::State reg);
	static gpio::PinNumber translatePinToGpioOffs(gpio::PinNumber pin);

	// TODO: Phase these out and decide based on config
	static uint8_t translatePinNumberToSevensegment(uint64_t pinmap);
	static uint8_t translatePinNumberToRGBLed(uint64_t pinmap);

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
