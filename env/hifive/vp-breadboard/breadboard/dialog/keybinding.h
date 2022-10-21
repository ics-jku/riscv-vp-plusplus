#pragma once

#include <QDialog>
#include <QFormLayout>
#include "devices/configurations.h"

class KeybindingDialog : public QDialog {
	Q_OBJECT

	QFormLayout *layout;
	Keys keys;
	DeviceID device;

	void add(int key);

public:
	KeybindingDialog(QWidget* parent);
	void setKeys(DeviceID device, Keys keys);

public slots:
	void accept() override;
	void reject() override;

signals:
	void keysChanged(DeviceID device, Keys keys);
};
