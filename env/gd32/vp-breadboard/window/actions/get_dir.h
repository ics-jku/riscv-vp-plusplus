#pragma once

#include <QAction>
#include <QString>

class GetDir : public QAction {
	Q_OBJECT

   public:
	GetDir(QString name);

   private slots:
	void getDirName();

   signals:
	void triggered(QString path);
};
