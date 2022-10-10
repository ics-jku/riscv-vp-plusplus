#include "raster.h"

#include "configurations.h"

#include <iostream>

QRect getGraphicBounds(QImage buffer, unsigned scale) {
	return QRect(buffer.offset().x(), buffer.offset().y(), buffer.width()*scale, buffer.height()*scale);
}

QRect bb_getRasterBounds() {
	return QRect(BB_ROW_X, BB_ROW_Y, BB_ONE_ROW*BB_ICON_SIZE, ((BB_INDEXES*2)+1)*BB_ICON_SIZE);
}

bool bb_isOnRaster(QPoint pos) {
	return QRect(BB_ROW_X, BB_ROW_Y, BB_ONE_ROW*BB_ICON_SIZE, BB_INDEXES*BB_ICON_SIZE).contains(pos) ||
			QRect(BB_ROW_X, BB_ROW_Y + (BB_INDEXES+1)*BB_ICON_SIZE, BB_ONE_ROW*BB_ICON_SIZE, BB_INDEXES*BB_ICON_SIZE).contains(pos);
}

Row bb_getRow(QPoint pos) {
	if(!bb_isOnRaster(pos)) {
		std::cerr << "[Breadboard Raster] Could not calculate row number: position is not on raster." << std::endl;
		return BB_ROWS;
	}
	QPoint rel_pos = pos - QPoint(BB_ROW_X, BB_ROW_Y);
	return (rel_pos.x() / BB_ICON_SIZE) + (rel_pos.y() >= BB_INDEXES*BB_ICON_SIZE ? BB_ONE_ROW : 0);
}

Index bb_getIndex(QPoint pos) {
	if(!bb_isOnRaster(pos)) {
		std::cerr << "[Breadboard Raster] Could not calculate index number: position is not on raster." << std::endl;
		return BB_INDEXES;
	}
	Index index = (pos - QPoint(BB_ROW_X, BB_ROW_Y)).y()/BB_ICON_SIZE;
	return index<BB_INDEXES?index:index-BB_INDEXES-1;
}

std::pair<Row, Index> bb_getRasterPosition(QPoint pos) {
	return {bb_getRow(pos), bb_getIndex(pos)};
}

QPoint bb_getAbsolutePosition(Row row, Index index) {
	return QPoint(BB_ROW_X+BB_ICON_SIZE*(row%BB_ONE_ROW),
			BB_ROW_Y+BB_ICON_SIZE*index+(BB_ICON_SIZE*6)*(row<BB_ONE_ROW?0:1));
}
