#include "channel_fd_if.h"

#include <err.h>
#include <unistd.h>

#define stop_fd (stop_pipe[0])
#define newpollfd(FD) (struct pollfd){.fd = FD, .events = POLLIN | POLLERR, .revents = 0};

Channel_FD_IF::Channel_FD_IF() {
	if (pipe(stop_pipe) == -1) {
		throw std::system_error(errno, std::generic_category());
	}
}

Channel_FD_IF::~Channel_FD_IF(void) {
	close(stop_pipe[0]);
	close(stop_pipe[1]);
}

void Channel_FD_IF::start_handling(int fd, unsigned int tx_fifo_depth, unsigned int rx_fifo_depth, bool write_only) {
	Channel_IF::start_handling(tx_fifo_depth, rx_fifo_depth);

	fds[0] = newpollfd(stop_fd);
	fds[1] = newpollfd(fd);

	stop_flag = false;
	if (!write_only) {
		rcvthr = new std::thread(&Channel_FD_IF::receive, this);
	}
	txthr = new std::thread(&Channel_FD_IF::transmit, this);
}

void Channel_FD_IF::stop_handling(void) {
	stop_flag = true;

	if (txthr) {
		post_txfull();  // unblock transmit thread
		txthr->join();
		delete txthr;
	}

	if (rcvthr) {
		uint8_t byte = 0;
		if (write(stop_pipe[1], &byte, sizeof(byte)) == -1) {
			// unblock receive thread
			err(EXIT_FAILURE, "couldn't unblock uart receive thread");
		}
		post_rxempty();  // unblock receive thread
		rcvthr->join();
		delete rcvthr;
	}

	Channel_IF::stop_handling();
}

void Channel_FD_IF::transmit(void) {
	uint8_t data;

	while (!stop_flag) {
		data = txpull();
		if (stop_flag) {
			break;
		}
		asyncEvent.notify();
		write_data(data);
	}
}

void Channel_FD_IF::receive(void) {
	while (!stop_flag) {
		if (poll(fds, (nfds_t)NFDS, -1) == -1) {
			throw std::system_error(errno, std::generic_category());
		}

		/* stop_fd is checked first as it is fds[0] */
		for (size_t i = 0; i < NFDS; i++) {
			int fd = fds[i].fd;
			short ev = fds[i].revents;

			if (ev & POLLERR) {
				throw std::runtime_error("received unexpected POLLERR");
			} else if (ev & POLLIN) {
				if (fd == stop_fd) {
					break;
				} else {
					handle_input(fd);
				}
			}
		}
	}
}
