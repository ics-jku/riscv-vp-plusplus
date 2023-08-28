#pragma once
#include <QMainWindow>
#include <cassert>

#include "vpdisplayserver.h"

namespace Ui {
class VPDisplay;
}

class VPDisplay : public QWidget {
	Q_OBJECT
	Framebuffer* framebuffer;
	QImage* frame;
	VPDisplayserver server;

   public:
	VPDisplay(QWidget* mparent = 0);
	~VPDisplay();
	void paintEvent(QPaintEvent*);
	// void keyPressEvent(QKeyEvent *e);
	void drawMainPage(QImage* mem);

	void notifyChange(bool success);
};
