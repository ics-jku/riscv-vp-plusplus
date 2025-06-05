#pragma once

#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <mutex>
#include <queue>
#include <systemc>
#include <thread>

#include "channel_if.h"
#include "core/common/irq_if.h"
#include "util/tlm_map.h"

class FU540_UART : public sc_core::sc_module {
   public:
	typedef uint32_t Register;
	static constexpr Register UART_TXWM = 1 << 0;
	static constexpr Register UART_RXWM = 1 << 1;
	static constexpr Register UART_FULL = 1 << 31;

	/* 8-entry transmit and receive FIFO buffers */
	static constexpr unsigned UART_FIFO_DEPTH = 8;

	/* Extracts the interrupt trigger threshold from a control register */
	static constexpr Register UART_CTRL_CNT(Register REG) {
		return REG >> 16;
	};

	static constexpr uint8_t TXDATA_REG_ADDR = 0x0;
	static constexpr uint8_t RXDATA_REG_ADDR = 0x4;
	static constexpr uint8_t TXCTRL_REG_ADDR = 0x8;
	static constexpr uint8_t RXCTRL_REG_ADDR = 0xC;
	static constexpr uint8_t IE_REG_ADDR = 0x10;
	static constexpr uint8_t IP_REG_ADDR = 0x14;
	static constexpr uint8_t DIV_REG_ADDR = 0x18;

	interrupt_gateway *plic = nullptr;
	tlm_utils::simple_target_socket<FU540_UART> tsock;

	FU540_UART(sc_core::sc_module_name, Channel_IF *channel, uint32_t irq);
	virtual ~FU540_UART(void);

	SC_HAS_PROCESS(FU540_UART);  // interrupt

   private:
	Channel_IF *channel;
	uint32_t irq;

	void register_access_callback(const vp::map::register_access_t &);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);
	void interrupt(void);

	// memory mapped configuration registers
	uint32_t txdata = 0;
	uint32_t rxdata = 0;
	uint32_t txctrl = 0;
	uint32_t rxctrl = 0;
	uint32_t ie = 0;
	uint32_t ip = 0;
	uint32_t div = 0;

	vp::map::LocalRouter router = {"UART"};
};
