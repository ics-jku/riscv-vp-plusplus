#pragma once

#include <QString>
#include <QSize>

const QString DEFAULT_PATH = ":/img/virtual_breadboard.png";
const QSize DEFAULT_SIZE = QSize(486, 233);

const unsigned BB_ROWS = 80;
const unsigned BB_ONE_ROW = BB_ROWS/2;
const unsigned BB_INDEXES = 5;

const unsigned BB_ICON_SIZE = DEFAULT_SIZE.width()/BB_ONE_ROW;

const QString DEVICE_DRAG_TYPE = "device";

const unsigned BB_ROW_X = 5;
const unsigned BB_ROW_Y = 50;
