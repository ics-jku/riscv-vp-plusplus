#pragma once

#include <QAction>
#include <QString>

class JsonAction : public QAction {
	Q_OBJECT

	QString file;

	public:
		JsonAction(QString file, int dir_length);

	public slots:
		void JsonActionSlot();

	signals:
		void triggered(QString file);
};
