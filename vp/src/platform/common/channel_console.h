#ifndef RISCV_VP_CHANNEL_CONSOLE_H
#define RISCV_VP_CHANNEL_CONSOLE_H

#include <channel_if.h>
#include <stdint.h>

class Channel_Console : public Channel_IF {
   public:
	void start(unsigned int tx_fifo_depth, unsigned int rx_fifo_depth) override;
	void stop() override;

   private:
	typedef enum {
		STATE_COMMAND,
		STATE_NORMAL,
	} uart_state;

	/**
	 * State of the input handling state machine. In normal mode
	 * (STATE_NORMAL) the next input character is forwarded to the
	 * guest. In command mode (STATE_COMMAND) the next input
	 * character is interpreted by ::handle_cmd.
	 */
	uart_state state = STATE_NORMAL;
	void handle_cmd(uint8_t);

	void handle_input(int fd) override;
	void write_data(uint8_t) override;
};

#endif  // RISCV_VP_CHANNEL_CONSOLE_H
