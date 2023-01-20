#ifndef PLATFORM_H
#define PLATFORM_H

/* #define MICRORV

// #ifdef MICRORV */
// LED
static volatile uint32_t *LED_ADDR = (uint32_t *)0x81000000;
// UART
static volatile uint32_t *UART_TX_DATA_ADDR = (uint32_t *)0x82000000;
static volatile uint32_t *UART_TX_CTRL_ADDR = (uint32_t *)0x82000004;
static volatile uint32_t *UART_RX_DATA_ADDR = (uint32_t *)0x82000008;
// CLIC
static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x02004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x0200bff8;

typedef struct {
	volatile uint32_t TXDATA;
	volatile uint32_t TXCTRL;
	volatile uint32_t RXDATA;
	volatile uint32_t RXOCCU;
	volatile uint32_t RXALEM;
	volatile uint32_t RXEMPT;
} UART_REGS;

#define UART        ((UART_REGS *)0x82000000)

/* #else
// // UART
// static volatile uint32_t *UART_TX_DATA_ADDR = (uint32_t *)0x82000000;
// static volatile uint32_t *UART_TX_CTRL_ADDR = (uint32_t *)0x82000004;
// static volatile uint32_t *UART_RX_DATA_ADDR = (uint32_t *)0x82000008;
// static volatile uint32_t *UART_RX_FIFO_OCC_ADDR = (uint32_t *)0x8200000c;
// static volatile uint32_t *UART_RX_FIFO_ALMOST_EMPTY_ADDR = (uint32_t *)0x82000010;
// static volatile uint32_t *UART_RX_FIFO_EMPTY_ADDR = (uint32_t *)0x82000014;

// // CLIC
// static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x02004000;
// static volatile uint64_t *MTIME_REG = (uint64_t *)0x0200bff8;
// #endif */

#endif /* PLATFORM_H */