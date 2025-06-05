#pragma once

#include <inttypes.h>

#include <functional>
#include <gpio-common.hpp>  // For UART_Byte types
#include <mutex>
#include <queue>

// #include "uart_if.h"

typedef std::function<void(gpio::UART_Bytes)> UartRXFunction;  // from server to peripheral
typedef std::function<void(gpio::UART_Bytes)> UartTXFunction;  // from peripheral to server

/* Currently currently not supported - TODO: tunnel-uart must be reworked to channel_tunnel (derived from Channel_IF) */
#if 0
class Tunnel_UART : public UART_IF {
	static constexpr unsigned DROP_AT_FIFO_DEPTH = 1600;

   public:
	Tunnel_UART(sc_core::sc_module_name, uint32_t irqsrc);
	virtual ~Tunnel_UART(void);

	void nonblock_receive(gpio::UART_Bytes bytes);
	void register_transmit_function(UartTXFunction fun);

   private:
	std::queue<gpio::UART_Byte> nonblocking_rx_queue;
	std::mutex nonblock_rx_mutex;

	bool stop;
	std::thread rx_worker;
	void rx_dequeue();

	std::thread tx_worker;
	void tx_dequeue(UartTXFunction fun);
};
#endif
