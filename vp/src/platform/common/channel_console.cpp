#include "channel_console.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <systemc>

#include "core/common/rawmode.h"

/* character → control key */
#define CTRL(c) ((c)&0x1f)

#define KEY_ESC CTRL('a')        /* Ctrl-a (character to enter command mode) */
#define KEY_EXIT 'x'             /* x (character to exit in command mode) */
#define KEY_CEXIT CTRL(KEY_EXIT) /* Ctrl-x (character to exit in command mode) */

void Channel_Console::start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) {
	// If stdin isn't a tty, it doesn't make much sense to poll from it.
	// In this case, we will run the UART in write-only mode.
	bool write_only = !isatty(STDIN_FILENO);

	enableRawMode(STDIN_FILENO);
	start_threads(STDIN_FILENO, tx_fifo_depth, rx_fifo_depth, write_only);
}

void Channel_Console::stop() {
	stop_threads();
	disableRawMode(STDIN_FILENO);
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
