/* Copied from linenoise <https://github.com/antirez/linenoise> with slight modifications.
 *
 * Copyright (c) 2019, Sören Tempel <tempel at uni-bremen dot de>
 * Copyright (c) 2010-2014, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 */

#include <system_error>

#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stddef.h>

#include "rawmode.h"

static int rawfd = -1;
static struct termios orig_termios;

static void reset_term(void) {
	// Signal might arrive before enableRawMode()
	if (rawfd < 0)
		return;

	disableRawMode(rawfd);
}

static void sighandler(int num) {
	(void)num;
	reset_term();
	exit(EXIT_FAILURE);
}

static void sethandler(void) {
	size_t i;
	struct sigaction act;
	int signals[] = {SIGINT, SIGTERM, SIGQUIT, SIGHUP};

	act.sa_flags = 0;
	act.sa_handler = sighandler;
	if (sigfillset(&act.sa_mask) == -1)
		throw std::system_error(errno, std::generic_category());

	for (i = 0; i < (sizeof(signals) / sizeof(signals[0])); i++) {
		if (sigaction(signals[i], &act, NULL))
			throw std::system_error(errno, std::generic_category());
	}

	/* also make sure we cleanup on exit(3) */
	if (atexit(reset_term))
		throw std::system_error(errno, std::generic_category());
}

void enableRawMode(int fd) {
	struct termios raw;

	// Check if rawm ode was already activated
	if (rawfd >= 0)
		return;

	if (!isatty(STDIN_FILENO))
		return; // not a tty, nothing to do

	if (tcgetattr(fd, &orig_termios) == -1)
		goto fatal;

	raw = orig_termios; /* modify the original mode */
	/* input modes: no break, no CR to NL, no parity check, no strip char,
	 * no start/stop output control. */
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);
	/* local modes - choing off, canonical off, no extended functions,
	 * no signal chars (^Z,^C) */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	/* control chars - set return condition: min number of bytes and timer.
	 * We want read to return every single byte, without timeout. */
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

	/* put terminal in raw mode after flushing */
	if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
		goto fatal;

	rawfd = fd;
	sethandler();

	return;
fatal:
	throw std::system_error(errno, std::generic_category());
}

void disableRawMode(int fd) {
	if (rawfd < 0)
		return;

	if (tcsetattr(fd, TCSAFLUSH, &orig_termios) == -1)
		throw std::system_error(errno, std::generic_category());

	rawfd = -1;
}
