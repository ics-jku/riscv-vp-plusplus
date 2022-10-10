#pragma once

#include "types.h"

#include <QPoint>

QRect getGraphicBounds(QImage buffer, unsigned scale);
QRect bb_getRasterBounds();

bool bb_isOnRaster(QPoint pos); // has to be on actual raster position, not over the middle

bool bb_isWithinRaster(QPoint pos); // within general raster bounds (including middle)

Row bb_getRow(QPoint pos);
Index bb_getIndex(QPoint pos);
QPoint bb_getAbsolutePosition(Row row, Index index=0);

std::pair<Row, Index> bb_getRasterPosition(QPoint pos);

