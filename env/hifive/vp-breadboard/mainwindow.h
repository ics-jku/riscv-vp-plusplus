#pragma once

#include "devices/c/all_devices.hpp"
#include "devices/luaEngine.hpp"
#include <QtWidgets/QMainWindow>
#include <cassert>
#include <map>
#include <gpio/gpio-client.hpp>

namespace Ui {
class VPBreadboard;
}

static constexpr unsigned max_num_buttons = 7;

class VPBreadboard : public QWidget {
	Q_OBJECT
	GpioClient gpio;

	struct IOF_Request {
		gpio::PinNumber pin;
	};

	IOF_Request oled_spi_channel;	// TODO: Make this a map for all IOF-Devices?
	IOF_Request oled_dc_channel;
	bool oled_spi_noresponse_mode;

	// TODO get Device factory
	Sevensegment* sevensegment;
	RGBLed* rgbLed;
	OLED_mmap* oled_mmap;
	OLED_iof* oled_iof;
	Button* buttons[max_num_buttons];
	//

	LuaEngine lua_factory;

	const char* host;
	const char* port;

	bool debugmode = false;
	unsigned moving_button = 0;
	bool connected = false;

	static uint64_t translateGpioToExtPin(gpio::State reg);
	static gpio::PinNumber translatePinToGpioOffs(gpio::PinNumber pin);

	// TODO: Phase these out and decide based on config
	static uint8_t translatePinNumberToSevensegment(uint64_t pinmap);
	static uint8_t translatePinNumberToRGBLed(uint64_t pinmap);

	bool loadConfigFile(std::string file);

public:
	VPBreadboard(std::string configfile,
			const char* host, const char* port,
			std::string additional_device_dir,
			QWidget* mparent = 0);
	~VPBreadboard();
	void showConnectionErrorOverlay(QPainter& p);
	void paintEvent(QPaintEvent*) override;
	void keyPressEvent(QKeyEvent* e) override;
	void keyReleaseEvent(QKeyEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

	void notifyChange(bool success);
};
