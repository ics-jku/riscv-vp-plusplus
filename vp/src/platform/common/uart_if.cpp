#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <uart_if.h>
#include <unistd.h>
#include <mutex>
#include <queue>

UART_IF::UART_IF(sc_core::sc_module_name, uint32_t irqsrc) : plic(nullptr){
	irq = irqsrc;
	tsock.register_b_transport(this, &UART_IF::transport);

	router.add_register_bank({
		{TXDATA_REG_ADDR, &txdata},
		{RXDATA_REG_ADDR, &rxdata},
		{TXCTRL_REG_ADDR, &txctrl},
		{RXCTRL_REG_ADDR, &rxctrl},
		{IE_REG_ADDR, &ie},
		{IP_REG_ADDR, &ip},
		{DIV_REG_ADDR, &div},
	    })
	    .register_handler(this, &UART_IF::register_access_callback);

	if (sem_init(&txfull, 0, 0))
		throw std::system_error(errno, std::generic_category());
	if (sem_init(&rxempty, 0, UART_FIFO_DEPTH))
		throw std::system_error(errno, std::generic_category());

	SC_METHOD(interrupt);
	sensitive << asyncEvent;
	dont_initialize();
}

UART_IF::~UART_IF(void) {
	sem_destroy(&txfull);
	sem_destroy(&rxempty);
}

void UART_IF::rxpush(uint8_t data) {
	swait(&rxempty);
	rcvmtx.lock();
	rx_fifo.push(data);
	rcvmtx.unlock();
	asyncEvent.notify();
}

uint8_t UART_IF::txpull() {
	uint8_t data;
	swait(&txfull);
	if(tx_fifo.size() == 0) // Other thread will only increase count, not decrease
		return 0;
	txmtx.lock();
	data = tx_fifo.front();
	tx_fifo.pop();
	txmtx.unlock();
	return data;
}

void UART_IF::register_access_callback(const vp::map::register_access_t &r) {
	if (r.read) {
		if (r.vptr == &txdata) {
			txmtx.lock();
			txdata = (tx_fifo.size() >= UART_FIFO_DEPTH) ? UART_FULL : 0;
			txmtx.unlock();
		} else if (r.vptr == &rxdata) {
			rcvmtx.lock();
			if (rx_fifo.empty()) {
				rxdata = 1 << 31;
			} else {
				rxdata = rx_fifo.front();
				rx_fifo.pop();
				spost(&rxempty);
			}
			rcvmtx.unlock();
		} else if (r.vptr == &txctrl) {
			// std::cout << "TXctl";
		} else if (r.vptr == &rxctrl) {
			// std::cout << "RXctrl";
		} else if (r.vptr == &ip) {
			uint32_t ret = 0;
			txmtx.lock();
			if (tx_fifo.size() < UART_CTRL_CNT(txctrl)) {
				ret |= UART_TXWM;
			}
			txmtx.unlock();
			rcvmtx.lock();
			if (rx_fifo.size() > UART_CTRL_CNT(rxctrl)) {
				ret |= UART_RXWM;
			}
			rcvmtx.unlock();
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
		if (r.vptr == &txctrl && UART_CTRL_CNT(r.nv) < UART_CTRL_CNT(txctrl))
			notify = true;
		else if (r.vptr == &rxctrl && UART_CTRL_CNT(r.nv) < UART_CTRL_CNT(rxctrl))
			notify = true;
	}

	r.fn();

	if (notify || (r.write && r.vptr == &ie))
		asyncEvent.notify();

	if (r.write && r.vptr == &txdata) {
		// from SoC to remote
		txmtx.lock();
		if (tx_fifo.size() >= UART_FIFO_DEPTH) {
			txmtx.unlock();
			return; /* write is ignored */
		}

		tx_fifo.push(txdata);
		txmtx.unlock();
		spost(&txfull);
	}
}

void UART_IF::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void UART_IF::interrupt(void) {
	bool trigger = false;

	/* XXX: Possible optimization would be to trigger the
	 * interrupt from the background thread. However,
	 * the PLIC methods are very likely not thread safe. */

	if (ie & UART_RXWM) {
		rcvmtx.lock();
		if (rx_fifo.size() > UART_CTRL_CNT(rxctrl))
			trigger = true;
		rcvmtx.unlock();
	}

	if (ie & UART_TXWM) {
		txmtx.lock();
		if (tx_fifo.size() < UART_CTRL_CNT(txctrl))
			trigger = true;
		txmtx.unlock();
	}

	if (trigger)
		plic->gateway_trigger_interrupt(irq);
}

void UART_IF::swait(sem_t *sem) {
	if (sem_wait(sem))
		throw std::system_error(errno, std::generic_category());
}

void UART_IF::spost(sem_t *sem) {
	if (sem_post(sem))
		throw std::system_error(errno, std::generic_category());
}
