#include "tunnel-uart.hpp"

#include <semaphore.h>

Tunnel_UART::Tunnel_UART(sc_core::sc_module_name name, uint32_t irqsrc) : UART_IF(name, irqsrc)
{
	stop = false;
	rx_worker = std::thread(std::bind(&Tunnel_UART::rx_dequeue, this));
};

Tunnel_UART::~Tunnel_UART(){
	stop = true;
	if(rx_worker.joinable()) {
		spost(&rxempty); // unblock receive thread
		rx_worker.join();
	}
	if(tx_worker.joinable()) {
		spost(&txfull); // unblock transmit thread
	}
}

void Tunnel_UART::nonblock_receive(gpio::UART_Bytes bytes) {
	const std::lock_guard<std::mutex> lock(nonblock_rx_mutex);
	if(nonblocking_rx_queue.size() > DROP_AT_FIFO_DEPTH - UART_FIFO_DEPTH) {
		std::cerr << "[tunnel-uart] Warn: pre-rx_queue growing to " << nonblocking_rx_queue.size() << " byte." << std::endl;
		std::cerr << "              The VP can probably not keep up with the remote." << std::endl;
	}
	if(nonblocking_rx_queue.size() > DROP_AT_FIFO_DEPTH) {
		// Dropping elements
		return;
	}
	for(const auto& byte : bytes) {
		nonblocking_rx_queue.push(byte);
	}
}

void Tunnel_UART::register_transmit_function(UartTXFunction fun) {
	tx_worker = std::thread(std::bind(&Tunnel_UART::tx_dequeue, this, fun));
}

void Tunnel_UART::rx_dequeue() {
	while(!stop) {
		nonblock_rx_mutex.lock();
		if(!nonblocking_rx_queue.empty() && !stop){
			gpio::UART_Byte byte = nonblocking_rx_queue.front();
			nonblocking_rx_queue.pop();
			nonblock_rx_mutex.unlock();
			rxpush(byte);	// may block
		} else {
			nonblock_rx_mutex.unlock();
		}
	}
}

void Tunnel_UART::tx_dequeue(UartTXFunction fun) {
	while(!stop) {
		// TODO: Perhaps make this a SysC-Thread for more realism?
		const auto data = txpull(); // TODO: Optimize if more elems are in tx queue
		if(stop)
			break;
		fun(gpio::UART_Bytes{data});
	}
}
