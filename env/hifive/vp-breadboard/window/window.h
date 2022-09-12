#pragma once

#include <QMainWindow>

#include "central.h"
#include "actions/json.h"

class MainWindow : public QMainWindow {
	Q_OBJECT

	Central *central;

	QMenu *config;
	QAction *load_config_dir;
	QMenu *devices;
	QAction *load_lua_dir;

	void createDropdown();
	void createJsonActions(QString dir);
	void createActions();

	void resizeEvent(QResizeEvent* e);

public:
	MainWindow(QString configfile, std::string additional_device_dir, const std::string host, const std::string port, bool overwrite_integrated_devices=false, QWidget *parent=0);
	~MainWindow();
};
