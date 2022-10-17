#pragma once

#include <QDialog>
#include <QFormLayout>
#include "devices/configurations.h"

class KeybindingDialog : public QDialog {
	Q_OBJECT

	QFormLayout *layout;
	Keys keys;

	void add(int key);

public:
	KeybindingDialog(QWidget* parent);
	void setKeys(Keys keys);

public slots:
	void accept() override;
	void reject() override;

signals:
	void keysChanged(Keys keys);
};
