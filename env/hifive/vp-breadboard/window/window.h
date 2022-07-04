#pragma once

#include <QMainWindow>

#include "central.h"

namespace Ui {
	class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT

	Ui::MainWindow *ui;
	Central *central;

public:
	MainWindow(QString configfile, std::string additional_device_dir, const char* host, const char* port, bool overwrite_integrated_devices=false, QWidget *parent=0);
	~MainWindow();
};
