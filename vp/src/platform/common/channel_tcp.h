#ifndef RISCV_VP_CHANNEL_TCP_H
#define RISCV_VP_CHANNEL_TCP_H

#include <stdint.h>

#include "channel_fd_if.h"

/*
 * io over a tcp port
 * starts a server and waits for a client
 */
class Channel_TCP final : public Channel_FD_IF {
   public:
	Channel_TCP(unsigned int port);
	virtual ~Channel_TCP();

   private:
	const unsigned int port;

	int open_fd() override;
	void close_fd(int fd) override;
	void write_data(uint8_t) override;
	void handle_input(int fd) override;

	int serverfd = -1;
	int clientfd = -1;
};

#endif  // RISCV_VP_CHANNEL_TCP_H
