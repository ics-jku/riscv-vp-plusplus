#ifndef RISCV_VP_CHANNEL_FD_IF_H
#define RISCV_VP_CHANNEL_FD_IF_H

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>

#include <thread>

#include "channel_if.h"
#include "platform/common/async_event.h"

/*
 * Derived from Channel_IF (which provides basic rx/tx fifo handling).
 * Adds threaded handling for file-descriptor (FD) based back-end communication interfaces.
 */
class Channel_FD_IF : public Channel_IF {
   public:
	Channel_FD_IF();
	virtual ~Channel_FD_IF(void);

   protected:
	void start_handling(int fd, unsigned int tx_fifo_depth = 1, unsigned int rx_fifo_depth = 1,
	                    bool write_only = false);
	void stop_handling(void);

   private:
	virtual void write_data(uint8_t) = 0;
	virtual void handle_input(int fd) = 0;

	void transmit();
	void receive();

	std::thread *rcvthr = NULL, *txthr = NULL;

	bool stop_flag;
	int stop_pipe[2];

	enum {
		NFDS = 2,
	};
	struct pollfd fds[NFDS];
};

#endif  // RISCV_VP_CHANNEL_FD_IF_H
