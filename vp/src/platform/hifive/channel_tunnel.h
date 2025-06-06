#pragma once

#include <inttypes.h>

#include <functional>
#include <gpio-common.hpp>  // For UART_Byte types
#include <mutex>
#include <queue>
#include <thread>

#include "platform/common/channel_if.h"

typedef std::function<void(gpio::UART_Bytes)> UartRXFunction;  // from server to peripheral
typedef std::function<void(gpio::UART_Bytes)> UartTXFunction;  // from peripheral to server

class Channel_Tunnel final : public Channel_IF {
	static constexpr unsigned DROP_AT_FIFO_DEPTH = 1600;

   public:
	virtual ~Channel_Tunnel();

	void start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) override;
	void stop() override;

	void nonblock_receive(gpio::UART_Bytes bytes);
	void register_transmit_function(UartTXFunction fun);

   private:
	std::queue<gpio::UART_Byte> nonblocking_rx_queue;
	std::mutex nonblock_rx_mutex;

	bool stop_flag;
	std::thread rx_worker;
	void rx_dequeue();

	std::thread tx_worker;
	void tx_dequeue(UartTXFunction fun);
};
