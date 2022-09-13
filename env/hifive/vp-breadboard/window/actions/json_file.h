#pragma once

#include <QAction>
#include <QString>

class JsonFile : public QAction {
	Q_OBJECT

	QString file;

public:
	JsonFile(QString file, int dir_length);

private slots:
	void JsonFileSelected();

signals:
	void triggered(QString file);
};
