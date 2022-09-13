#include "json_dir.h"

JsonDir::JsonDir(int index) : QAction("Remove Directory"), index(index) {
	connect(this, &QAction::triggered, this, &JsonDir::removeJsonDir);
}

void JsonDir::removeJsonDir() {
	emit(triggered(index));
}

