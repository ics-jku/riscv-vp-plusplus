#ifndef RISCV_VP_CHANNEL_SLIP_H
#define RISCV_VP_CHANNEL_SLIP_H

#include <stdint.h>

#include "channel_fd_if.h"

class Channel_SLIP final : public Channel_FD_IF {
   public:
	Channel_SLIP(std::string netdev) : netdev(netdev){};
	virtual ~Channel_SLIP();

   private:
	const std::string netdev;
	int get_mtu(const char *);
	void send_packet(void);

	int open_fd() override;
	void close_fd(int fd) override;
	void write_data(uint8_t) override;
	void handle_input(int fd) override;

	int tunfd = -1;

	uint8_t *sndbuf = NULL, *rcvbuf = NULL;
	size_t sndsiz, rcvsiz;
};

#endif  // RISCV_VP_CHANNEL_SLIP_H
