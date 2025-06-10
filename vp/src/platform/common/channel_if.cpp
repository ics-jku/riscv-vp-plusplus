#include "channel_if.h"

void Channel_IF::start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) {
	this->tx_fifo_depth = tx_fifo_depth;
	this->rx_fifo_depth = rx_fifo_depth;

	if (sem_init(&txfull, 0, 0)) {
		throw std::system_error(errno, std::generic_category());
	}
	if (sem_init(&rxempty, 0, rx_fifo_depth)) {
		throw std::system_error(errno, std::generic_category());
	}

	start_handling();
}

void Channel_IF::stop() {
	stop_handling();
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
	asyncEvent.notify();

	return data;
}

void Channel_IF::rxpush(uint8_t data) {
	swait(&rxempty);
	rcvmtx.lock();
	rx_fifo.push(data);
	rcvmtx.unlock();
	asyncEvent.notify();
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
