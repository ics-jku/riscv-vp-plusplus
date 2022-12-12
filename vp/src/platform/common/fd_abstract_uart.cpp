#include <fcntl.h>
#include <poll.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <mutex>
#include <queue>
#include <thread>
#include <err.h>

#include "fd_abstract_uart.h"

#define stop_fd (stop_pipe[0])
#define newpollfd(FD) \
	(struct pollfd){.fd = FD, .events = POLLIN | POLLERR, .revents = 0};

FD_ABSTRACT_UART::FD_ABSTRACT_UART(sc_core::sc_module_name name, uint32_t irqsrc) : UART_IF(name, irqsrc) {
	stop = false;
	if (pipe(stop_pipe) == -1)
		throw std::system_error(errno, std::generic_category());
}

FD_ABSTRACT_UART::~FD_ABSTRACT_UART(void) {
	close(stop_pipe[0]);
	close(stop_pipe[1]);
}

void FD_ABSTRACT_UART::start_threads(int fd, bool write_only) {
	fds[0] = newpollfd(stop_fd);
	fds[1] = newpollfd(fd);

	if (!write_only)
		rcvthr = new std::thread(&FD_ABSTRACT_UART::receive, this);
	txthr = new std::thread(&FD_ABSTRACT_UART::transmit, this);
}

void FD_ABSTRACT_UART::stop_threads(void) {
	stop = true;

	if (txthr) {
		spost(&txfull); // unblock transmit thread
		txthr->join();
		delete txthr;
	}

	if (rcvthr) {
		uint8_t byte = 0;
		if (write(stop_pipe[1], &byte, sizeof(byte)) == -1) // unblock receive thread
			err(EXIT_FAILURE, "couldn't unblock uart receive thread");
		spost(&rxempty); // unblock receive thread
		rcvthr->join();
		delete rcvthr;
	}
}

void FD_ABSTRACT_UART::transmit(void) {
	uint8_t data;

	while (!stop) {
		data = txpull();
		if(stop)
			break;
		asyncEvent.notify();
		write_data(data);
	}
}

void FD_ABSTRACT_UART::receive(void) {
	while (!stop) {
		if (poll(fds, (nfds_t)NFDS, -1) == -1)
			throw std::system_error(errno, std::generic_category());

		/* stop_fd is checked first as it is fds[0] */
		for (size_t i = 0; i < NFDS; i++) {
			int fd = fds[i].fd;
			short ev = fds[i].revents;

			if (ev & POLLERR) {
				throw std::runtime_error("received unexpected POLLERR");
			} else if (ev & POLLIN) {
				if (fd == stop_fd)
					break;
				else
					handle_input(fd);
			}
		}
	}
}
