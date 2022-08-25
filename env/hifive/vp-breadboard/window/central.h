#pragma once

#include <QtWidgets>

#include "embedded/embedded.h"
#include "breadboard/breadboard.h"

class Central : public QWidget {
	Breadboard *breadboard;
	Embedded *embedded;

public:
	Central(QString configfile, std::string additional_device_dir, const std::string host, const std::string port, bool overwrite_integrated_devices, QWidget *parent);
	~Central();

private slots:
	void timerUpdate();
};
