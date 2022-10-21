#include "keyedit.h"

#include <QKeyEvent>

KeyEdit::KeyEdit(int key) : QKeySequenceEdit(QKeySequence(key)) {}

void KeyEdit::keyPressEvent(QKeyEvent *e) {
	emit(removeKey(keySequence()[0]));
	if(keySequence().count() < 2) {
		QKeySequenceEdit::keyPressEvent(e);
		setKeySequence(QKeySequence(keySequence()[0]));
		emit(newKey(keySequence()[0]));
		clearFocus();
	} else {
		e->ignore();
	}
}
