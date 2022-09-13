#include "window.h"

#include "actions/json_dir.h"
#include "actions/json_file.h"
#include "actions/load.h"

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
	delete config;
//	delete devices;
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
	config = menuBar()->addMenu(tr("&Config"));
	Load* load_config_dir = new Load("&JSON");
	connect(load_config_dir, &Load::triggered, this, &MainWindow::addJsonDir);
	config->addAction(load_config_dir);
	config->addSeparator();
	addJsonDir(":/conf");

//	devices = menuBar()->addMenu(tr("&Devices"));
//	Load *load_lua_dir = new Load("&LUA");
//	connect(load_lua_dir, &Load::triggered, central, &Central::loadLUA);
//	devices->addAction(load_lua_dir);
}
