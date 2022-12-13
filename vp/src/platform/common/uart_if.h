#pragma once

#include <stdint.h>
#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>

#include <systemc>
#include <tlm_utils/simple_target_socket.h>

#include <thread>
#include <mutex>
#include <queue>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"
#include "platform/common/async_event.h"

class UART_IF : public sc_core::sc_module {
public:
	typedef uint32_t Register;
	static constexpr Register UART_TXWM = 1 << 0;
	static constexpr Register UART_RXWM = 1 << 1;
	static constexpr Register UART_FULL = 1 << 31;

	/* 8-entry transmit and receive FIFO buffers */
	static constexpr unsigned UART_FIFO_DEPTH = 8;

	/* Extracts the interrupt trigger threshold from a control register */
	static constexpr Register UART_CTRL_CNT(Register REG){ return REG >> 16;};

	static constexpr uint8_t TXDATA_REG_ADDR = 0x0;
	static constexpr uint8_t RXDATA_REG_ADDR = 0x4;
	static constexpr uint8_t TXCTRL_REG_ADDR = 0x8;
	static constexpr uint8_t RXCTRL_REG_ADDR = 0xC;
	static constexpr uint8_t IE_REG_ADDR = 0x10;
	static constexpr uint8_t IP_REG_ADDR = 0x14;
	static constexpr uint8_t DIV_REG_ADDR = 0x18;

	interrupt_gateway *plic;
	tlm_utils::simple_target_socket<UART_IF> tsock;

	UART_IF(sc_core::sc_module_name, uint32_t irqsrc);
	virtual ~UART_IF(void);

	SC_HAS_PROCESS(UART_IF);	// interrupt

private:

	void register_access_callback(const vp::map::register_access_t &);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);
	void interrupt(void);

	uint32_t irq;

	// memory mapped configuration registers
	uint32_t txdata = 0;
	uint32_t rxdata = 0;
	uint32_t txctrl = 0;
	uint32_t rxctrl = 0;
	uint32_t ie = 0;
	uint32_t ip = 0;
	uint32_t div = 0;

	vp::map::LocalRouter router = {"UART"};

protected:
	std::queue<uint8_t> tx_fifo;
	sem_t txfull;
	std::queue<uint8_t> rx_fifo;
	sem_t rxempty;
	std::mutex rcvmtx, txmtx;
	AsyncEvent asyncEvent;

	void swait(sem_t *sem);
	void spost(sem_t *sem);

	// blocking push into SoC
	void rxpush(uint8_t data);
	// blocking pull from SoC to remote
	uint8_t txpull();
};
