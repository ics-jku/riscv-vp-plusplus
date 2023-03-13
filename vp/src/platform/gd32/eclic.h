#ifndef RISCV_VP_ECLIC_H
#define RISCV_VP_ECLIC_H

#include <tlm_utils/simple_target_socket.h>

#include <mutex>
#include <queue>
#include <systemc>

#include "nuclei_core/nuclei_irq_if.h"
#include "util/nuclei_memory_map.h"
#include "util/tlm_map.h"

#define NUMBER_INTERRUPTS 87
#define MAX_PRIORITY 15

struct Interrupt {
	uint32_t id;
	uint8_t level;
	uint8_t priority;

	Interrupt(uint32_t id, uint8_t clicintctl, uint32_t clicinfo, uint8_t cliccfg) : id(id) {
		const uint8_t clicintctlbits = (clicinfo & 0x1E00000) >> 21;
		uint8_t nlbits = (cliccfg & 0x1E) >> 1;

		if (nlbits > clicintctlbits)
			nlbits = clicintctlbits;

		const uint8_t level_mask = 0xFF << (8 - nlbits);
		const uint8_t level_extend = 0xFF >> nlbits;
		level = (clicintctl & level_mask) | level_extend;

		const uint8_t priority_mask = (0xFF >> nlbits) & (0xFF << (8 - clicintctlbits));
		const uint8_t priority_extend = 0xFF >> clicintctlbits;
		priority = (clicintctl & priority_mask) | priority_extend;
	}
};

class InterruptComparator {
   public:
	inline bool operator()(const Interrupt &a, const Interrupt &b) const {
		return (b.level > a.level) || (b.level == a.level && b.priority > a.priority) ||
		       (b.level == a.level && b.priority == a.priority && b.id > a.id);
	}
};

template <unsigned NumberInterrupts, uint32_t MaxPriority>
class ECLIC : public sc_core::sc_module, public nuclei_interrupt_gateway {
   public:
	tlm_utils::simple_target_socket<ECLIC> tsock;

	RegisterRange regs_cliccfg{0x0, 1};
	IntegerView<uint8_t> cliccfg{regs_cliccfg};

	RegisterRange regs_clicinfo{0x4, 4};
	IntegerView<uint32_t> clicinfo{regs_clicinfo};

	RegisterRange regs_mth{0xb, 1};
	IntegerView<uint8_t> mth{regs_mth};

	ModRegisterRange<4> regs_clicintip{0x1000, NumberInterrupts};
	ArrayView<uint8_t> clicintip{regs_clicintip};

	ModRegisterRange<4> regs_clicintie{0x1001, NumberInterrupts};
	ArrayView<uint8_t> clicintie{regs_clicintie};

	ModRegisterRange<4> regs_clicintattr{0x1002, NumberInterrupts};
	ArrayView<uint8_t> clicintattr{regs_clicintattr};

	ModRegisterRange<4> regs_clicintctl{0x1003, NumberInterrupts};
	ArrayView<uint8_t> clicintctl{regs_clicintctl};

	std::vector<RegisterRange *> register_ranges{&regs_cliccfg,   &regs_clicinfo,    &regs_mth,       &regs_clicintip,
	                                             &regs_clicintie, &regs_clicintattr, &regs_clicintctl};

	std::priority_queue<Interrupt, std::vector<Interrupt>, InterruptComparator> pending_interrupts;
	std::mutex pending_interrupts_mutex;

	ECLIC(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &ECLIC::transport);

		// fill clicinfo register with following information
		// cf. "core_feature_eclic.h"
		// CLICINTCTLBITS = 4, VERSION = 1, NUM_INTERRUPT = NumberInterrupts
		const uint32_t info = 4 << 21 | 1 << 13 | NumberInterrupts;
		clicinfo.write(info);
		regs_clicinfo.readonly = true;

		cliccfg.write(1);  // LSB is reserved and ties to 1
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		vp::mm::route("ECLIC", register_ranges, trans, delay);
	}

	void gateway_trigger_interrupt(uint32_t irq_id) {
		assert(irq_id > 0 && irq_id < NumberInterrupts);
		clicintip[irq_id] = 1;
		std::lock_guard<std::mutex> guard(pending_interrupts_mutex);
		pending_interrupts.push({irq_id, clicintctl[irq_id], clicinfo, cliccfg});
	}

	void gateway_clear_interrupt(uint32_t irq_id) {
		assert(irq_id > 0 && irq_id < NumberInterrupts);
		clicintip[irq_id] = 0;
	}
};

#endif  // RISCV_VP_ECLIC_H
