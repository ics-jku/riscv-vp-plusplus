#include "config.h"
#include "delete.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>

#include <iostream>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
	QPushButton *closeButton = new QPushButton("Close");
	connect(closeButton, &QAbstractButton::pressed, this, &QDialog::reject);
	QPushButton *saveButton = new QPushButton("Save");
	connect(saveButton, &QAbstractButton::pressed, this, &QDialog::accept);
	QHBoxLayout *buttons_close = new QHBoxLayout;
	buttons_close->addWidget(closeButton);
	buttons_close->addWidget(saveButton);
	QPushButton *addBoolButton = new QPushButton("Add bool value");
	connect(addBoolButton, &QPushButton::pressed, [this](){this->addBool("", false);});
	QPushButton *addIntButton = new QPushButton("Add int value");
	connect(addIntButton, &QPushButton::pressed, [this](){this->addInt("", 0);});
	QPushButton *addStringButton = new QPushButton("Add string value");
	connect(addStringButton, &QPushButton::pressed, [this](){this->addString("","");});
	QHBoxLayout *buttons_add = new QHBoxLayout;
	buttons_add->addWidget(addBoolButton);
	buttons_add->addWidget(addIntButton);
	buttons_add->addWidget(addStringButton);

	layout = new QFormLayout(this);
	layout->addRow(buttons_add);
	layout->addRow(buttons_close);

	this->config = new Config();

	setWindowTitle("Edit config values");
}

void ConfigDialog::addValue(ConfigDescription name, QWidget* value) {
	QHBoxLayout *value_layout = new QHBoxLayout;
	value_layout->addWidget(value);
	DeleteButton *delete_button = new DeleteButton;
	connect(delete_button, &DeleteButton::pressed, this, &ConfigDialog::removeElement);
	value_layout->addWidget(delete_button);
	QLineEdit *name_edit = new QLineEdit(QString::fromStdString(name));
	connect(name_edit, &QLineEdit::textChanged, [this, name](QString text) { // TODO
		auto conf_value = config->find(name);
		if(conf_value!=config->end()) {
			config->emplace(text.toStdString(), conf_value->second);
		}
	});
	layout->insertRow(0, name_edit, value_layout);
}

void ConfigDialog::addBool(ConfigDescription name, bool value) {
	config->emplace(name, ConfigElem{value});
	QCheckBox *box = new QCheckBox("");
	box->setChecked(value);
	connect(box, &QCheckBox::stateChanged, [this, name](int state) {
		auto conf_value = config->find(name);
		if(conf_value!=config->end() && conf_value->second.type == ConfigElem::Type::boolean) {
			conf_value->second.value.boolean = state == Qt::Checked;
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::addInt(ConfigDescription name, int value) {
	config->emplace(name, ConfigElem{(int64_t)value});
	QSpinBox *box = new QSpinBox;
	box->setValue(value);
	connect(box, QOverload<int>::of(&QSpinBox::valueChanged), [this, name](int newValue) {
		auto conf_value = config->find(name);
		if(conf_value!=config->end() && conf_value->second.type == ConfigElem::Type::integer) {
			conf_value->second.value.integer = newValue;
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::addString(ConfigDescription name, QString value) {
	QByteArray value_bytes = value.toLocal8Bit();
	config->emplace(name, ConfigElem{value_bytes.data()});
	QLineEdit *box = new QLineEdit(value);
	connect(box, &QLineEdit::textChanged, [this, name](QString text) {
		auto conf_value = config->find(name);
		if(conf_value!=config->end() && conf_value->second.type == ConfigElem::Type::string) {
			config->erase(name);
			QByteArray text_bytes = text.toLocal8Bit();
			config->emplace(name, ConfigElem{text_bytes.data()});
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::removeElement(QLayout *element_layout) { // TODO does not work
	layout->removeRow(element_layout);
}

void ConfigDialog::setConfig(Config* config) {
	for(auto const& [description, element] : *config) {
		switch(element.type) {
		case ConfigElem::Type::integer: {
			addInt(description, element.value.integer);
			break;
		}
		case ConfigElem::Type::boolean: {
			addBool(description, element.value.boolean);
			break;
		}
		case ConfigElem::Type::string: {
			addString(description, QString::fromLocal8Bit(element.value.string));
			break;
		}
		}
	}
}

void ConfigDialog::reject() {
	while(layout->rowCount() > 2) {
		layout->removeRow(0);
	}
	QDialog::reject();
}

void ConfigDialog::accept() {
	for(auto const& [description, element] : *config) {
		switch(element.type) {
		case ConfigElem::Type::integer: {
			std::cout << "Integer value " << description << ": " << element.value.integer << std::endl;
			break;
		}
		case ConfigElem::Type::boolean: {
			std::cout << "Boolean value " << description << ": " << element.value.boolean << std::endl;
			break;
		}
		case ConfigElem::Type::string: {
			std::cout << "String value " << description << ": " << element.value.string << std::endl;
			break;
		}
		}
	}
	emit(configChanged(config));
	while(layout->rowCount() > 1) {
		layout->removeRow(0);
	}
	QDialog::accept();
}
