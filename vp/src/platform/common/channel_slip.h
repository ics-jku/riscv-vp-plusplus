/*
 * Copyright (C) 2026 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 * based on https://github.com/agra-uni-bremen/riscv-vp/commits/master/vp/src/platform/common/slip.{h,cpp}
 */

#ifndef RISCV_VP_CHANNEL_SLIP_H
#define RISCV_VP_CHANNEL_SLIP_H

#include <stdint.h>

#include "channel_fd_if.h"

class Channel_SLIP final : public Channel_FD_IF {
   public:
	Channel_SLIP(std::string netdev) : netdev(netdev) {};
	virtual ~Channel_SLIP();

   private:
	int get_mtu(const char *);
	const std::string netdev;
	int tunfd = -1;

	/* Channel_FD_IF */
	int open_fd() override;
	void close_fd(int fd) override;
	void write_data(uint8_t byte) override;
	void handle_input(int fd) override;

	/* slip2tun */
	uint8_t *s2t_buffer = nullptr;
	size_t s2t_buffer_size = 0;
	size_t s2t_packet_size = 0;
	bool s2t_state_esc = false;
	void s2t_send_packet(void);
	void s2t(uint8_t byte);

	/* tun2slip */
	uint8_t *t2s_buffer = nullptr;
	size_t t2s_buffer_size = 0;
	void t2s(int fd);
};

#endif  // RISCV_VP_CHANNEL_SLIP_H
