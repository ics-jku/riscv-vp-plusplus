#include "json.h"

JsonAction::JsonAction(QString file, int dir_length) : QAction(file.right(file.size()-dir_length)), file(file) {
	connect(this, &QAction::triggered, this, &JsonAction::JsonActionSlot);
}

void JsonAction::JsonActionSlot() {
	emit(triggered(file));
}

