#include "window.h"

#include <QApplication>
#include <QDirIterator>
#include <QLayout>
#include <QMenuBar>
#include <QStatusBar>

#include "actions/get_dir.h"
#include "actions/json_entry.h"

MainWindow::MainWindow(QString configfile, std::string additional_device_dir, const std::string host,
                       bool overwrite_integrated_devices, QWidget* parent)
    : QMainWindow(parent) {
	setWindowTitle("MainWindow");

	central = new Central(host, this);
	central->loadLUA(additional_device_dir, overwrite_integrated_devices);
	central->loadJSON(configfile);
	setCentralWidget(central);
	connect(central, &Central::connectionUpdate, this, &MainWindow::connectionUpdate);
	connect(central, &Central::sendStatus, this->statusBar(), &QStatusBar::showMessage);

	createDropdown();

	layout()->setSizeConstraint(QLayout::SetFixedSize);
}

MainWindow::~MainWindow() {}

void MainWindow::quit() {
	for (auto const& [_, port] : gpio::PORT_MAP) {
		if (port == gpio::Port::UNDEF) {
			continue;
		}
		central->destroyConnection(port);
	}
	QApplication::quit();
}

void MainWindow::toggleDebug() {
	debug_label->setText(central->toggleDebug() ? "Debug" : "");
}

void MainWindow::connectionUpdate(bool active) {
	connection_label->setText(active ? "Connected" : "Disconnected");
}

void MainWindow::saveJSON(QString file) {
	statusBar()->showMessage("Saving breadboard status to config file " + file, 10000);
	central->saveJSON(file);
	QDir dir(file);
	dir.cdUp();
	loadJsonDirEntries(dir.absolutePath());
}

void MainWindow::removeJsonDir(QString dir) {
	auto dir_menu =
	    std::find_if(json_dirs.begin(), json_dirs.end(), [dir](QMenu* menu) { return menu->title() == dir; });
	if (dir_menu == json_dirs.end())
		return;
	statusBar()->showMessage("Remove JSON directory", 10000);
	QAction* json_dir_action = (*dir_menu)->menuAction();
	config->removeAction(json_dir_action);
	json_dirs.erase(dir_menu);
}

void MainWindow::loadJsonDirEntries(QString dir) {
	QString title = dir.startsWith(":") ? "Integrated" : dir;
	auto dir_menu =
	    std::find_if(json_dirs.begin(), json_dirs.end(), [title](QMenu* menu) { return menu->title() == title; });
	if (dir_menu == json_dirs.end())
		return;
	QDirIterator it(dir, {"*.json"}, QDir::Files);
	if (!it.hasNext()) {
		removeJsonDir(dir);
		return;
	}
	for (QAction* action : (*dir_menu)->actions()) {
		if (action->text().endsWith(".json")) {
			(*dir_menu)->removeAction(action);
		}
	}
	while (it.hasNext()) {
		QString file = it.next();
		JsonEntry* cur = new JsonEntry(file, file.right(file.size() - (dir.size() + 1)));
		connect(cur, &JsonEntry::triggered, central, &Central::loadJSON);
		(*dir_menu)->addAction(cur);
	}
}

void MainWindow::addJsonDir(QString dir) {
	if (std::find_if(json_dirs.begin(), json_dirs.end(), [dir](QMenu* menu) { return menu->title() == dir; }) !=
	    json_dirs.end())
		return;
	statusBar()->showMessage("Add JSON directory " + dir, 10000);
	QMenu* dir_menu = new QMenu(dir.startsWith(":") ? "Integrated" : dir);
	json_dirs.push_back(dir_menu);
	config->addMenu(dir_menu);
	if (!dir.startsWith(":")) {
		JsonEntry* remove_dir = new JsonEntry(dir, "Remove Directory");
		connect(remove_dir, &JsonEntry::triggered, this, &MainWindow::removeJsonDir);
		dir_menu->addAction(remove_dir);
		JsonEntry* reload_dir = new JsonEntry(dir, "Reload Directory");
		connect(reload_dir, &JsonEntry::triggered, this, &MainWindow::loadJsonDirEntries);
		dir_menu->addAction(reload_dir);
		dir_menu->addSeparator();
	}
	loadJsonDirEntries(dir);
}

void MainWindow::createDropdown() {
	config = menuBar()->addMenu("Config");
	GetDir* load_config_file = new GetDir("Load JSON file");
	load_config_file->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
	connect(load_config_file, &GetDir::triggered, central, &Central::loadJSON);
	config->addAction(load_config_file);
	GetDir* save_config = new GetDir("Save to JSON file");
	save_config->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	connect(save_config, &GetDir::triggered, this, &MainWindow::saveJSON);
	config->addAction(save_config);
	QAction* clear_breadboard = new QAction("Clear breadboard");
	clear_breadboard->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
	connect(clear_breadboard, &QAction::triggered, central, &Central::clearBreadboard);
	config->addAction(clear_breadboard);
	GetDir* load_config_dir = new GetDir("Add JSON directory");
	load_config_dir->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
	connect(load_config_dir, &GetDir::triggered, this, &MainWindow::addJsonDir);
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
