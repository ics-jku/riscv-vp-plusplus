#pragma once

#include <QAction>
#include <QString>

class Load : public QAction {
	Q_OBJECT

public:
	Load(QString name);

public slots:
	void getDirName();

signals:
	void triggered(QString dir);
};
