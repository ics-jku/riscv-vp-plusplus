#pragma once

#include "configurations.h"

bool isInsideWindow(DeviceGraphic graphic, QPoint newPos, QSize windowSize);

QRect getGraphicBounds(DeviceGraphic graphic);
bool isInsideGraphic(DeviceGraphic graphic, QPoint point);
bool isInsideGraphic(DeviceGraphic graphic, DeviceGraphic newGraphic, QPoint newPos);

bool bb_isOnRaster(QPoint pos); // has to be on actual raster position, not over the middle

bool bb_isWithinRaster(QPoint pos); // within general raster bounds (including middle)
bool bb_isWithinRaster(DeviceGraphic graphic, QPoint newPos); // within general raster bounds

RowID bb_getRow(QPoint pos);
IndexID bb_getIndex(QPoint pos);
QPoint bb_getAbsolutePosition(RowID row, IndexID index=0);

std::pair<RowID, IndexID> bb_getRasterPosition(QPoint pos);

