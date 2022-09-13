#pragma once

#include <QtWidgets>

#include "embedded/embedded.h"
#include "breadboard/breadboard.h"

class Central : public QWidget {
	Q_OBJECT

	Breadboard *breadboard;
	Embedded *embedded;

public:
	Central(const std::string host, const std::string port, QWidget *parent);
	~Central();
	void destroyConnection();

public slots:
	void loadJSON(QString file);
	void loadLUA(std::string dir, bool overwrite_integrated_devices);
	void toggleDebug();

private slots:
	void timerUpdate();
};
