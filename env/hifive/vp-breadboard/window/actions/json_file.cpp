#include "json_file.h"

JsonFile::JsonFile(QString file, int dir_length) : QAction(file.right(file.size()-dir_length)), file(file) {
	connect(this, &QAction::triggered, this, &JsonFile::JsonFileSelected);
}

void JsonFile::JsonFileSelected() {
	emit(triggered(file));
}

