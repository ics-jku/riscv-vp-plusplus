#include "window.h"

#include "ui_mainwindow.h"

MainWindow::MainWindow(QString configfile, std::string additional_device_dir,
		const char* host, const char* port, bool overwrite_integrated_devices, QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	central = new Central(configfile, additional_device_dir, host, port, overwrite_integrated_devices, this);
	setCentralWidget(central);
}

MainWindow::~MainWindow() { }
