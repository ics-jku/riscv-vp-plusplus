#include "stdint.h"

#include "irq.h"


static volatile char * const TERMINAL_ADDR = (char * const)0x20000000;

static volatile uint32_t * const DMA_SRC_ADDR  = (uint32_t * const)0x70000000;
static volatile uint32_t * const DMA_DST_ADDR  = (uint32_t * const)0x70000004;
static volatile uint32_t * const DMA_LEN_ADDR  = (uint32_t * const)0x70000008;
static volatile uint32_t * const DMA_OP_ADDR   = (uint32_t * const)0x7000000C;
static volatile uint32_t * const DMA_STAT_ADDR = (uint32_t * const)0x70000010;

static const uint32_t DMA_OP_NOP = 0;
static const uint32_t DMA_OP_MEMCPY = 1;


volatile _Bool dma_completed = 0;

void dma_irq_handler() {
	dma_completed = 1;
}

void init() {
	register_interrupt_handler(4, dma_irq_handler);
}

int main() {
	uint8_t src[32] = { [ 0 ... 31 ] = 70 };
	uint8_t dst[32] = { 0 };

	dma_completed = 0;
	*DMA_SRC_ADDR = (uint32_t)(&src[0]);
	*DMA_DST_ADDR = (uint32_t)(&dst[0]);
	*DMA_LEN_ADDR = sizeof(src);
	*DMA_OP_ADDR  = DMA_OP_MEMCPY;

	while (!dma_completed) {
		asm volatile ("wfi");
	}

	for (int i=0; i<32; ++i) {
		*TERMINAL_ADDR = dst[i];
	}
	*TERMINAL_ADDR = '\n';

	return 0;
}
