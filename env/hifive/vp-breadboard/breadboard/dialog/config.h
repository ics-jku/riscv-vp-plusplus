#pragma once

#include <QDialog>
#include <QFormLayout>
#include "devices/configurations.h"

class ConfigDialog : public QDialog {
	Q_OBJECT

	QFormLayout *layout;

public:
	ConfigDialog(QWidget* parent);
	void setConfig(Config* config);

public slots:
	void accept() override;
	void reject() override;

private slots:
	void removeElement(QLayout *layout);
	void addInt();
	void addBool();
	void addString();
};
