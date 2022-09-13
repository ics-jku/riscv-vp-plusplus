#include "load.h"

#include <QFileDialog>

Load::Load(QString name) : QAction("Load new " + name + " directory") {
	connect(this, &QAction::triggered, this, &Load::getDirName);
}

void Load::getDirName() {
	QString dir = QFileDialog::getExistingDirectory(parentWidget(), tr("Open Directory"),
            "/home",
            QFileDialog::ShowDirsOnly
            | QFileDialog::DontResolveSymlinks);
	emit(triggered(dir));
}
