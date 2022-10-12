#pragma once

#include <QPushButton>

class DeleteButton : public QPushButton {
	Q_OBJECT

public:
	DeleteButton(QWidget *parent=0);

public slots:
	void deleteElement();

signals:
	void pressed(QLayout *layout);
};
