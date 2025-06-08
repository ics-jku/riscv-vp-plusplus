#ifndef RISCV_VP_CHANNEL_CONSOLE_H
#define RISCV_VP_CHANNEL_CONSOLE_H

#include <stdint.h>

#include "channel_fd_if.h"

class Channel_Console final : public Channel_FD_IF {
   public:
	virtual ~Channel_Console();

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

	int open_fd() override;
	void close_fd(int fd) override;
	void handle_input(int fd) override;
	void write_data(uint8_t) override;
};

#endif  // RISCV_VP_CHANNEL_CONSOLE_H
