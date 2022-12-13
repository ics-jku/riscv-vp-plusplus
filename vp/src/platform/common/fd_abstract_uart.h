#pragma once

#include <stdint.h>
#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>

#include <thread>

#include "uart_if.h"

class FD_ABSTRACT_UART : public UART_IF {
public:
	FD_ABSTRACT_UART(sc_core::sc_module_name, uint32_t);
	virtual ~FD_ABSTRACT_UART(void);

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
