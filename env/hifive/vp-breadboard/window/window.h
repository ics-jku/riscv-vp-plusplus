#pragma once

#include <QMainWindow>

#include "central.h"

class MainWindow : public QMainWindow {
	Q_OBJECT

	Central *central;

	void resizeEvent(QResizeEvent* e);

public:
	MainWindow(QString configfile, std::string additional_device_dir, const std::string host, const std::string port, bool overwrite_integrated_devices=false, QWidget *parent=0);
	~MainWindow();
};
