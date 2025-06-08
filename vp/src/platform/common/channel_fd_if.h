#ifndef RISCV_VP_CHANNEL_FD_IF_H
#define RISCV_VP_CHANNEL_FD_IF_H

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

   private:
	virtual void start_handling() override;
	virtual void stop_handling() override;
	virtual int open_fd(void) = 0;
	virtual void close_fd(int fd) = 0;

	virtual void write_data(uint8_t) = 0;
	virtual void handle_input(int fd) = 0;

	void transmitter_threadf();
	void main_threadf();
	void receiver(int fd);

	std::thread *main_thread = nullptr;
	std::thread *transmitter_thread = nullptr;

	bool stop_flag;
	int stop_pipe[2];
};

#endif  // RISCV_VP_CHANNEL_FD_IF_H
