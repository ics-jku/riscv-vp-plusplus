#include "keybinding.h"

#include <QHBoxLayout>
#include <QPushButton>

#include "keyedit.h"

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
	connect(addButton, &QPushButton::pressed, [this](){add(0);});
	layout->addRow(addButton);
	layout->addRow(buttons);

	setWindowTitle("Edit keybindings");
}

void KeybindingDialog::add(int key) {
	keys.emplace(key);
	QHBoxLayout *value_layout = new QHBoxLayout;
	KeyEdit *box = new KeyEdit(key);
	connect(box, &KeyEdit::removeKey, [this](int old_key) {
		keys.erase(old_key);
	});
	connect(box, &KeyEdit::newKey, [this](int new_key) {
		keys.emplace(new_key);
	});
	value_layout->addWidget(box);
	QPushButton *delete_value = new QPushButton("Delete");
	connect(delete_value, &QPushButton::pressed, [this, value_layout, box](){
		keys.erase(box->keySequence()[0]);
		layout->removeRow(value_layout);
	});
	value_layout->addWidget(delete_value);
	layout->insertRow(0, value_layout);
}

void KeybindingDialog::accept() {
	emit(keysChanged(device, keys));
	while(layout->rowCount() > 2) {
		layout->removeRow(0);
	}
	keys.clear();
	device = "";
	QDialog::accept();
}

void KeybindingDialog::reject() {
	while(layout->rowCount() > 2) {
		layout->removeRow(0);
	}
	keys.clear();
	device = "";
	QDialog::reject();
}

void KeybindingDialog::setKeys(DeviceID device, Keys keys) {
	this->keys.clear();
	this->device = device;
	for(const Key& key : keys) {
		add(key);
	}
}
