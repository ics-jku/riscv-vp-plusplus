#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
	QApplication a(argc, argv);
	VPDisplay w;
	w.show();

	return a.exec();
}
