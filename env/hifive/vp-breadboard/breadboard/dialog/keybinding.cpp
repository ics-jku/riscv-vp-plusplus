#include "keybinding.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QKeySequenceEdit>

KeybindingDialog::KeybindingDialog(QWidget *parent) : QDialog(parent) {
	layout = new QFormLayout(this);
	QPushButton *closeButton = new QPushButton("Close");
	connect(closeButton, &QAbstractButton::pressed, this, &QDialog::reject);
	QPushButton *saveButton = new QPushButton("Save");
	connect(saveButton, &QAbstractButton::pressed, this, &QDialog::accept);
	QHBoxLayout *buttons = new QHBoxLayout;
	buttons->addWidget(closeButton);
	buttons->addWidget(saveButton);
	QPushButton *addButton = new QPushButton("Add");
	connect(addButton, &QPushButton::pressed, this, &KeybindingDialog::add);
	layout->addRow(addButton);
	layout->addRow(buttons);

	setWindowTitle("Edit keybindings");
}

void KeybindingDialog::add() {
	layout->insertRow(0, new QKeySequenceEdit(QKeySequence()));
}

void KeybindingDialog::accept() { // TODO save, intern als QKeySequence/anders mehrere?
	QDialog::accept();
}

void KeybindingDialog::setKeys(Keys keys) {
	for(const Key& key : keys) {
		layout->insertRow(0, new QKeySequenceEdit(QKeySequence(key))); // TODO delete
	}
}
