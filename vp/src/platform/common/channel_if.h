#ifndef RISCV_VP_CHANNEL_IF_H
#define RISCV_VP_CHANNEL_IF_H

#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

#include <mutex>
#include <queue>

#include "platform/common/async_event.h"

/*
 * Defines a base communication channel interface (e.g. for uarts) and provides
 * basic rx/tx fifo handling.
 */
class Channel_IF {
   public:
	virtual ~Channel_IF(){};

	void start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth);
	void stop();

	AsyncEvent asyncEvent;

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

	// blocking pull from SoC to remote
	uint8_t txpull();
	// blocking push into SoC
	void rxpush(uint8_t data);

	void post_txfull() {
		spost(&txfull);
	}

	void post_rxempty() {
		spost(&rxempty);
	}

   private:
	virtual void start_handling() = 0;
	virtual void stop_handling() = 0;

	unsigned int rx_fifo_depth = 1;
	unsigned int tx_fifo_depth = 1;

	std::queue<uint8_t> tx_fifo;
	sem_t txfull;
	std::queue<uint8_t> rx_fifo;
	sem_t rxempty;
	std::mutex rcvmtx, txmtx;

	void swait(sem_t *sem);
	void spost(sem_t *sem);
};

#endif  // RISCV_VP_CHANNEL_IF_H
