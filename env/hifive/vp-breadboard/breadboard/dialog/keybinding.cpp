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
	connect(addButton, &QPushButton::pressed, [this](){add(QKeySequence());});
	layout->addRow(addButton);
	layout->addRow(buttons);

	setWindowTitle("Edit keybindings");
}

void KeybindingDialog::add(QKeySequence sequence) {
	keys.emplace(sequence[0]); // TODO intern tatsächlich Tastenkombinationen
	QKeySequenceEdit *box = new QKeySequenceEdit(sequence);
	connect(box, &QKeySequenceEdit::keySequenceChanged, [this](QKeySequence newSequence) {
		add(newSequence); // TODO remove/edit old one
	});
	layout->insertRow(0, box); // TODO delete
}

void KeybindingDialog::accept() {
	emit(keysChanged(keys));
	while(layout->rowCount() > 2) {
		layout->removeRow(0);
	}
	QDialog::accept();
}

void KeybindingDialog::setKeys(Keys keys) {
	for(const Key& key : keys) {
		add(QKeySequence(key));
	}
}
