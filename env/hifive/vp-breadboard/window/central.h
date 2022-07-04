#pragma once

#include <QtWidgets>

#include "embedded/embedded.h"
#include "breadboard/breadboard.h"

class Central : public QWidget {
	Breadboard *breadboard;
	Embedded *embedded;

public:
	Central(QString configfile, std::string additional_device_dir, const char* host, const char* port, bool overwrite_integrated_devices, QWidget *parent);

private slots:
	void timerUpdate();
};
