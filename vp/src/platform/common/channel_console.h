#ifndef RISCV_VP_CHANNEL_CONSOLE_H
#define RISCV_VP_CHANNEL_CONSOLE_H

#include <stdint.h>

#include <set>

#include "channel_fd_if.h"
#include "core/common/debug.h"

class Channel_Console final : public Channel_FD_IF {
   public:
	Channel_Console(std::set<debug_target_if *> debug_targets = {}) : debug_targets(debug_targets){};
	virtual ~Channel_Console();

	void debug_targets_set(std::set<debug_target_if *> debug_targets) {
		this->debug_targets = debug_targets;
	}

	void debug_targets_add(debug_target_if *debug_target) {
		debug_targets.insert(debug_target);
	}

	void debug_targets_remove(debug_target_if *debug_target) {
		debug_targets.erase(debug_target);
	}

   private:
	typedef enum {
		STATE_COMMAND,
		STATE_NORMAL,
	} uart_state;

	bool trace_mode = false;
	std::set<debug_target_if *> debug_targets;

	void debug_targets_toggle_trace_mode(void);
	void debug_targets_print_stats(void);

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
