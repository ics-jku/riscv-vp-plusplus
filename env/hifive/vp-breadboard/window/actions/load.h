#pragma once

#include <QAction>
#include <QString>

class Load : public QAction {
	Q_OBJECT

public:
	Load(QString name);

private slots:
	void getDirName();

signals:
	void triggered(QString dir);
};
