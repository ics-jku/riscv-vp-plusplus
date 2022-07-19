#include "raster.h"

bool graphicContainsPoint(DeviceGraphic graphic, QPoint point) {
	QPoint max = QPoint(graphic.offset.x() + (graphic.image.width() * graphic.scale),
			graphic.offset.y() + (graphic.image.height() * graphic.scale));
	return point.x() >= graphic.offset.x() && point.x() <= max.x()
			&& point.y() >= graphic.offset.y() && point.y() <= max.y();
}
