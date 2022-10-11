#include "config.h"
#include "delete.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
	QPushButton *closeButton = new QPushButton("Close");
	connect(closeButton, &QAbstractButton::pressed, this, &QDialog::reject);
	QPushButton *saveButton = new QPushButton("Save");
	connect(saveButton, &QAbstractButton::pressed, this, &QDialog::accept);
	QHBoxLayout *buttons_close = new QHBoxLayout;
	buttons_close->addWidget(closeButton);
	buttons_close->addWidget(saveButton);
	QPushButton *addBoolButton = new QPushButton("Add bool value");
	connect(addBoolButton, &QPushButton::pressed, this, &ConfigDialog::addBool);
	QPushButton *addIntButton = new QPushButton("Add int value");
	connect(addIntButton, &QPushButton::pressed, this, &ConfigDialog::addInt);
	QPushButton *addStringButton = new QPushButton("Add string value");
	connect(addStringButton, &QPushButton::pressed, this, &ConfigDialog::addString);
	QHBoxLayout *buttons_add = new QHBoxLayout;
	buttons_add->addWidget(addBoolButton);
	buttons_add->addWidget(addIntButton);
	buttons_add->addWidget(addStringButton);

	layout = new QFormLayout(this);
	layout->addRow(buttons_add);
	layout->addRow(buttons_close);

	setWindowTitle("Edit config values");
}

void ConfigDialog::addBool() { // TODO

}

void ConfigDialog::addInt() { // TODO

}

void ConfigDialog::addString() { // TODO

}

void ConfigDialog::reject() {
	while(layout->rowCount() > 1) {
		layout->removeRow(0);
	}
	QDialog::reject();
}

void ConfigDialog::accept() { // TODO save
	while(layout->rowCount() > 1) {
		layout->removeRow(0);
	}
	QDialog::accept();
}

void ConfigDialog::removeElement(QLayout *element_layout) { // TODO does not work
	layout->removeRow(element_layout);
}

void ConfigDialog::setConfig(Config* config) { // TODO edit name
	for(auto const& [description, element] : (*config)) {
		switch(element.type) {
		case ConfigElem::Type::boolean: {
			QHBoxLayout *value_layout = new QHBoxLayout;
			QCheckBox *value = new QCheckBox("");
			value->setChecked(element.value.boolean);
			value_layout->addWidget(value);
			DeleteButton *delete_value = new DeleteButton("Delete");
			connect(delete_value, &DeleteButton::pressed, this, &ConfigDialog::removeElement);
			value_layout->addWidget(delete_value);
			layout->insertRow(0, QString::fromStdString(description), value_layout);
			break;
		}
		case ConfigElem::Type::integer: {
			QHBoxLayout *value_layout = new QHBoxLayout;
			QSpinBox *value = new QSpinBox;
			value->setValue(element.value.integer);
			value_layout->addWidget(value);
			DeleteButton *delete_value = new DeleteButton("Delete");
			connect(delete_value, &DeleteButton::pressed, this, &ConfigDialog::removeElement);
			value_layout->addWidget(delete_value);
			layout->insertRow(0, QString::fromStdString(description), value_layout);
			break;
		}
		case ConfigElem::Type::string: {
			QLineEdit *value = new QLineEdit(QString::fromLocal8Bit(element.value.string));
			layout->insertRow(0, QString::fromStdString(description), value);
			break;
		}
		}
	}
}
