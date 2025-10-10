/*
 * Copyright (C) 2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 */

#include "ns16550a_uart.h"

#include <stdlib.h>
#include <unistd.h>

/* enable debug messages */
#undef DEBUG
// #define DEBUG

#define FORCE_DBG_PRINTF(...) fprintf(stderr, "NS16550A_UART: DEBUG: " __VA_ARGS__);
#ifdef DEBUG
#define DBG_PRINTF FORCE_DBG_PRINTF
#else
#define DBG_PRINTF(...)
#endif

NS16550A_UART::NS16550A_UART(sc_core::sc_module_name, Channel_IF *channel, uint32_t irq) : channel(channel), irq(irq) {
	tsock.register_b_transport(this, &NS16550A_UART::transport);

	mm_regs.pre_read_callback = std::bind(&NS16550A_UART::pre_read_regs, this, std::placeholders::_1);
	mm_regs.post_write_callback = std::bind(&NS16550A_UART::post_write_regs, this, std::placeholders::_1);

	/* 8 bit, 1 stop, no parity, not break, DLAB = 0 */
	regs[LCR_REG_ADDR] = UART_LCR_WLEN8;

	SC_METHOD(interrupt);
	sensitive << channel->asyncEvent;
	dont_initialize();

	channel->start(UART_TX_FIFO_DEPTH, UART_RX_FIFO_DEPTH);
}

NS16550A_UART::~NS16550A_UART(void) {}

bool NS16550A_UART::pre_read_regs(RegisterRange::ReadInfo t) {
	uint8_t val = 0;

	switch (t.addr) {
		case RX_TX_DLL_REG_ADDR:
			if ((regs[LCR_REG_ADDR] & UART_LCR_DLAB) == 0) {
				/* DLAB = 0 -> RX */
				uint32_t rxdata = channel->rxpull();
				if (rxdata & (1 << 31)) {
					DBG_PRINTF("pre_read 0x%lX RX: read from empty fifo!\n", t.addr);
				}
				/* ignore empty flag */
				val = rxdata & 0xff;
				DBG_PRINTF("pre_read 0x%lX RX 0x%X\n", t.addr, val);
			} else {
				/* DLAB = 1 -> DLL */
				val = reg_dll;
				DBG_PRINTF("pre_read 0x%lX DLL 0x%X\n", t.addr, val);
			}
			break;

		case IER_DLM_REG_ADDR:
			if ((regs[LCR_REG_ADDR] & UART_LCR_DLAB) == 0) {
				/* DLAB = 0 -> IER */
				val = reg_ier;
				DBG_PRINTF("pre_read 0x%lX IER 0x%X\n", t.addr, val);
			} else {
				/* DLAB = 1 -> DLM */
				val = reg_dlm;
				DBG_PRINTF("pre_read 0x%lX DLM 0x%X\n", t.addr, val);
			}
			break;

		case IIR_FCR_REG_ADDR:
			/* IIR */

			/* interrupts ordered according to priority (highest first) */
			if ((reg_ier & UART_IER_RDI) && (channel->get_rx_fifo_size() > 0)) {
				val = UART_IIR_RLSI;
			} else if ((reg_ier & UART_IER_THRI) && (channel->get_tx_fifo_size() == 0)) {
				val = UART_IIR_THRI;
			} else {
				val = UART_IIR_NO_INT;
			}

			if (reg_fcr & UART_FCR_ENABLE_FIFO) {
				val |= UART_IIR_FIFO_ENABLED_16550;
			}
			DBG_PRINTF("pre_read 0x%lX IIR 0x%X\n", t.addr, val);
			break;

		case LCR_REG_ADDR:
			/* LCR */
			DBG_PRINTF("pre_read 0x%lX LCR 0x%X\n", t.addr, regs[t.addr]);
			break;

		case MCR_REG_ADDR:
			/* MCR */
			DBG_PRINTF("pre_read 0x%lX MCR 0x%X\n", t.addr, regs[t.addr]);
			break;

		case LSR_REG_ADDR:
			/* LSR */

			/* no errors */
			val = 0;

			if (channel->get_tx_fifo_size() == 0) {
				/* tx fifo and shift register empty */
				val |= (UART_LSR_TEMT | UART_LSR_THRE);
			}

			/* Note: LSR_DR does never consider receiver_trigger_level (always set if rx fifo size > 0) */
			if (channel->get_rx_fifo_size() > 0) {
				/* rx data ready */
				val |= UART_LSR_DR;
			}

			DBG_PRINTF("pre_read 0x%lX LSR 0x%X\n", t.addr, val);
			break;

		case MSR_REG_ADDR:
			/* MSR */
			/* everything ready and clear to send (e.g. if flow control is used) */
			val = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
			DBG_PRINTF("pre_read 0x%lX MSR 0x%X\n", t.addr, val);
			break;

		case SCR_REG_ADDR:
			/* SCR */
			DBG_PRINTF("pre_read 0x%lX SCR 0x%X\n", t.addr, regs[t.addr]);
			break;

		default:
			std::cerr << "NS16650A_UART(" << name() << "): ERROR: invalid read on offset 0x" << std::hex << t.addr
			          << std::dec << std::endl;
			assert(0);
	}

	regs[t.addr] = val;
	return true;
}

void NS16550A_UART::post_write_regs(RegisterRange::WriteInfo t) {
	uint8_t val = *t.trans.get_data_ptr();

	switch (t.addr) {
		case RX_TX_DLL_REG_ADDR:
			if ((regs[LCR_REG_ADDR] & UART_LCR_DLAB) == 0) {
				/* DLAB = 0 -> TX */
				channel->txpush(val);
				DBG_PRINTF("post_write 0x%lX TX 0x%X\n", t.addr, val);
			} else {
				/* DLAB = 1 -> DLL */
				reg_dll = val;
				DBG_PRINTF("post_write 0x%lX DLL 0x%X\n", t.addr, val);
			}
			break;

		case IER_DLM_REG_ADDR:
			if ((regs[LCR_REG_ADDR] & UART_LCR_DLAB) == 0) {
				/* DLAB = 0 -> IER */
				reg_ier = val;
				DBG_PRINTF("post_write 0x%lX IER 0x%X\n", t.addr, val);
			} else {
				/* DLAB = 1 -> DLM */
				reg_dlm = val;
				DBG_PRINTF("post_write 0x%lX DLM 0x%X\n", t.addr, val);
			}
			/* interrupt config changed */
			channel->asyncEvent.notify();
			break;

		case IIR_FCR_REG_ADDR:
			/* FCR */

			DBG_PRINTF("post_write 0x%lX FCR 0x%X\n", t.addr, val);

			// TODO: write of other bis in fcr is only allowed if this bit is set
			if (val & UART_FCR_ENABLE_FIFO) {
				DBG_PRINTF(" - enable\n");
			} else {
				DBG_PRINTF(" - disable\n");
				// TODO -> clear rx/tx in channel */
			}

			if (val & UART_FCR_CLEAR_XMIT) {
				// TODO -> clear tx in channel
				DBG_PRINTF(" - clear tx\n");
			}

			if (val & UART_FCR_CLEAR_RCVR) {
				// TODO -> clear rx in channel
				DBG_PRINTF(" - clear rx\n");
			}

			switch (val & UART_FCR6_R_TRIGGER_MASK) {
				case UART_FCR6_R_TRIGGER_8:
					receiver_trigger_level = 8;
					break;
				case UART_FCR6_R_TRIGGER_16:
					receiver_trigger_level = 16;
					break;
				case UART_FCR6_R_TRIGGER_24:
					receiver_trigger_level = 24;
					break;
				case UART_FCR6_R_TRIGGER_28:
					receiver_trigger_level = 28;
					break;
				default:
					// must never happen!
					std::cerr << "NS16650A_UART(" << name() << "): ERROR: invalid receive trigger level: fcr = 0x"
					          << std::hex << val << std::dec << std::endl;
			}
			switch (val & UART_FCR6_T_TRIGGER_MASK) {
				case UART_FCR6_T_TRIGGER_16:
					transmitter_trigger_level = 16;
					break;
				case UART_FCR6_T_TRIGGER_8:
					transmitter_trigger_level = 8;
					break;
				case UART_FCR6_T_TRIGGER_24:
					transmitter_trigger_level = 24;
					break;
				default:
					std::cerr << "NS16650A_UART(" << name() << "): ERROR: invalid transmit trigger level: fcr = 0x"
					          << std::hex << val << std::dec << std::endl;
					/* set default */
					val &= ~UART_FCR6_T_TRIGGER_MASK;
					val |= UART_FCR6_T_TRIGGER_16;
					transmitter_trigger_level = 16;
			}
			DBG_PRINTF(" - trigger level: rx = %u, tx = %u\n", receiver_trigger_level, transmitter_trigger_level);

			reg_fcr = val;
			break;

		case LCR_REG_ADDR:
			/* LCR */
			DBG_PRINTF("post_write 0x%lX LCR 0x%X (DLAB=%i)\n", t.addr, val,
			           (regs[LCR_REG_ADDR] & UART_LCR_DLAB) ? 1 : 0);
			break;

		case MCR_REG_ADDR:
			/* MCR */
			/* modem control lines (DTR, RTS, ...) -> can be ignored */
			DBG_PRINTF("post_write 0x%lX MCR 0x%X\n", t.addr, val);
			if (val & UART_MCR_LOOP) {
				std::cerr << "NS16650A_UART(" << name() << "): WARNING: loopback mode not supported: mcr = 0x"
				          << std::hex << val << std::dec << " -> kept disabled" << std::endl;
				regs[MCR_REG_ADDR] &= ~UART_MCR_LOOP;
			}
			break;

		case LSR_REG_ADDR:
			/* LSR */
			DBG_PRINTF("post_write 0x%lX LSR 0x%X\n", t.addr, val);
			break;

		case MSR_REG_ADDR:
			/* MSR */
			DBG_PRINTF("post_write 0x%lX MSR 0x%X\n", t.addr, val);
			break;

		case SCR_REG_ADDR:
			/* SCR */
			/* scratch register -> can be ignored */
			DBG_PRINTF("post_write 0x%lX SCR 0x%X\n", t.addr, val);
			break;

		default:
			std::cerr << "NS16650A_UART(" << name() << "): ERROR: invalid write to offset 0x" << std::hex << t.addr
			          << " (value = 0x" << val << ")" << std::dec << std::endl;
			assert(0);
	}
}

void NS16550A_UART::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	vp::mm::route("NS16550A_UART", register_ranges, trans, delay);
}

void NS16550A_UART::interrupt(void) {
	bool trigger = false;

	if ((reg_ier & UART_IER_RDI) && (channel->get_rx_fifo_size() > 0)) {
		trigger = true;
	} else if ((reg_ier & UART_IER_THRI) && (channel->get_tx_fifo_size() == 0)) {
		trigger = true;
	}

	if (trigger) {
		DBG_PRINTF("interrupt trigger\n");
		plic->gateway_trigger_interrupt(irq);
	}
}
