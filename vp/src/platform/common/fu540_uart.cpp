#include "fu540_uart.h"

#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <mutex>
#include <queue>

FU540_UART::FU540_UART(sc_core::sc_module_name, Channel_IF *channel, uint32_t irq) : channel(channel), irq(irq) {
	tsock.register_b_transport(this, &FU540_UART::transport);

	router
	    .add_register_bank({
	        {TXDATA_REG_ADDR, &txdata},
	        {RXDATA_REG_ADDR, &rxdata},
	        {TXCTRL_REG_ADDR, &txctrl},
	        {RXCTRL_REG_ADDR, &rxctrl},
	        {IE_REG_ADDR, &ie},
	        {IP_REG_ADDR, &ip},
	        {DIV_REG_ADDR, &div},
	    })
	    .register_handler(this, &FU540_UART::register_access_callback);

	SC_METHOD(interrupt);
	sensitive << channel->asyncEvent;

	channel->start(UART_FIFO_DEPTH, UART_FIFO_DEPTH);

	dont_initialize();
}

FU540_UART::~FU540_UART(void) {}

void FU540_UART::register_access_callback(const vp::map::register_access_t &r) {
	if (r.read) {
		if (r.vptr == &txdata) {
			txdata = (channel->get_tx_fifo_size() >= UART_FIFO_DEPTH) ? UART_FULL : 0;
		} else if (r.vptr == &rxdata) {
			/* return value encoding does already match fu540 specification */
			rxdata = channel->rxpull();
		} else if (r.vptr == &txctrl) {
			// std::cout << "TXctl";
		} else if (r.vptr == &rxctrl) {
			// std::cout << "RXctrl";
		} else if (r.vptr == &ip) {
			uint32_t ret = 0;
			if (channel->get_tx_fifo_size() < UART_CTRL_CNT(txctrl)) {
				ret |= UART_TXWM;
			}
			if (channel->get_rx_fifo_size() > UART_CTRL_CNT(rxctrl)) {
				ret |= UART_RXWM;
			}
			ip = ret;
		} else if (r.vptr == &ie) {
			// do nothing
		} else if (r.vptr == &div) {
			// just return the last set value
		} else {
			std::cerr << "invalid offset for UART " << std::endl;
		}
	}

	bool notify = false;
	if (r.write) {
		if (r.vptr == &txctrl && UART_CTRL_CNT(r.nv) < UART_CTRL_CNT(txctrl)) {
			notify = true;
		} else if (r.vptr == &rxctrl && UART_CTRL_CNT(r.nv) < UART_CTRL_CNT(rxctrl)) {
			notify = true;
		}
	}

	r.fn();

	if (notify || (r.write && r.vptr == &ie)) {
		channel->asyncEvent.notify();
	}

	if (r.write && r.vptr == &txdata) {
		// from SoC to remote
		channel->txpush(txdata);
	}
}

void FU540_UART::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void FU540_UART::interrupt(void) {
	bool trigger = false;

	/* XXX: Possible optimization would be to trigger the
	 * interrupt from the background thread. However,
	 * the PLIC methods are very likely not thread safe. */

	if ((ie & UART_RXWM) && (channel->get_rx_fifo_size() > UART_CTRL_CNT(rxctrl))) {
		trigger = true;
	}

	if ((ie & UART_TXWM) && (channel->get_tx_fifo_size() < UART_CTRL_CNT(txctrl))) {
		trigger = true;
	}

	if (trigger) {
		plic->gateway_trigger_interrupt(irq);
	}
}
