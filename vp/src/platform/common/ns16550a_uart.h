/*
 * Copyright (C) 2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Functional model of a NS16550A UART
 *
 * Important Notes/TODO:
 *  * The model currently does not implement the FIFO threshold (fcr) + timeout mechanism used in FIFO mode. It always
 *    generates receiver/transmitter interrupts as soon rx is available or tx is empty (similar as in non-fifo mode).
 *    -> The implemented behavior is still fully compatible with the FIFO-mode, but it will lead to higher interrupt
 *       load on executed software
 *  * Minimal implementation for modem control lines (output is ignored; all inputs are set "clear to send")
 *  * Loopback mode (mcr bit 4) is not implemented yet (triggers warning; happens on FreeBSD but does no harm)
 *  * RX/TX FIFO clear not supported yet
 *  * Write protection of FCR bits in case of !UART_FCR_ENABLE_FIFO not implemented yet
 *  * Baudrate/Timing not implemented yet
 *  * Break not implemented yet
 *  * DMA not implemented yet
 */
#ifndef RISCV_VP_NS16650_UART_H
#define RISCV_VP_NS16650_UART_H

#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "channel_if.h"
#include "core/common/irq_if.h"
#include "util/memory_map.h"

class NS16550A_UART : public sc_core::sc_module {
   public:
	/* FIFO Depths (use maximum configuration for ns16550a -> see fcr register) */
	static constexpr unsigned UART_RX_FIFO_DEPTH = 28;
	static constexpr unsigned UART_TX_FIFO_DEPTH = 24;

	/* Register Addresses */
	static constexpr uint8_t RX_TX_DLL_REG_ADDR = 0x0;
	static constexpr uint8_t IER_DLM_REG_ADDR = 0x1;
	static constexpr uint8_t IIR_FCR_REG_ADDR = 0x2;
	static constexpr uint8_t LCR_REG_ADDR = 0x3;
	static constexpr uint8_t MCR_REG_ADDR = 0x4;
	static constexpr uint8_t LSR_REG_ADDR = 0x5;
	static constexpr uint8_t MSR_REG_ADDR = 0x6;
	static constexpr uint8_t SCR_REG_ADDR = 0x7;

	/* 0x1(RW) - IER - Interrupt Enable Register */
	static constexpr uint8_t UART_IER_MSI = 0x08;  /* Enable Modem status interrupt */
	static constexpr uint8_t UART_IER_RLSI = 0x04; /* Enable receiver line status interrupt */
	static constexpr uint8_t UART_IER_THRI = 0x02; /* Enable Transmitter holding register int. */
	static constexpr uint8_t UART_IER_RDI = 0x01;  /* Enable receiver data interrupt */

	/* 0x2(READ) - IIR - Interrupt ID Register */
	static constexpr uint8_t UART_IIR_NO_INT = 0x01;             /* No interrupts pending */
	static constexpr uint8_t UART_IIR_ID = 0x0e;                 /* Mask for the interrupt ID */
	static constexpr uint8_t UART_IIR_MSI = 0x00;                /* Modem status interrupt */
	static constexpr uint8_t UART_IIR_THRI = 0x02;               /* Transmitter holding register empty */
	static constexpr uint8_t UART_IIR_RDI = 0x04;                /* Receiver data interrupt */
	static constexpr uint8_t UART_IIR_RLSI = 0x06;               /* Receiver line status interrupt */
	static constexpr uint8_t UART_IIR_FIFO_ENABLED_16550 = 0xc0; /* 16550A: FIFO enabled */

	/* 0x2(WRITE) - FCR - FIFO Control Register */
	static constexpr uint8_t UART_FCR_DMA_SELECT = 0x08;      /* For DMA applications */
	static constexpr uint8_t UART_FCR_CLEAR_XMIT = 0x04;      /* Clear the XMIT FIFO */
	static constexpr uint8_t UART_FCR_CLEAR_RCVR = 0x02;      /* Clear the RCVR FIFO */
	static constexpr uint8_t UART_FCR_ENABLE_FIFO = 0x01;     /* Enable the FIFO */
	static constexpr uint8_t UART_FCR6_R_TRIGGER_MASK = 0xC0; /* Mask for receive trigger */
	static constexpr uint8_t UART_FCR6_R_TRIGGER_8 = 0x00;    /* Receive trigger set at 1 */
	static constexpr uint8_t UART_FCR6_R_TRIGGER_16 = 0x40;   /* Receive trigger set at 4 */
	static constexpr uint8_t UART_FCR6_R_TRIGGER_24 = 0x80;   /* Receive trigger set at 8 */
	static constexpr uint8_t UART_FCR6_R_TRIGGER_28 = 0xC0;   /* Receive trigger set at 14 */
	static constexpr uint8_t UART_FCR6_T_TRIGGER_MASK = 0x30; /* Mask for transmit trigger */
	static constexpr uint8_t UART_FCR6_T_TRIGGER_16 = 0x00;   /* Transmit trigger set at 16 */
	static constexpr uint8_t UART_FCR6_T_TRIGGER_8 = 0x10;    /* Transmit trigger set at 8 */
	static constexpr uint8_t UART_FCR6_T_TRIGGER_24 = 0x20;   /* Transmit trigger set at 24 */

	/* 0x3(RW) - LCR - Line Control Register */
	static constexpr uint8_t UART_LCR_DLAB = 0x80;   /* Divisor latch access bit */
	static constexpr uint8_t UART_LCR_SBC = 0x40;    /* Set break control */
	static constexpr uint8_t UART_LCR_SPAR = 0x20;   /* Stick parity (?) */
	static constexpr uint8_t UART_LCR_EPAR = 0x10;   /* Even parity select */
	static constexpr uint8_t UART_LCR_PARITY = 0x08; /* Parity Enable */
	static constexpr uint8_t UART_LCR_STOP = 0x04;   /* Stop bits: 0=1 bit, 1=2 bits */
	static constexpr uint8_t UART_LCR_WLEN5 = 0x00;  /* Wordlength: 5 bits */
	static constexpr uint8_t UART_LCR_WLEN6 = 0x01;  /* Wordlength: 6 bits */
	static constexpr uint8_t UART_LCR_WLEN7 = 0x02;  /* Wordlength: 7 bits */
	static constexpr uint8_t UART_LCR_WLEN8 = 0x03;  /* Wordlength: 8 bits */

	/* 0x4(RW) - MCR - Modem Control Register */
	static constexpr uint8_t UART_MCR_CLKSEL = 0x80; /* Divide clock by 4 (TI16C752, EFR[4]=1) */
	static constexpr uint8_t UART_MCR_TCRTLR = 0x40; /* Access TCR/TLR (TI16C752, EFR[4]=1) */
	static constexpr uint8_t UART_MCR_XONANY = 0x20; /* Enable Xon Any (TI16C752, EFR[4]=1) */
	static constexpr uint8_t UART_MCR_AFE = 0x20;    /* Enable auto-RTS/CTS (TI16C550C/TI16C750) */
	static constexpr uint8_t UART_MCR_LOOP = 0x10;   /* Enable loopback test mode */
	static constexpr uint8_t UART_MCR_OUT2 = 0x08;   /* Out2 complement */
	static constexpr uint8_t UART_MCR_OUT1 = 0x04;   /* Out1 complement */
	static constexpr uint8_t UART_MCR_RTS = 0x02;    /* RTS complement */
	static constexpr uint8_t UART_MCR_DTR = 0x01;    /* DTR complement */

	/* 0x5(RW) - LSR - Line Status Register */
	static constexpr uint8_t UART_LSR_TEMT = 0x40; /* Transmitter empty */
	static constexpr uint8_t UART_LSR_THRE = 0x20; /* Transmit-hold-register empty */
	static constexpr uint8_t UART_LSR_BI = 0x10;   /* Break interrupt indicator */
	static constexpr uint8_t UART_LSR_FE = 0x08;   /* Frame error indicator */
	static constexpr uint8_t UART_LSR_PE = 0x04;   /* Parity error indicator */
	static constexpr uint8_t UART_LSR_OE = 0x02;   /* Overrun error indicator */
	static constexpr uint8_t UART_LSR_DR = 0x01;   /* Receiver data ready */

	/* 0x6(RW) - MSR - Modem Status Register */
	static constexpr uint8_t UART_MSR_DCD = 0x80;  /* Data Carrier Detect */
	static constexpr uint8_t UART_MSR_RI = 0x40;   /* Ring Indicator */
	static constexpr uint8_t UART_MSR_DSR = 0x20;  /* Data Set Ready */
	static constexpr uint8_t UART_MSR_CTS = 0x10;  /* Clear to Send */
	static constexpr uint8_t UART_MSR_DDCD = 0x08; /* Delta DCD */
	static constexpr uint8_t UART_MSR_TERI = 0x04; /* Trailing edge ring indicator */
	static constexpr uint8_t UART_MSR_DDSR = 0x02; /* Delta DSR */
	static constexpr uint8_t UART_MSR_DCTS = 0x01; /* Delta CTS */

	interrupt_gateway *plic = nullptr;
	tlm_utils::simple_target_socket<NS16550A_UART> tsock;

	NS16550A_UART(sc_core::sc_module_name, Channel_IF *channel, uint32_t irq);
	virtual ~NS16550A_UART(void);

	SC_HAS_PROCESS(NS16550A_UART);  // interrupt

   private:
	Channel_IF *channel;
	uint32_t irq;

	RegisterRange mm_regs{0x0, 8};
	ArrayView<uint8_t> regs{mm_regs};
	std::vector<RegisterRange *> register_ranges{&mm_regs};
	/* registers switched by Read/Write and DLAB=0/1 */
	uint8_t reg_ier = 0;
	uint8_t reg_fcr = 0;
	uint8_t reg_dll = 0;
	uint8_t reg_dlm = 0;

	/*
	 * Trigger Levels (set via FCR)
	 * (default values corresponding fcr bits = 0)
	 * currently not used -> see Important Notes above
	 */
	unsigned int receiver_trigger_level = 8;
	unsigned int transmitter_trigger_level = 16;

	bool pre_read_regs(RegisterRange::ReadInfo t);
	void post_write_regs(RegisterRange::WriteInfo t);

	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);
	void interrupt(void);
};

#endif /* RISCV_VP_NS16650_UART_H */
