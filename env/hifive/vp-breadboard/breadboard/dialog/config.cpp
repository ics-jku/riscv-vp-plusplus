#include "config.h"

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
	saveButton->setDefault(true);
	QHBoxLayout *buttons_close = new QHBoxLayout;
	buttons_close->addWidget(closeButton);
	buttons_close->addWidget(saveButton);

	layout = new QFormLayout(this);
	layout->addRow(buttons_close);

	setWindowTitle("Edit config values");
}

void ConfigDialog::addValue(ConfigDescription name, QWidget* value) {
	layout->insertRow(0, QString::fromStdString(name), value);
}

void ConfigDialog::addBool(ConfigDescription name, bool value) {
	QCheckBox *box = new QCheckBox("");
	box->setChecked(value);
	connect(box, &QCheckBox::stateChanged, [this, name](int state) {
		auto conf_value = config.find(name);
		if(conf_value!=config.end() && conf_value->second.type == ConfigElem::Type::boolean) {
			conf_value->second.value.boolean = state == Qt::Checked;
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::addInt(ConfigDescription name, int value) {
	QSpinBox *box = new QSpinBox;
	box->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
	box->setWrapping(true);
	box->setValue(value);
	connect(box, QOverload<int>::of(&QSpinBox::valueChanged), [this, name](int newValue) {
		auto conf_value = config.find(name);
		if(conf_value!=config.end() && conf_value->second.type == ConfigElem::Type::integer) {
			conf_value->second.value.integer = newValue;
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::addString(ConfigDescription name, QString value) {
	QLineEdit *box = new QLineEdit(value);
	connect(box, &QLineEdit::textChanged, [this, name](QString text) {
		auto conf_value = config.find(name);
		if(conf_value!=config.end() && conf_value->second.type == ConfigElem::Type::string) {
			config.erase(name);
			QByteArray text_bytes = text.toLocal8Bit();
			config.emplace(name, ConfigElem{text_bytes.data()});
		}
	});
	this->addValue(name, box);
}

void ConfigDialog::setConfig(DeviceID device, Config config) {
	this->device = device;
	this->config = config;
	for(auto const& [description, element] : config) {
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
	while(layout->rowCount() > 1) {
		layout->removeRow(0);
	}
	config.clear();
	device = "";
	QDialog::reject();
}

void ConfigDialog::accept() {
	emit(configChanged(device, config));
	while(layout->rowCount() > 1) {
		layout->removeRow(0);
	}
	config.clear();
	device = "";
	QDialog::accept();
}
