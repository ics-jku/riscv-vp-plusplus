#include "channel_console.h"

#include <unistd.h>

#include "core/common/rawmode.h"

/* character → control key */
#define CTRL(c) ((c)&0x1f)

#define KEY_ESC CTRL('a')        /* Ctrl-a (character to enter command mode) */
#define KEY_EXIT 'x'             /* x (character to exit in command mode) */
#define KEY_CEXIT CTRL(KEY_EXIT) /* Ctrl-x (character to exit in command mode) */

Channel_Console::~Channel_Console() {
	stop();
}

int Channel_Console::open_fd() {
	enableRawMode(STDIN_FILENO);
	return STDIN_FILENO;
}

void Channel_Console::close_fd(int fd) {
	disableRawMode(fd);
}

void Channel_Console::handle_input(int fd) {
	uint8_t buf;
	ssize_t nread;

	nread = read(fd, &buf, sizeof(buf));
	if (nread == -1) {
		throw std::system_error(errno, std::generic_category());
	} else if (nread != sizeof(buf)) {
		throw std::runtime_error("short read");
	}

	switch (state) {
		case STATE_NORMAL:
			if (buf != KEY_ESC) {
				// filter out first esc sequence
				rxpush(buf);
			}
			break;
		case STATE_COMMAND:
			handle_cmd(buf);
			break;
	}

	/* update state of input state machine for next run */
	if (buf == KEY_ESC && state != STATE_COMMAND) {
		state = STATE_COMMAND;
	} else {
		state = STATE_NORMAL;
	}
}

void Channel_Console::handle_cmd(uint8_t cmd) {
	switch (cmd) {
		case KEY_ESC: /* double escape */
			rxpush(cmd);
			break;
		case KEY_EXIT:
		case KEY_CEXIT:
			exit(EXIT_SUCCESS);
			break;
		default:
			return; /* unknown command → ignore */
	}
}

void Channel_Console::write_data(uint8_t data) {
	ssize_t nwritten;
	nwritten = write(STDOUT_FILENO, &data, sizeof(data));
	if (nwritten == -1) {
		throw std::system_error(errno, std::generic_category());
	} else if (nwritten != sizeof(data)) {
		throw std::runtime_error("short write");
	}
}
