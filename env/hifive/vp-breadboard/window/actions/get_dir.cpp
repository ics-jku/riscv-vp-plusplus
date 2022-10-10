#include <QFileDialog>
#include "get_dir.h"

GetDir::GetDir(QString name) : QAction(name) {
	connect(this, &QAction::triggered, this, &GetDir::getDirName);
}

void GetDir::getDirName() {
	QString path;
	if(text().startsWith("Add") || text().startsWith("Overwrite")) {
		path = QFileDialog::getExistingDirectory(parentWidget(), "Open Directory", QDir::currentPath(),
				QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	} else if(text().startsWith("Save")){
		path = QFileDialog::getSaveFileName(parentWidget(), "Select JSON file", QDir::currentPath(), "JSON files (*.json)");
	} else {
		path = QFileDialog::getOpenFileName(parentWidget(), "Select JSON file", QDir::currentPath(), "JSON files (*.json)");
	}
	emit(triggered(path));
}
