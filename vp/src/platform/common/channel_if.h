#ifndef RISCV_VP_CHANNEL_IF_H
#define RISCV_VP_CHANNEL_IF_H

#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

#include <mutex>
#include <queue>
#include <thread>

#include "platform/common/async_event.h"

class Channel_IF {
   public:
	Channel_IF();
	virtual ~Channel_IF(void);

	AsyncEvent asyncEvent;  // TODO

	unsigned int get_tx_fifo_depth() {
		return tx_fifo_depth;
	}

	unsigned int get_rx_fifo_depth() {
		return rx_fifo_depth;
	}

	unsigned int get_tx_fifo_size();
	unsigned int get_rx_fifo_size();

	/*
	 * get char from rx fifo (non-blocking)
	 * remote -> SoC
	 * return:
	 *  bit 31 = 1 -> empty (no char)
	 *  bit 31 = 0 -> char in bits [7:0]
	 */
	unsigned int rxpull();

	/*
	 * push char to tx fifo (non-blocking)
	 * SoC -> remote
	 * return:
	 *  true: success
	 *  false: fifo full
	 */
	bool txpush(uint8_t txdata);

	virtual void start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) = 0;
	virtual void stop() = 0;

   protected:
	void start_threads(int fd, unsigned int tx_fifo_depth = 1, unsigned int rx_fifo_depth = 1, bool write_only = false);
	void stop_threads(void);

	// blocking pull from SoC to remote
	uint8_t txpull();
	// blocking push into SoC
	void rxpush(uint8_t data);

   private:
	unsigned int rx_fifo_depth = 1;
	unsigned int tx_fifo_depth = 1;

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

	std::queue<uint8_t> tx_fifo;
	sem_t txfull;
	std::queue<uint8_t> rx_fifo;
	sem_t rxempty;
	std::mutex rcvmtx, txmtx;

	void swait(sem_t *sem);
	void spost(sem_t *sem);
};

#endif  // RISCV_VP_CHANNEL_IF_H
