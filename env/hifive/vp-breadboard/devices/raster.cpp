#include "raster.h"

#include "breadboard/configurations.h"

DeviceRow device_getRow(QPoint pos) {
	return pos.x() / BB_ICON_SIZE;
}

DeviceIndex device_getIndex(QPoint pos) {
	return pos.y() / BB_ICON_SIZE;
}

std::pair<DeviceRow, DeviceIndex> device_getRasterPosition(QPoint pos) {
	return {device_getRow(pos), device_getIndex(pos)};
}

QPoint device_getAbsolutePosition(DeviceRow row, DeviceIndex index) {
	return QPoint(row*BB_ICON_SIZE,index*BB_ICON_SIZE);
}
