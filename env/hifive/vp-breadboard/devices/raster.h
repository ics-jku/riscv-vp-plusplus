#pragma once

#include "configurations.h"

#include <QPoint>

DeviceRow device_getRow(QPoint pos);
DeviceIndex device_getIndex(QPoint pos);
std::pair<DeviceRow, DeviceIndex> device_getRasterPosition(QPoint pos);

QPoint device_getAbsolutePosition(DeviceRow, DeviceIndex);
