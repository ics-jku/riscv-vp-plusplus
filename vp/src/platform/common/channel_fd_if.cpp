#include "channel_fd_if.h"

#include <err.h>
#include <poll.h>
#include <unistd.h>

#define stop_fd (stop_pipe[0])
#define newpollfd(FD) (struct pollfd){.fd = FD, .events = POLLIN | POLLHUP | POLLERR, .revents = 0};

Channel_FD_IF::Channel_FD_IF() {
	if (pipe(stop_pipe) == -1) {
		throw std::system_error(errno, std::generic_category());
	}
}

Channel_FD_IF::~Channel_FD_IF(void) {
	stop();
	close(stop_pipe[0]);
	close(stop_pipe[1]);
}

void Channel_FD_IF::start_handling() {
	stop_flag = false;
	main_thread = new std::thread(&Channel_FD_IF::main_threadf, this);
}

void Channel_FD_IF::stop_handling() {
	stop_flag = true;

	if (main_thread) {
		uint8_t byte = 0;
		if (write(stop_pipe[1], &byte, sizeof(byte)) == -1) {
			// unblock receive thread
			err(EXIT_FAILURE, "couldn't unblock uart receive thread");
		}
		post_rxempty();  // unblock receive thread
		main_thread->join();
		delete main_thread;
		main_thread = nullptr;
	}
}

/* runs also if open_fd fails -> write_data must be ignored in concrete implementation */
void Channel_FD_IF::transmitter_threadf(void) {
	uint8_t data;

	while (!stop_flag) {
		data = txpull();
		if (stop_flag) {
			break;
		}
		write_data(data);
	}
}

void Channel_FD_IF::main_threadf(void) {
	/* start tx thread -> runs always (see above) */
	transmitter_thread = new std::thread(&Channel_FD_IF::transmitter_threadf, this);

	/* handle receive with auto reconnect */
	while (!stop_flag) {
		int fd = open_fd();
		if (fd < 0) {
			/* error -> stop */
			break;
		}

		receiver(fd);
		close_fd(fd);
	}

	/* stop and wait for tx thread to complete */
	stop_flag = true;
	post_txfull();  // unblock transmit thread
	transmitter_thread->join();
	delete transmitter_thread;
	transmitter_thread = nullptr;
}

void Channel_FD_IF::receiver(int fd) {
	const int NFDS = 2;
	struct pollfd fds[NFDS];

	fds[0] = newpollfd(stop_fd);
	fds[1] = newpollfd(fd);

	while (!stop_flag) {
		if (poll(fds, (nfds_t)NFDS, -1) == -1) {
			throw std::system_error(errno, std::generic_category());
		}

		/* stop_fd is checked first as it is fds[0] */
		for (size_t i = 0; i < NFDS; i++) {
			int fd = fds[i].fd;
			short ev = fds[i].revents;

			if (ev & POLLERR) {
				/* some error happend -> stop receiver */
				return;
			} else if (ev & POLLHUP) {
				/* some hang-up happend -> stop receiver */
				return;
			} else if (ev & POLLIN) {
				if (fd == stop_fd) {
					/* stop -> stop receiver */
					return;
				} else {
					handle_input(fd);
				}
			}
		}
	}
}
