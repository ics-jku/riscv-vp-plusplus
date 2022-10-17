#pragma once

#include <QKeySequenceEdit>

class KeyEdit : public QKeySequenceEdit {
	Q_OBJECT

	void keyPressEvent(QKeyEvent* e) override;

public:
	KeyEdit(int key);

signals:
	void newKey(int key);
	void removeKey(int key);
};
