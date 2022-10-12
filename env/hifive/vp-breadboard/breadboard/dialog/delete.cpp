#include "delete.h"

DeleteButton::DeleteButton(QWidget *parent) : QPushButton("Delete", parent) {
	connect(this, &QPushButton::pressed, this, &DeleteButton::deleteElement);
}

void DeleteButton::deleteElement() {
	emit(pressed(parentWidget()->layout()));
}
