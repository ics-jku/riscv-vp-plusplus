#include "channel_tunnel.h"

#include <semaphore.h>

Channel_Tunnel::~Channel_Tunnel() {
	stop();
}

void Channel_Tunnel::start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) {
	start_handling(tx_fifo_depth, rx_fifo_depth);

	stop_flag = false;
	rx_worker = std::thread(std::bind(&Channel_Tunnel::rx_dequeue, this));
}

void Channel_Tunnel::stop() {
	stop_flag = true;
	if (rx_worker.joinable()) {
		post_rxempty();  // unblock receive thread
		rx_worker.join();
	}
	if (tx_worker.joinable()) {
		post_txfull();  // unblock transmit thread
	}

	stop_handling();
}

void Channel_Tunnel::nonblock_receive(gpio::UART_Bytes bytes) {
	const std::lock_guard<std::mutex> lock(nonblock_rx_mutex);
	if (nonblocking_rx_queue.size() > DROP_AT_FIFO_DEPTH - get_rx_fifo_size()) {
		std::cerr << "[tunnel-uart] Warn: pre-rx_queue growing to " << nonblocking_rx_queue.size() << " byte."
		          << std::endl;
		std::cerr << "              The VP can probably not keep up with the remote." << std::endl;
	}
	if (nonblocking_rx_queue.size() > DROP_AT_FIFO_DEPTH) {
		// Dropping elements
		return;
	}
	for (const auto& byte : bytes) {
		nonblocking_rx_queue.push(byte);
	}
}

void Channel_Tunnel::register_transmit_function(UartTXFunction fun) {
	tx_worker = std::thread(std::bind(&Channel_Tunnel::tx_dequeue, this, fun));
}

void Channel_Tunnel::rx_dequeue() {
	while (!stop_flag) {
		nonblock_rx_mutex.lock();
		if (!nonblocking_rx_queue.empty() && !stop_flag) {
			gpio::UART_Byte byte = nonblocking_rx_queue.front();
			nonblocking_rx_queue.pop();
			nonblock_rx_mutex.unlock();
			rxpush(byte);  // may block
		} else {
			nonblock_rx_mutex.unlock();
		}
	}
}

void Channel_Tunnel::tx_dequeue(UartTXFunction fun) {
	while (!stop_flag) {
		// TODO: Perhaps make this a SysC-Thread for more realism?
		const auto data = txpull();  // TODO: Optimize if more elems are in tx queue
		if (stop_flag) {
			break;
		}
		fun(gpio::UART_Bytes{data});
	}
}
