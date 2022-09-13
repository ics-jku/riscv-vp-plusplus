#pragma once

#include <QAction>
#include <QString>

class JsonDir : public QAction {
	Q_OBJECT

	int index;

public:
	JsonDir(int index);

public slots:
	void removeJsonDir();

signals:
	void triggered(int index);
};
