#ifndef RISCV_VP_CHANNEL_CONSOLE_H
#define RISCV_VP_CHANNEL_CONSOLE_H

#include <stdint.h>

#include <atomic>
#include <set>
#include <systemc>

#include "channel_fd_if.h"
#include "core/common/debug.h"

class Channel_Console final : public sc_core::sc_module, public Channel_FD_IF {
   public:
	Channel_Console(sc_core::sc_module_name = "console", std::set<debug_target_if *> debug_targets = {});
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
	bool debug_targets_datadmi_is_enabled(void);
	void debug_targets_toggle_datadmi(void);
	bool debug_targets_dbbcache_is_enabled(void);
	void debug_targets_toggle_dbbcache(void);
	bool debug_targets_lscache_is_enabled(void);
	void debug_targets_toggle_lscache(void);
	void debug_targets_print_stats(void);

	/**
	 * State of the input handling state machine. In normal mode
	 * (STATE_NORMAL) the next input character is forwarded to the
	 * guest. In command mode (STATE_COMMAND) the next input
	 * character is interpreted by ::handle_cmd.
	 */
	uart_state state = STATE_NORMAL;

	std::atomic<std::uint8_t> cmd_requested;
	AsyncEvent cmd_asyncEvent;
	void trigger_handle_cmd(uint8_t);
	void handle_cmd();

	int open_fd() override;
	void close_fd(int fd) override;
	void handle_input(int fd) override;
	void write_data(uint8_t) override;
};

#endif  // RISCV_VP_CHANNEL_CONSOLE_H
