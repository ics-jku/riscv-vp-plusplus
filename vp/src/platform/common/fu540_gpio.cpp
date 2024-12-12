#include "fu540_gpio.h"

#include <bitset>

#define FU540_N_GPIOS 16

enum {
	// Pin value
	REG_INPUT_VAL = 0x00,
	// Pin input enable*
	REG_INPUT_EN = 0x04,
	// Pin output enable*
	REG_OUTPUT_EN = 0x08,
	// Output value
	REG_OUTPUT_VAL = 0x0C,
	// Internal pull-up enable*
	REG_PUE = 0x10,
	// Pin drive strength
	REG_DS = 0x14,
	// Rise interrupt enable
	REG_RISE_IE = 0x18,
	// Rise interrupt pending
	REG_RISE_IP = 0x1C,
	// Fall interrupt enable
	REG_FALL_IE = 0x20,
	// Fall interrupt pending
	REG_FALL_IP = 0x24,
	// High interrupt enable
	REG_HIGH_IE = 0x28,
	// High interrupt pending
	REG_HIGH_IP = 0x2C,
	// Low interrupt enable
	REG_LOW_IE = 0x30,
	// Low interrupt pending
	REG_LOW_IP = 0x34,
	// Output XOR (invert)
	REG_OUT_XOR = 0x40,
};

FU540_GPIO::FU540_GPIO(const sc_core::sc_module_name &, const int *interrupts)
    : GPIO_IF(FU540_N_GPIOS), interrupts(interrupts) {
	tsock.register_b_transport(this, &FU540_GPIO::transport);
	router
	    .add_register_bank({
	        {REG_RISE_IP, &reg_rise_ip},
	        {REG_FALL_IP, &reg_fall_ip},
	        {REG_HIGH_IP, &reg_high_ip},
	        {REG_LOW_IP, &reg_low_ip},
	    })
	    .register_handler(this, &FU540_GPIO::register_update_pending_callback);
	router
	    .add_register_bank({
	        {REG_INPUT_EN, &reg_input_en},
	        {REG_OUTPUT_EN, &reg_output_en},
	        {REG_OUTPUT_VAL, &reg_output_val},
	        {REG_RISE_IE, &reg_rise_ie},
	        {REG_FALL_IE, &reg_fall_ie},
	        {REG_HIGH_IE, &reg_high_ie},
	        {REG_LOW_IE, &reg_low_ie},
	        {REG_OUT_XOR, &reg_out_xor},
	    })
	    .register_handler(this, &FU540_GPIO::register_update_callback);
	router
	    .add_register_bank({
	        {REG_INPUT_VAL, &reg_input_val},
	        {REG_PUE, &reg_pue},
	        {REG_DS, &reg_ds},
	    })
	    .register_handler(this, &FU540_GPIO::register_update_default_callback);
}

FU540_GPIO::~FU540_GPIO(void) {}

void FU540_GPIO::set_gpios(uint64_t set, uint64_t mask) {
	uint32_t gpio_val_last = gpio_val;

	for (unsigned int i = 0; i < N_GPIOS; i++) {
		uint32_t im = (1 << i);
		if (im & mask) {
			if (im & set) {
				gpio_val |= im;
			} else {
				gpio_val &= ~im;
			}
		}
	}

	update_gpios(gpio_val_last);
}

void FU540_GPIO::trigger_interrupt(uint32_t gpio_nr) {
	if (plic == nullptr || interrupts == nullptr) {
		return;
	}
	plic->gateway_trigger_interrupt(interrupts[gpio_nr]);
}

void FU540_GPIO::update_gpios(uint32_t gpio_val_last) {
	bool trigger_interrupt = false;

	/* handle invert */
	uint32_t val = reg_output_val ^ reg_out_xor;

	for (unsigned int i = 0; i < N_GPIOS; i++) {
		uint32_t im = (1 << i);

		/* update enabled outputs */
		if (im & reg_output_en) {
			if (val & im) {
				gpio_val |= im;
			} else {
				gpio_val &= ~im;
			}
		}

		// TODO: probably wrong -> ip set even if ie is not? */

		/* handle level interrupts */
		if (im & reg_high_ie & gpio_val) {
			reg_high_ip |= im;
			trigger_interrupt = true;
		}
		if (im & reg_low_ie & ~gpio_val) {
			reg_low_ip |= im;
			trigger_interrupt = true;
		}

		/* handle edge interrupts */
		if ((reg_rise_ie & im) && ((gpio_val_last & im) == 0) && ((gpio_val & im) == 1)) {
			reg_rise_ip |= im;
			trigger_interrupt = true;
		}
		if ((reg_fall_ie & im) && ((gpio_val_last & im) == 1) && ((gpio_val & im) == 0)) {
			reg_fall_ip |= im;
			trigger_interrupt = true;
		}

		if (trigger_interrupt) {
			this->trigger_interrupt(i);
		}
	}

	/* debug output */
	// std::cout << "FU540_GPIO: gpios = " << std::bitset<FU540_N_GPIOS>(gpio_val) << std::endl;

	/* update input */
	reg_input_val = gpio_val & reg_input_en;
}

void FU540_GPIO::register_update_pending_callback(const vp::map::register_access_t &r) {
	if (r.write) {
		/* FU540-C000-V1.0-1.pdf
		 * pending bit is reset to 0, if 1 is written
		 */

		/* get the current value, we want to modify */
		uint32_t cur_pending = *r.vptr;
		/* temporarily write the value (mask), just to get it afterwards */
		r.fn();
		uint32_t reset_mask = *r.vptr;

		/* set the real new value
		 * e.g.
		 * cur_pending: 0b1101
		 * reset_mask:  0b0001
		 * result:      0b1100
		 */
		*r.vptr = cur_pending & ~reset_mask;

	} else {
		r.fn();
	}
}

void FU540_GPIO::register_update_callback(const vp::map::register_access_t &r) {
	r.fn();
	update_gpios();
}

void FU540_GPIO::register_update_default_callback(const vp::map::register_access_t &r) {
	r.fn();
}

void FU540_GPIO::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}
