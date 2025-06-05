#include "channel_if.h"

#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <mutex>
#include <queue>
#include <thread>

#define stop_fd (stop_pipe[0])
#define newpollfd(FD) (struct pollfd){.fd = FD, .events = POLLIN | POLLERR, .revents = 0};

Channel_IF::Channel_IF() {
	if (pipe(stop_pipe) == -1) {
		throw std::system_error(errno, std::generic_category());
	}
}

Channel_IF::~Channel_IF(void) {
	close(stop_pipe[0]);
	close(stop_pipe[1]);
}

unsigned int Channel_IF::get_tx_fifo_size() {
	unsigned int ret = 0;
	txmtx.lock();
	ret = tx_fifo.size();
	txmtx.unlock();
	return ret;
}

unsigned int Channel_IF::get_rx_fifo_size() {
	unsigned int ret = 0;
	rcvmtx.lock();
	ret = rx_fifo.size();
	rcvmtx.unlock();
	return ret;
}

unsigned int Channel_IF::rxpull() {
	unsigned int ret;

	rcvmtx.lock();
	if (rx_fifo.empty()) {
		ret = 1 << 31;
	} else {
		ret = rx_fifo.front();
		rx_fifo.pop();
		spost(&rxempty);
	}
	rcvmtx.unlock();

	return ret;
}

bool Channel_IF::txpush(uint8_t txdata) {
	// from SoC to remote
	txmtx.lock();
	if (tx_fifo.size() >= tx_fifo_depth) {
		txmtx.unlock();
		return false; /* write is ignored */
	}

	tx_fifo.push(txdata);
	txmtx.unlock();
	spost(&txfull);
	return true;
}

void Channel_IF::start_threads(int fd, unsigned int tx_fifo_depth, unsigned int rx_fifo_depth, bool write_only) {
	this->tx_fifo_depth = tx_fifo_depth;
	this->rx_fifo_depth = rx_fifo_depth;

	if (sem_init(&txfull, 0, 0)) {
		throw std::system_error(errno, std::generic_category());
	}
	if (sem_init(&rxempty, 0, rx_fifo_depth)) {
		throw std::system_error(errno, std::generic_category());
	}

	fds[0] = newpollfd(stop_fd);
	fds[1] = newpollfd(fd);

	stop_flag = false;
	if (!write_only) {
		rcvthr = new std::thread(&Channel_IF::receive, this);
	}
	txthr = new std::thread(&Channel_IF::transmit, this);
}

void Channel_IF::stop_threads(void) {
	stop_flag = true;

	if (txthr) {
		spost(&txfull);  // unblock transmit thread
		txthr->join();
		delete txthr;
	}

	if (rcvthr) {
		uint8_t byte = 0;
		if (write(stop_pipe[1], &byte, sizeof(byte)) == -1) {
			// unblock receive thread
			err(EXIT_FAILURE, "couldn't unblock uart receive thread");
		}
		spost(&rxempty);  // unblock receive thread
		rcvthr->join();
		delete rcvthr;
	}
}

uint8_t Channel_IF::txpull() {
	uint8_t data;

	swait(&txfull);
	if (tx_fifo.size() == 0) {
		// Other thread will only increase count, not decrease
		return 0;
	}
	txmtx.lock();
	data = tx_fifo.front();
	tx_fifo.pop();
	txmtx.unlock();

	return data;
}

void Channel_IF::rxpush(uint8_t data) {
	swait(&rxempty);
	rcvmtx.lock();
	rx_fifo.push(data);
	rcvmtx.unlock();
	asyncEvent.notify();
}

void Channel_IF::transmit(void) {
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

void Channel_IF::receive(void) {
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

void Channel_IF::swait(sem_t *sem) {
	if (sem_wait(sem)) {
		throw std::system_error(errno, std::generic_category());
	}
}

void Channel_IF::spost(sem_t *sem) {
	if (sem_post(sem)) {
		throw std::system_error(errno, std::generic_category());
	}
}
