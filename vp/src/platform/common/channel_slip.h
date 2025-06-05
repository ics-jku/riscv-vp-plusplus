#ifndef RISCV_VP_CHANNEL_SLIP_H
#define RISCV_VP_CHANNEL_SLIP_H

#include <channel_if.h>
#include <stdint.h>

class Channel_SLIP : public Channel_IF {
   public:
	Channel_SLIP(std::string netdev) : netdev(netdev){};

	void start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) override;
	void stop() override;

   private:
	const std::string netdev;
	int get_mtu(const char *);
	void send_packet(void);
	void write_data(uint8_t) override;
	void handle_input(int fd) override;

	int tunfd;

	uint8_t *sndbuf = NULL, *rcvbuf = NULL;
	size_t sndsiz, rcvsiz;
};

#endif  // RISCV_VP_CHANNEL_SLIP_H
