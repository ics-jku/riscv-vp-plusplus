#pragma once

#include <QDialog>
#include <QFormLayout>
#include "devices/configurations.h"

class KeybindingDialog : public QDialog {
	Q_OBJECT

	QFormLayout *layout;

public:
	KeybindingDialog(QWidget* parent);
	void setKeys(Keys keys);

public slots:
	void accept() override;

private slots:
	void add();
};
