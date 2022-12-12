#pragma once

#include <stdint.h>
#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>

#include <systemc>
#include <tlm_utils/simple_target_socket.h>

#include <thread>
#include <mutex>
#include <queue>

#include "uart_if.h"
#include "core/common/irq_if.h"
#include "util/tlm_map.h"
#include "platform/common/async_event.h"

class FD_ABSTRACT_UART : public UART_IF {
public:
	FD_ABSTRACT_UART(sc_core::sc_module_name, uint32_t);
	~FD_ABSTRACT_UART(void);

protected:
	void start_threads(int fd, bool write_only = false);
	void stop_threads(void);

private:
	virtual void write_data(uint8_t) = 0;
	virtual void handle_input(int fd) = 0;

	void transmit();
	void receive();

	std::thread *rcvthr = NULL, *txthr = NULL;

	bool stop;
	int stop_pipe[2];

	enum {
		NFDS = 2,
	};
	struct pollfd fds[NFDS];
};
