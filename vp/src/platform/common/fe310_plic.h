#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "util/memory_map.h"
#include "util/propertymap.h"
#include "util/tlm_map.h"

static constexpr bool trace_mode = false;

/**
 * This class is supposed to implement the PLIC defined in Chapter 10
 * of the FE310-G000 manual. Currently, it still has various differences
 * from the version documented in the manual.
 */
template <unsigned NumberCores, unsigned NumberInterrupts, unsigned NumberInterruptEntries, uint32_t MaxPriority>
struct FE310_PLIC : public sc_core::sc_module, public interrupt_gateway {
	static_assert(NumberInterrupts <= 4096, "out of bound");
	static_assert(NumberCores <= 15360, "out of bound");
	static constexpr unsigned WORDS_FOR_INTERRUPT_ENTRIES = NumberInterruptEntries;
	// this does not work for the snake example
	// static constexpr unsigned WORDS_FOR_INTERRUPT_ENTRIES = (NumberInterruptEntries+(32))/32;

	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_access_clock_cycles = 4;
	unsigned int prop_irq_trigger_clock_cycles = 1;

	sc_core::sc_time access_delay;
	sc_core::sc_time irq_trigger_delay;

	tlm_utils::simple_target_socket<FE310_PLIC> tsock;

	std::array<external_interrupt_target *, NumberCores> target_harts{};

	// shared for all harts priority 1 is the lowest. Zero means do not interrupt
	// NOTE: addressing starts at 0x4 because interrupt 0 is reserved, however some example SW still writes to address
	// 0x0, hence we added it to the address map
	RegisterRange regs_interrupt_priorities{0x0, sizeof(uint32_t) * (NumberInterrupts + 1)};
	ArrayView<uint32_t> interrupt_priorities{regs_interrupt_priorities};

	RegisterRange regs_pending_interrupts{0x1000, sizeof(uint32_t) * WORDS_FOR_INTERRUPT_ENTRIES};
	ArrayView<uint32_t> pending_interrupts{regs_pending_interrupts};

	struct HartConfig {
		uint32_t priority_threshold;
		uint32_t claim_response;
	};
	RegisterRange regs_hart_enabled_interrupts{0x2000, sizeof(uint32_t) * WORDS_FOR_INTERRUPT_ENTRIES *NumberCores};
	ArrayView<uint32_t> hart_enabled_interrupts{regs_hart_enabled_interrupts};

	RegisterRange regs_hart_config{0x200000, sizeof(HartConfig) * NumberCores};
	ArrayView<HartConfig> hart_config{regs_hart_config};

	std::vector<RegisterRange *> register_ranges{&regs_interrupt_priorities, &regs_pending_interrupts,
	                                             &regs_hart_enabled_interrupts, &regs_hart_config};

	PrivilegeLevel irq_level;
	std::array<bool, NumberCores> hart_eip{};

	sc_core::sc_event e_run;

	SC_HAS_PROCESS(FE310_PLIC);

	FE310_PLIC(sc_core::sc_module_name, PrivilegeLevel level = MachineMode) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("FE310_PLIC." + name(), "clock_cycle_period", sc_time, prop_clock_cycle_period);
		VPPP_PROPERTY_GET("FE310_PLIC." + name(), "access_clock_cycles", uint64, prop_access_clock_cycles);
		VPPP_PROPERTY_GET("FE310_PLIC." + name(), "irq_trigger_clock_cycles", uint64, prop_irq_trigger_clock_cycles);

		access_delay = prop_access_clock_cycles * prop_clock_cycle_period;
		irq_trigger_delay = prop_irq_trigger_clock_cycles * prop_clock_cycle_period;

		tsock.register_b_transport(this, &FE310_PLIC::transport);

		regs_pending_interrupts.readonly = true;
		regs_hart_config.alignment = 4;

		regs_interrupt_priorities.post_write_callback =
		    std::bind(&FE310_PLIC::post_write_interrupt_priorities, this, std::placeholders::_1);
		regs_hart_config.post_write_callback =
		    std::bind(&FE310_PLIC::post_write_hart_config, this, std::placeholders::_1);
		regs_hart_config.pre_read_callback = std::bind(&FE310_PLIC::pre_read_hart_config, this, std::placeholders::_1);

		if (trace_mode) {
			regs_hart_enabled_interrupts.post_write_callback = [this](RegisterRange::WriteInfo t) {
				std::cout << "[vp::plic] Wrote enabled_interrupts at offs +" << std::dec << t.addr << " value 0x"
				          << std::hex << *reinterpret_cast<uint32_t *>(t.trans.get_data_ptr()) << std::dec << std::endl;
				for (unsigned n = 0; n < NumberCores; ++n) {
					for (unsigned i = 0; i < WORDS_FOR_INTERRUPT_ENTRIES; ++i) {
						const uint32_t itr_group = hart_enabled_interrupts(n, i);
						if (itr_group) {
							for (unsigned b = 0; b < 32; b++) {
								if ((1 << b) & itr_group) {
									std::cout << "[vp::plic]\t Hart " << n << " ITR " << i * 32 + b << " enabled."
									          << std::dec << std::endl;
								}
							}
						}
					}
				}
			};
		}

		for (unsigned i = 0; i < NumberInterrupts; ++i) {
			interrupt_priorities[i] = 0;
		}

		for (unsigned n = 0; n < NumberCores; ++n) {
			target_harts[n] = nullptr;
			hart_eip[n] = false;
			for (unsigned i = 0; i < WORDS_FOR_INTERRUPT_ENTRIES; ++i) {
				hart_enabled_interrupts(n, i) = 0;  // all interrupts disabled by default
			}
		}

		irq_level = level;
		SC_THREAD(run);
	}

	void gateway_trigger_interrupt(uint32_t irq_id) {
		// NOTE: can use different techniques for each gateway, in this case a
		// simple non queued edge trigger
		assert(irq_id > 0 && irq_id < NumberInterrupts);
		// std::cout << "[vp::plic] incoming interrupt " << irq_id << std::endl;

		unsigned idx = irq_id / 32;
		unsigned off = irq_id % 32;

		pending_interrupts[idx] |= 1 << off;

		e_run.notify(irq_trigger_delay);
	}

	void clear_pending_interrupt(unsigned irq_id) {
		assert(irq_id < NumberInterrupts);  // NOTE: ignore clear of zero interrupt (zero is not available)
		if (trace_mode)
			std::cout << "[vp::plic] clear pending interrupt " << irq_id << std::endl;

		unsigned idx = irq_id / 32;
		unsigned off = irq_id % 32;

		pending_interrupts[idx] &= ~(1 << off);
	}

	unsigned hart_get_next_pending_interrupt(unsigned hart_id, bool consider_threshold) {
		unsigned min_id = 0;
		unsigned max_priority = 0;

		for (unsigned i = 1; i < NumberInterrupts; ++i) {
			unsigned idx = i / 32;
			unsigned off = i % 32;

			if (hart_enabled_interrupts(hart_id, idx) & (1 << off)) {
				if (trace_mode)
					std::cout << "[vp::plic] hart " << hart_id << " has enabled ITR " << i << std::endl;
				if (pending_interrupts[idx] & (1 << off)) {
					auto prio = interrupt_priorities[i];
					if (trace_mode)
						std::cout << "[vp::plic] .. and it is pending with priority " << prio << std::endl;
					if (prio > 0 && (!consider_threshold || (prio > hart_config[hart_id].priority_threshold))) {
						if (trace_mode)
							std::cout << "[vp::plic]   .. which is greater than the hart's threshold of "
							          << hart_config[hart_id].priority_threshold << std::endl;
						if (prio > max_priority) {
							max_priority = prio;
							min_id = i;
						}
					}
				}
			}
		}

		return min_id;
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		delay += access_delay;
		// std::cout << "[vp::plic] Writing at 0x" << trans.get_address() << " value 0x" <<
		// *reinterpret_cast<uint32_t*>(trans.get_data_ptr()) << std::endl;
		vp::mm::route("FE310_PLIC", register_ranges, trans, delay);
	}

	void post_write_interrupt_priorities(RegisterRange::WriteInfo) {
		if (trace_mode)
			std::cout << "[vp::plic] wrote ITR priority:" << std::endl;
		unsigned i = 0;
		for (auto &x : interrupt_priorities) {
			x = std::min(x, MaxPriority);
			if (trace_mode)
				if (x)
					std::cout << "[vp::plic]\t Prio for ITR nr. " << i << ": " << x << std::endl;
			i++;
		}
	}

	bool pre_read_hart_config(RegisterRange::ReadInfo t) {
		assert(t.addr % 4 == 0);
		unsigned idx = t.addr / 4;

		if ((idx % 2) == 1) {
			// access is directed to claim response register
			assert(t.size == 4);
			--idx;

			unsigned min_id = hart_get_next_pending_interrupt(0, false);
			hart_config[idx].claim_response = min_id;
			clear_pending_interrupt(min_id);
		}

		return true;
	}

	void post_write_hart_config(RegisterRange::WriteInfo t) {
		assert(t.addr % 4 == 0);
		unsigned idx = t.addr / 4;

		if ((idx % 2) == 1) {
			// access is directed to claim response register
			assert(t.size == 4);
			--idx;
			if (trace_mode)
				std::cout << "[vp::plic] wrote ITR claim/response" << std::endl;

			if (hart_has_pending_enabled_interrupts(idx)) {
				assert(hart_eip[idx]);
				// trigger again to make this work even if the SW clears the harts interrupt pending bit
				target_harts[idx]->trigger_external_interrupt(irq_level);
			} else {
				hart_eip[idx] = false;
				target_harts[idx]->clear_external_interrupt(irq_level);
				if (trace_mode)
					std::cout << "[vp::plic] clear eip" << std::endl;
			}
		} else {
			if (trace_mode)
				std::cout << "[vp::plic] wrote ITR priority threshold 0x" << std::hex
				          << *reinterpret_cast<uint32_t *>(t.trans.get_data_ptr()) << " for hart " << idx / 2
				          << std::dec << std::endl;
		}
	}

	bool hart_has_pending_enabled_interrupts(unsigned hart_id) {
		return hart_get_next_pending_interrupt(hart_id, true) > 0;
	}

	void run() {
		while (true) {
			sc_core::wait(e_run);

			for (unsigned i = 0; i < NumberCores; ++i) {
				if (!hart_eip[i]) {
					if (hart_has_pending_enabled_interrupts(i)) {
						if (trace_mode)
							std::cout << "[vp::plic] trigger interrupt " << hart_get_next_pending_interrupt(i, true)
							          << std::hex << std::endl;
						hart_eip[i] = true;
						target_harts[i]->trigger_external_interrupt(irq_level);
					}
				}
			}
		}
	}
};
