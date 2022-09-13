#include "window.h"

#include "actions/json_dir.h"
#include "actions/json_file.h"
#include "actions/load.h"

#include <QApplication>
#include <QDirIterator>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(QString configfile, std::string additional_device_dir,
		const std::string host, const std::string port, bool overwrite_integrated_devices, QWidget *parent) : QMainWindow(parent) {
	setWindowTitle("MainWindow");

	central = new Central(host, port, this);
	central->loadLUA(additional_device_dir, overwrite_integrated_devices);
	central->loadJSON(configfile);
	setCentralWidget(central);
	connect(central, &Central::connectionUpdate, this, &MainWindow::connectionUpdate);

	createDropdown();
}

MainWindow::~MainWindow() {
}

void MainWindow::quit() {
	central->destroyConnection();
	QApplication::quit();
}

void MainWindow::resizeEvent(QResizeEvent *e) {
	setFixedSize(sizeHint());
}

void MainWindow::toggleDebug() {
	debug_label->setText(central->toggleDebug() ? "" : "Debug");
}

void MainWindow::connectionUpdate(bool active) {
	connection_label->setText(active ? "Connected" : "Disconnected");
}

void MainWindow::addJsonDir(QString dir) {
	QDirIterator it(dir, {"*.json"}, QDir::Files);
	if(!it.hasNext()) {
		return;
	}
	QMenu *dir_menu = new QMenu(dir);
	while (it.hasNext()) {
		JsonFile *cur = new JsonFile(it.next(), dir.size()+1);
		connect(cur, &JsonFile::triggered, central, &Central::loadJSON);
		dir_menu->addAction(cur);
	}
	dir_menu->addSeparator();
	JsonDir *remove_dir = new JsonDir(json_dirs.size());
	connect(remove_dir, &JsonDir::triggered, this, &MainWindow::removeJsonDir);
	dir_menu->addAction(remove_dir);
	json_dirs.push_back(dir_menu);
	config->addMenu(dir_menu);
}

void MainWindow::removeJsonDir(int index) {
	if(index >= json_dirs.size()) {
		return;
	}
	QMenu* json_dir = json_dirs.at(index);
	QAction* json_dir_action = json_dir->menuAction();
	config->removeAction(json_dir_action);
	json_dirs.erase(json_dirs.begin()+index);
}

void MainWindow::createDropdown() {
	config = menuBar()->addMenu("Config");
	Load* load_config_dir = new Load("JSON");
	load_config_dir->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_J));
	connect(load_config_dir, &Load::triggered, this, &MainWindow::addJsonDir);
	config->addAction(load_config_dir);
	config->addSeparator();
	addJsonDir(":/conf");

	QMenu* window = menuBar()->addMenu("Window");
	QAction* debug = new QAction("Debug Mode");
	debug->setShortcut(QKeySequence(Qt::Key_Space));
	connect(debug, &QAction::triggered, this, &MainWindow::toggleDebug);
	window->addAction(debug);
	window->addSeparator();
	QAction* quit = new QAction("Quit");
	quit->setShortcut(QKeySequence(Qt::Key_Q));
	connect(quit, &QAction::triggered, this, &MainWindow::quit);
	window->addAction(quit);

	debug_label = new QLabel();
	statusBar()->addPermanentWidget(debug_label);
	connection_label = new QLabel("Disconnected");
	statusBar()->addPermanentWidget(connection_label);

//	devices = menuBar()->addMenu("&Devices");
//	Load *load_lua_dir = new Load("&LUA");
//	connect(load_lua_dir, &Load::triggered, central, &Central::loadLUA);
//	devices->addAction(load_lua_dir);
}
