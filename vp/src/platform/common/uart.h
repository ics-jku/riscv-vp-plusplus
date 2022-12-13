#ifndef RISCV_VP_UART_H
#define RISCV_VP_UART_H

#include <stdint.h>
#include <fd_abstract_uart.h>
#include <systemc>

class UART : public FD_ABSTRACT_UART {
public:
	UART(const sc_core::sc_module_name&, uint32_t);
	virtual ~UART(void);

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

#endif  // RISCV_VP_UART_H
