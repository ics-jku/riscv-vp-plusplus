#pragma once

#include <QDialog>
#include <QFormLayout>
#include "devices/configurations.h"

class ConfigDialog : public QDialog {
	Q_OBJECT

	QFormLayout *layout;
	Config *config;

	void addValue(ConfigDescription name, QWidget* value);
	void addInt(ConfigDescription name, int value);
	void addBool(ConfigDescription name, bool value);
	void addString(ConfigDescription name, QString value);

public:
	ConfigDialog(QWidget* parent);
	void setConfig(Config* config);

public slots:
	void accept() override;
	void reject() override;

signals:
	void configChanged(Config* config);
};
