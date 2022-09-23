#include "raster.h"

bool isInsideWindow(DeviceGraphic graphic, QPoint newPos, QSize windowSize) {
	graphic.offset = newPos;
	QRect bounds = getGraphicBounds(graphic);
	QRect window = QRect(QPoint(0,0), windowSize);
	return window.contains(bounds);
}

QRect getGraphicBounds(DeviceGraphic graphic) {
	return QRect(graphic.offset.x(), graphic.offset.y(), graphic.image.width() * graphic.scale, graphic.image.height() * graphic.scale);
}

bool isInsideGraphic(DeviceGraphic graphic, QPoint point) {
	return getGraphicBounds(graphic).contains(point);
}

bool isInsideGraphic(DeviceGraphic graphic, DeviceGraphic newGraphic, QPoint newPos) {
	newGraphic.offset = newPos;
	return getGraphicBounds(graphic).intersects(getGraphicBounds(newGraphic));
}

bool bb_isOnRaster(QPoint pos) {
	return QRect(BB_ROW_X, BB_ROW_Y, BB_ONE_ROW*BB_ICON_WIDTH, BB_INDEXES*BB_ICON_WIDTH).contains(pos) ||
			QRect(BB_ROW_X, BB_ROW_Y + (BB_INDEXES+1)*BB_ICON_WIDTH, BB_ONE_ROW*BB_ICON_WIDTH, BB_INDEXES*BB_ICON_WIDTH).contains(pos);
}

bool bb_isWithinRaster(QPoint pos) {
	return QRect(BB_ROW_X, BB_ROW_Y, BB_ONE_ROW*BB_ICON_WIDTH, ((BB_INDEXES*2)+1)*BB_ICON_WIDTH).contains(pos);
}

bool bb_isWithinRaster(DeviceGraphic graphic, QPoint newPos) {
	graphic.offset = newPos;
	QRect bounds = getGraphicBounds(graphic);
	return bb_isWithinRaster(bounds.topLeft()) && bb_isWithinRaster(bounds.bottomRight());
}

RowID bb_getRow(QPoint pos) {
	if(!bb_isOnRaster(pos)) {
		return BB_ROWS;
	}
	QPoint rel_pos = pos - QPoint(BB_ROW_X, BB_ROW_Y);
	return (rel_pos.x() / BB_ICON_WIDTH) + (rel_pos.y() >= BB_INDEXES*BB_ICON_WIDTH ? BB_ONE_ROW : 0);
}

IndexID bb_getIndex(QPoint pos) {
	if(!bb_isOnRaster(pos)) {
		return BB_INDEXES;
	}
	IndexID index = (pos - QPoint(BB_ROW_X, BB_ROW_Y)).y()/BB_ICON_WIDTH;
	return index<BB_INDEXES?index:index-BB_INDEXES-1;
}

QPoint bb_getAbsolutePosition(RowID row, IndexID index) {
	return QPoint(BB_ROW_X+BB_ICON_WIDTH*(row%BB_ONE_ROW),
			BB_ROW_Y+BB_ICON_WIDTH*index+(BB_ICON_WIDTH*6)*(row<BB_ONE_ROW?0:1));
}

std::pair<RowID, IndexID> bb_getRasterPosition(QPoint pos) {
	return {bb_getRow(pos), bb_getIndex(pos)};
}
