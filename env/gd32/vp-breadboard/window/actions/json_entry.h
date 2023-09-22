#pragma once

#include <QAction>
#include <QString>

class JsonEntry : public QAction {
	Q_OBJECT

	QString file;

   public:
	JsonEntry(QString file, QString title);

   private slots:
	void JsonEntrySelected();

   signals:
	void triggered(QString file);
};
