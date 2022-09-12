#include "window.h"

#include "ui_mainwindow.h"

MainWindow::MainWindow(QString configfile, std::string additional_device_dir,
		const std::string host, const std::string port, bool overwrite_integrated_devices, QWidget *parent) : QMainWindow(parent) {
	setWindowTitle("MainWindow");

	central = new Central(host, port, this);
	central->loadLUA(additional_device_dir, overwrite_integrated_devices);
	central->loadJSON(configfile);
	setCentralWidget(central);

	createDropdown();
}

void MainWindow::resizeEvent(QResizeEvent *e) {
	setFixedSize(central->width(), central->height());
}

MainWindow::~MainWindow() {
	delete central;
	delete load_config_dir;
	delete load_lua_dir;
	delete config;
	delete devices;
}

void MainWindow::createJsonActions(QString dir) {
	QDirIterator it(dir);
	if(!it.hasNext()) {
		return;
	}
	QMenu *dir_menu = new QMenu(dir);
	while (it.hasNext()) {
		JsonAction *cur = new JsonAction(it.next(), dir.size()+1);
		connect(cur, &JsonAction::triggered, central, &Central::loadJSON);
		dir_menu->addAction(cur);
	}
	dir_menu->addSeparator();
	QAction *remove_dir = new QAction(tr("Remove directory"));
	// TODO connect
	dir_menu->addAction(remove_dir);
	config->addMenu(dir_menu);
}

void MainWindow::createActions() {
	load_config_dir = new QAction(tr("Load new &JSON directory"));
	load_lua_dir = new QAction(tr("Load new &LUA directory"));
	// TODO open window for selecting dir
}

void MainWindow::createDropdown() {
	createActions();

	config = menuBar()->addMenu(tr("&Config"));
	config->addAction(load_config_dir);
	config->addSeparator();
	createJsonActions(":/conf");

	devices = menuBar()->addMenu(tr("&Devices"));
	devices->addAction(load_lua_dir);
}
