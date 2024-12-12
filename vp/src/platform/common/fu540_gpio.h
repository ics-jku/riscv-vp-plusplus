#ifndef RISCV_VP_FU540_GPIO_H
#define RISCV_VP_FU540_GPIO_H

/*
 * gpio module for SiFive FU540
 */

#include <stdint.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "platform/common/gpio_if.h"
#include "util/tlm_map.h"

/* fu540 gpio with 16 gpios */
class FU540_GPIO : public sc_core::sc_module, public GPIO_IF {
   public:
	const int *interrupts = nullptr;
	interrupt_gateway *plic = nullptr;

	tlm_utils::simple_target_socket<FU540_GPIO> tsock;

	FU540_GPIO(const sc_core::sc_module_name &name, const int *interrupts);
	~FU540_GPIO(void);

	SC_HAS_PROCESS(FU540_GPIO);

	/*
	 * NOTE: Allows only in
	 *  * elaboration phase: BEFORE plic is set, and
	 *  * simulation phase: always
	 */
	void set_gpios(uint64_t set, uint64_t mask) override;
	uint64_t get_gpios() override {
		return gpio_val;
	}

   private:
	void trigger_interrupt(uint32_t gpio_nr);
	void update_gpios(uint32_t gpio_val_last);
	void update_gpios() {
		update_gpios(gpio_val);
	}

	void register_update_pending_callback(const vp::map::register_access_t &);
	void register_update_callback(const vp::map::register_access_t &);
	void register_update_default_callback(const vp::map::register_access_t &);
	void transport(tlm::tlm_generic_payload &, sc_core::sc_time &);

	uint32_t reg_input_val = 0;
	uint32_t reg_input_en = 0;
	uint32_t reg_output_en = 0;
	uint32_t reg_output_val = 0;
	uint32_t reg_pue = 0;
	uint32_t reg_ds = 0;
	uint32_t reg_rise_ie = 0;
	uint32_t reg_rise_ip = 0;
	uint32_t reg_fall_ie = 0;
	uint32_t reg_fall_ip = 0;
	uint32_t reg_high_ie = 0;
	uint32_t reg_high_ip = 0;
	uint32_t reg_low_ie = 0;
	uint32_t reg_low_ip = 0;
	uint32_t reg_out_xor = 0;

	uint32_t gpio_val = 0;

	vp::map::LocalRouter router = {"FU540_GPIO"};
};

#endif /* RISCV_VP_FU540_GPIO_H */
