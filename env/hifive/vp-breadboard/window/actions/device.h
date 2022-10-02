#pragma once

#include <QAction>
#include "devices/configurations.h"

class DeviceEntry : public QAction {
	Q_OBJECT

	DeviceClass classname;

public:
	DeviceEntry(DeviceClass classname);

private slots:
	void DeviceEntrySelected();

signals:
	void triggered(DeviceClass classname);
};
