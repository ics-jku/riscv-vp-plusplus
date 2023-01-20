#include "irq.h"
#include "assert.h"


#define RISCV_MACHINE_SOFTWARE_INTERRUPT 3
#define RISCV_MACHINE_TIMER_INTERRUPT 7
#define RISCV_MACHINE_EXTERNAL_INTERRUPT 11


#define PLIC_BASE 0x40000000
#define IRQ_TABLE_NUM_ENTRIES 64
static volatile uint32_t * const PLIC_INTERRUPT_ENABLE_START = (uint32_t * const)(PLIC_BASE + 0x2000);
static volatile uint32_t * const PLIC_CLAIM_AND_RESPONSE_REGISTER = (uint32_t * const)(PLIC_BASE + 0x200004);


static void irq_empty_handler() {}
static irq_handler_t irq_handler_table[IRQ_TABLE_NUM_ENTRIES] = { [ 0 ... IRQ_TABLE_NUM_ENTRIES-1 ] = irq_empty_handler };


#define CLINT_BASE 0x2000000

volatile uint64_t* mtime =   (uint64_t*)(CLINT_BASE + 0xbff8);
volatile uint64_t* mtimecmp = (uint64_t*)(CLINT_BASE + 0x4000);

static irq_handler_t timer_irq_handler = 0;


void level_1_interrupt_handler(uint32_t cause) {

	switch (cause & 0xf) {
		case RISCV_MACHINE_EXTERNAL_INTERRUPT: {
			asm volatile ("csrc mip, %0" : : "r" (0x800));

			uint32_t irq_id = *PLIC_CLAIM_AND_RESPONSE_REGISTER;

			irq_handler_table[irq_id]();

			*PLIC_CLAIM_AND_RESPONSE_REGISTER = 1;

			return;
		}

		case RISCV_MACHINE_TIMER_INTERRUPT: {
			// Note: the pending timer interrupt bit will be automatically cleared when writing to the *mtimecmp* register of this hart
			if (timer_irq_handler) {
				// let the user registered handler clear the timer interrupt bit
				timer_irq_handler();
			} else {
				// reset the *mtimecmp* register to zero to clear the pending bit
				*mtimecmp = 0;
			}

			return;
		}
	}

	assert (0 && "unsupported cause");
}

void register_interrupt_handler(uint32_t irq_id, irq_handler_t fn) {
	assert (irq_id < IRQ_TABLE_NUM_ENTRIES);
	// enable interrupt
	volatile uint32_t* const reg = (PLIC_INTERRUPT_ENABLE_START + irq_id/32);
	*reg |= 1 << (irq_id%32);
	// set a prio different to zero (which means do-not-interrupt)
	*((uint32_t*) (PLIC_BASE + irq_id*sizeof(uint32_t))) = 1;
	irq_handler_table[irq_id] = fn;
}

void register_timer_interrupt_handler(irq_handler_t fn) {
	timer_irq_handler = fn;
}
