#include "window.h"

#include <QApplication>
#include <QDirIterator>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QLayout>

MainWindow::MainWindow(QString configfile, std::string additional_device_dir,
		const std::string host, const std::string port,
		bool overwrite_integrated_devices, QWidget *parent) : QMainWindow(parent) {
	setWindowTitle("MainWindow");

	central = new Central(host, port, this);
	central->loadLUA(additional_device_dir, overwrite_integrated_devices);
	central->loadJSON(configfile);
	setCentralWidget(central);
	connect(central, &Central::connectionUpdate, [this](bool active){
		connection_label->setText(active ? "Connected" : "Disconnected");
	});
	connect(central, &Central::sendStatus, this->statusBar(), &QStatusBar::showMessage);

	createDropdown();

	layout()->setSizeConstraint(QLayout::SetFixedSize);
}

MainWindow::~MainWindow() {
}

void MainWindow::saveJSON(QString file) {
	statusBar()->showMessage("Saving breadboard status to config file " + file, 10000);
	central->saveJSON(file);
	QDir dir(file);
	dir.cdUp();
	loadJsonDirEntries(dir.absolutePath());
}

void MainWindow::removeJsonDir(QString dir) {
	auto dir_menu = std::find_if(json_dirs.begin(), json_dirs.end(),
			[dir](QMenu* menu){return menu->title() == dir;});
	if(dir_menu == json_dirs.end()) return;
	statusBar()->showMessage("Remove JSON directory", 10000);
	QAction* json_dir_action = (*dir_menu)->menuAction();
	config->removeAction(json_dir_action);
	json_dirs.erase(dir_menu);
}

void MainWindow::loadJsonDirEntries(QString dir) {
	QString title = dir.startsWith(":") ? "Integrated" : dir;
	auto dir_menu = std::find_if(json_dirs.begin(), json_dirs.end(),
			[title](QMenu* menu){ return menu->title() == title; });
	if(dir_menu == json_dirs.end()) return;
	QDirIterator it(dir, {"*.json"}, QDir::Files);
	if(!it.hasNext()) {
		removeJsonDir(dir);
		return;
	}
	for(QAction* action : (*dir_menu)->actions()) {
		if(action->text().endsWith(".json")) {
			(*dir_menu)->removeAction(action);
		}
	}
	while(it.hasNext()) {
		QString file = it.next();
		QAction *cur = new QAction(file.right(file.size()-(dir.size() + 1)));
		connect(cur, &QAction::triggered, [this, file](){
			central->loadJSON(file);
		});
		(*dir_menu)->addAction(cur);
	}
}

void MainWindow::addJsonDir(QString dir) {
	if(std::find_if(json_dirs.begin(), json_dirs.end(),
			[dir](QMenu* menu){return menu->title() == dir;}) != json_dirs.end()) return;
	statusBar()->showMessage("Add JSON directory " + dir, 10000);
	QMenu* dir_menu = new QMenu(dir.startsWith(":") ? "Integrated" : dir, config);
	json_dirs.push_back(dir_menu);
	config->addMenu(dir_menu);
	if(!dir.startsWith(":")) {
		QAction *remove_dir = new QAction("Remove Directory");
		connect(remove_dir, &QAction::triggered, [this, dir](){
			removeJsonDir(dir);
		});
		dir_menu->addAction(remove_dir);
		QAction *reload_dir = new QAction("Reload Directory");
		connect(reload_dir, &QAction::triggered, [this, dir](){
			loadJsonDirEntries(dir);
		});
		dir_menu->addAction(reload_dir);
		dir_menu->addSeparator();
	}
	loadJsonDirEntries(dir);
}

void MainWindow::createDropdown() {
	config = menuBar()->addMenu("Config");
	QAction* load_config_file = new QAction("Load JSON file");
	load_config_file->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
	connect(load_config_file, &QAction::triggered, [this](){
		QString path = QFileDialog::getOpenFileName(parentWidget(), "Select JSON file",
				QDir::currentPath(), "JSON files (*.json)");
		central->loadJSON(path);
	});
	config->addAction(load_config_file);
	QAction* save_config = new QAction("Save to JSON file");
	save_config->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	connect(save_config, &QAction::triggered, [this](){
		QString path = QFileDialog::getSaveFileName(parentWidget(), "Select JSON file",
				QDir::currentPath(), "JSON files (*.json)");
		saveJSON(path);
	});
	config->addAction(save_config);
	QAction* clear_breadboard = new QAction("Clear breadboard");
	clear_breadboard->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
	connect(clear_breadboard, &QAction::triggered, central, &Central::clearBreadboard);
	config->addAction(clear_breadboard);
	QAction* load_config_dir = new QAction("Add JSON directory");
	load_config_dir->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_A));
	connect(load_config_dir, &QAction::triggered, [this](){
		QString path = QFileDialog::getExistingDirectory(parentWidget(), "Open Directory",
				QDir::currentPath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		addJsonDir(path);
	});
	config->addAction(load_config_dir);
	config->addSeparator();
	addJsonDir(":/conf");

	lua = menuBar()->addMenu("LUA");
	QAction* add_lua_dir = new QAction("Add directory");
	connect(add_lua_dir, &QAction::triggered, [this](){
		QString path = QFileDialog::getExistingDirectory(parentWidget(), "Open Directory",
						QDir::currentPath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		central->loadLUA(path.toStdString(), false);
	});
	lua->addAction(add_lua_dir);
	QAction* overwrite_luas = new QAction("Add directory (Overwrite integrated)");
	connect(overwrite_luas, &QAction::triggered, [this](){
		QString path = QFileDialog::getExistingDirectory(parentWidget(), "Open Directory",
						QDir::currentPath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		central->loadLUA(path.toStdString(), true);
	});
	lua->addAction(overwrite_luas);

	QMenu* window = menuBar()->addMenu("Window");
	QAction* debug = new QAction("Debug Mode");
	debug->setShortcut(QKeySequence(Qt::Key_Space));
	connect(debug, &QAction::triggered, [this](){
		debug_label->setText(central->toggleDebug() ? "Debug" : "");
	});
	window->addAction(debug);
	window->addSeparator();
	QAction* quit = new QAction("Quit");
	quit->setShortcut(QKeySequence(Qt::Key_Q));
	connect(quit, &QAction::triggered, [this](){
		central->destroyConnection();
		QApplication::quit();
	});
	window->addAction(quit);

	debug_label = new QLabel();
	statusBar()->addPermanentWidget(debug_label);
	connection_label = new QLabel("Disconnected");
	statusBar()->addPermanentWidget(connection_label);
}
