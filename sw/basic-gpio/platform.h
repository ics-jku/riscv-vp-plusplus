#ifndef PLATFORM_H
#define PLATFORM_H

//#define MICRORV

#ifdef MICRORV
// LED
static volatile uint32_t *LED_ADDR = (uint32_t *)0x81000000;
// UART
static volatile uint32_t *UART_TX_DATA_ADDR = (uint32_t *)0x82000000;
static volatile uint32_t *UART_TX_CTRL_ADDR = (uint32_t *)0x82000004;
static volatile uint32_t *UART_RX_DATA_ADDR = (uint32_t *)0x82000008;
// CLIC
static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x02004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x0200bff8;
// GPIO
static volatile uint32_t *GPIO_BANK_A_DIRECTION_ADDR = (uint32_t *)0x83000000;
static volatile uint32_t *GPIO_BANK_A_OUTPUT_ADDR = (uint32_t *)0x83000004;
static volatile uint32_t *GPIO_BANK_A_INPUT_ADDR = (uint32_t *)0x83000008;
#else
// LED
static volatile uint32_t *LED_ADDR = (uint32_t *)0x81000000;
// UART
static volatile uint32_t *UART_TX_DATA_ADDR = (uint32_t *)0x82000000;
static volatile uint32_t *UART_TX_CTRL_ADDR = (uint32_t *)0x82000004;
static volatile uint32_t *UART_RX_DATA_ADDR = (uint32_t *)0x82000008;
// CLIC
static volatile uint64_t *MTIMECMP_REG = (uint64_t *)0x02004000;
static volatile uint64_t *MTIME_REG = (uint64_t *)0x0200bff8;
// GPIO
static volatile uint32_t *GPIO_BANK_A_DIRECTION_ADDR = (uint32_t *)0x83000000;
static volatile uint32_t *GPIO_BANK_A_OUTPUT_ADDR = (uint32_t *)0x83000004;
static volatile uint32_t *GPIO_BANK_A_INPUT_ADDR = (uint32_t *)0x83000008;
#endif

#endif /* PLATFORM_H */