#pragma once

#include <QAction>
#include <QString>

class GetDir : public QAction {
	Q_OBJECT

	bool dir;

public:
	GetDir(QString name, bool dir);

private slots:
	void getDirName();

signals:
	void triggered(QString path);
};
