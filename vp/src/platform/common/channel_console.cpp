#include "channel_console.h"

#include <unistd.h>

#include "core/common/rawmode.h"

/* character → control key */
#define CTRL(c) ((c) & 0x1f)

#define KEY_ESC CTRL('a')        /* Ctrl-a (character to enter command mode) */
#define KEY_NONE 0               /* no key pressed */
#define KEY_HELP 'h'             /* h (print help) */
#define KEY_TRACE 't'            /* t (toggle trace mode) */
#define KEY_STATS 's'            /* s (print statistics) */
#define KEY_DATADMI 'D'          /* D (toggle data DMI) */
#define KEY_DBBCACHE 'd'         /* d (toggle dbbcache) */
#define KEY_LSCACHE 'l'          /* l (toggle lscache) */
#define KEY_QUIT 'q'             /* q (character to quit (sc_stop) in command mode) */
#define KEY_EXIT 'x'             /* x (character to exit (exit) in command mode) */
#define KEY_CEXIT CTRL(KEY_EXIT) /* Ctrl-x (character to exit in command mode) */

Channel_Console::Channel_Console(sc_core::sc_module_name, std::set<debug_target_if *> debug_targets)
    : debug_targets(debug_targets) {
	cmd_requested = KEY_NONE;
	SC_METHOD(handle_cmd);
	sensitive << cmd_asyncEvent;
}

Channel_Console::~Channel_Console() {
	stop();
}

void Channel_Console::debug_targets_toggle_trace_mode(void) {
	trace_mode = !trace_mode;
	if (trace_mode) {
		std::cout << "CONSOLE: trace mode enabled" << std::endl;
	}
	for (debug_target_if *debug_target : debug_targets) {
		debug_target->enable_trace(trace_mode);
	}
	if (!trace_mode) {
		std::cout << "CONSOLE: trace mode disabled" << std::endl;
	}
}

bool Channel_Console::debug_targets_datadmi_is_enabled(void) {
	if (debug_targets.size() == 0) {
		/* no debug targets -> false */
		return false;
	}

	/* determine the state by looking at the first debug target */
	return (*debug_targets.begin())->datadmi_enabled();
}

void Channel_Console::debug_targets_toggle_datadmi(void) {
	bool state = debug_targets_datadmi_is_enabled();

	/* switch all debug targets */
	for (debug_target_if *debug_target : debug_targets) {
		debug_target->enable_datadmi(!state);
	}
	std::cout << "CONSOLE: datadmi:  " << (debug_targets_datadmi_is_enabled() ? "enabled" : "disabled") << std::endl;
}

bool Channel_Console::debug_targets_dbbcache_is_enabled(void) {
	if (debug_targets.size() == 0) {
		/* no debug targets -> false */
		return false;
	}

	/* determine the state by looking at the first debug target */
	return (*debug_targets.begin())->dbbcache_enabled();
}

void Channel_Console::debug_targets_toggle_dbbcache(void) {
	bool state = debug_targets_dbbcache_is_enabled();

	/* switch all debug targets */
	for (debug_target_if *debug_target : debug_targets) {
		debug_target->enable_dbbcache(!state);
	}
	std::cout << "CONSOLE: dbbcache: " << (debug_targets_dbbcache_is_enabled() ? "enabled" : "disabled") << std::endl;
}

bool Channel_Console::debug_targets_lscache_is_enabled(void) {
	if (debug_targets.size() == 0) {
		/* no debug targets -> false */
		return false;
	}

	/* determine the state by looking at the first debug target */
	return (*debug_targets.begin())->lscache_enabled();
}

void Channel_Console::debug_targets_toggle_lscache(void) {
	bool state = debug_targets_lscache_is_enabled();

	/* switch all debug targets */
	for (debug_target_if *debug_target : debug_targets) {
		debug_target->enable_lscache(!state);
	}
	std::cout << "CONSOLE: lscache:  " << (debug_targets_lscache_is_enabled() ? "enabled" : "disabled") << std::endl;
}

void Channel_Console::debug_targets_print_stats(void) {
	std::cout << "CONSOLE: print stats" << std::endl;
	std::cout << "++++++++++++++++++++" << std::endl;
	for (debug_target_if *debug_target : debug_targets) {
		debug_target->print_stats();
	}
	std::cout << "CONSOLE: datadmi:  " << (debug_targets_datadmi_is_enabled() ? "enabled" : "disabled") << std::endl;
	std::cout << "CONSOLE: dbbcache: " << (debug_targets_dbbcache_is_enabled() ? "enabled" : "disabled") << std::endl;
	std::cout << "CONSOLE: lscache:  " << (debug_targets_lscache_is_enabled() ? "enabled" : "disabled") << std::endl;
	std::cout << "++++++++++++++++++++" << std::endl;
}

int Channel_Console::open_fd() {
	/*
	 * If stdin is not a tty (e.g. vp started with nohup), reads may fail
	 * -> Disable the receiver in this case!
	 */
	set_write_only(!isatty(STDIN_FILENO));

	std::cout << "CONSOLE: press ^A-h for help" << std::endl;

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
			switch (buf) {
				case KEY_ESC:
					/* double escape */
					rxpush(buf);
					break;
				case KEY_EXIT:
				case KEY_CEXIT:
					/* force stop commant (stop immediatly) */
					exit(EXIT_SUCCESS);
					break;
				default:
					/* command */
					trigger_handle_cmd(buf);
			}
			break;
	}

	/* update state of input state machine for next run */
	if (buf == KEY_ESC && state != STATE_COMMAND) {
		state = STATE_COMMAND;
	} else {
		state = STATE_NORMAL;
	}
}

void Channel_Console::trigger_handle_cmd(uint8_t cmd) {
	/*
	 * "handle_input" runs in a different system thread than the Systemc simulation, i.e. we can not directly call
	 * methods for command handling without synchronization. Instead of explicitly adding synchronization between the
	 * "handle_input" thread and systemc thread we move the command handling to the SystemC thread.
	 */
	cmd_requested = cmd;
	cmd_asyncEvent.notify();
}

void Channel_Console::handle_cmd() {
	/* get and reset */
	uint8_t cmd = cmd_requested;
	cmd_requested = KEY_NONE;

	/* do command */
	switch (cmd) {
		case KEY_HELP:
			std::cout << "CONSOLE: HELP:\n"
			          << "    ^a-^a  send ^A (ctrl-a)\n"
			          << "    ^a-h   print this help\n"
			          << "    ^a-s   print stats of debug targets\n"
			          << "    ^a-t   toggle trace mode of debug targets\n"
			          << "    ^a-D   toggle data DMI of debug targets\n"
			          << "           (requires enable at start-up: --use-data-dmi or --use-dmi)\n"
			          << "    ^a-d   toggle dbbcache of debug targets\n"
			          << "    ^a-l   toggle lscache of debug targets (requires support for data-DMI)\n"
			          << "    ^a-q   quit - stop simulation with sc_stop\n"
			          << "    ^a-x   exit - hard stop of simulation with exit" << std::endl;
			break;
		case KEY_TRACE:
			debug_targets_toggle_trace_mode();
			break;
		case KEY_STATS:
			debug_targets_print_stats();
			break;
		case KEY_DATADMI:
			debug_targets_toggle_datadmi();
			break;
		case KEY_DBBCACHE:
			debug_targets_toggle_dbbcache();
			break;
		case KEY_LSCACHE:
			debug_targets_toggle_lscache();
			break;
		case KEY_QUIT:
			sc_core::sc_stop();
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
