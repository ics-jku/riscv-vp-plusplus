#include "json_entry.h"

JsonEntry::JsonEntry(QString file, QString title) : QAction(title), file(file) {
	connect(this, &QAction::triggered, this, &JsonEntry::JsonEntrySelected);
}

void JsonEntry::JsonEntrySelected() {
	emit(triggered(file));
}
