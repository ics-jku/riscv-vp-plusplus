#include <QFileDialog>
#include "get_dir.h"

GetDir::GetDir(QString name, bool dir) : QAction(name), dir(dir) {
	connect(this, &QAction::triggered, this, &GetDir::getDirName);
}

void GetDir::getDirName() {
	QString path;
	if(dir) {
		path = QFileDialog::getExistingDirectory(parentWidget(), "Open Directory", "/home",
				QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	} else {
		path = QFileDialog::getSaveFileName(parentWidget(), "Select JSON file", QDir::currentPath(), "JSON files (*.json)");
	}
	emit(triggered(path));
}
