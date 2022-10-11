#include "delete.h"

DeleteButton::DeleteButton(QString text, QWidget *parent) : QPushButton(text, parent) {
	connect(this, &QPushButton::pressed, this, &DeleteButton::deleteElement);
}

void DeleteButton::deleteElement() {
	emit(pressed(parentWidget()->layout()));
}
