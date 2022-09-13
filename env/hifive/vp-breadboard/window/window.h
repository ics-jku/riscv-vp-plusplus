#pragma once

#include <QMainWindow>

#include "central.h"

class MainWindow : public QMainWindow {
	Q_OBJECT

	Central *central;

	QMenu *config;
	std::vector<QMenu*> json_dirs;
//	QMenu *devices;

	void createDropdown();
	void addJsonDir(QString dir);
	void removeJsonDir(int index);

	void resizeEvent(QResizeEvent* e);

public:
	MainWindow(QString configfile, std::string additional_device_dir, const std::string host, const std::string port, bool overwrite_integrated_devices=false, QWidget *parent=0);
	~MainWindow();
};
