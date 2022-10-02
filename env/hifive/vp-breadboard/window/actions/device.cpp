#include "device.h"

DeviceEntry::DeviceEntry(DeviceClass classname) : QAction(QString::fromStdString(classname)), classname(classname) {
	connect(this, &QAction::triggered, this, &DeviceEntry::DeviceEntrySelected);
}

void DeviceEntry::DeviceEntrySelected() {
	emit(triggered(classname));
}

