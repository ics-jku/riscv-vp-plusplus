#include "sifive_plic.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "util/propertymap.h"

inline uint32_t GET_IDX(uint32_t &irq) {
	return irq / 32;
}
inline uint32_t GET_OFF(uint32_t &irq) {
	return 1 << irq % 32;
}

/**
 * TODO: Ensure that irq 0 is hardwired to zero
 * TODO: FE310 raises external interrupt during interrupt completion
 */

static void assert_addr(size_t start, size_t end, RegisterRange *range) {
	assert(range->start == start && range->end + 1 == end + sizeof(uint32_t));
}

SIFIVE_PLIC::SIFIVE_PLIC(sc_core::sc_module_name, bool fu540_mode, unsigned harts, unsigned numirq)
    : FU540_MODE(fu540_mode), NUMIRQ(numirq), HART_REG_SIZE(((NUMIRQ + 63) / 64) * sizeof(uint64_t)) {
	/* get config properties from global property tree (or use default) */
	VPPP_PROPERTY_GET("SIFIVE_PLIC." + name(), "clock_cycle_period", sc_time, prop_clock_cycle_period);
	VPPP_PROPERTY_GET("SIFIVE_PLIC." + name(), "access_clock_cycles", uint64, prop_access_clock_cycles);
	VPPP_PROPERTY_GET("SIFIVE_PLIC." + name(), "irq_trigger_clock_cycles", uint64, prop_irq_trigger_clock_cycles);

	target_harts = std::vector<external_interrupt_target *>(harts, NULL);

	access_delay = prop_access_clock_cycles * prop_clock_cycle_period;
	irq_trigger_delay = prop_irq_trigger_clock_cycles * prop_clock_cycle_period;

	create_registers();
	tsock.register_b_transport(this, &SIFIVE_PLIC::transport);

	SC_THREAD(run);
};

void SIFIVE_PLIC::create_registers(void) {
	regs_interrupt_priorities.post_write_callback =
	    std::bind(&SIFIVE_PLIC::write_irq_prios, this, std::placeholders::_1);

	/* make pending interrupts read-only */
	regs_pending_interrupts.pre_write_callback = [](RegisterRange::WriteInfo) { return false; };

	if (FU540_MODE) {
		/* The priorities end address, as documented in the FU540-C000
		 * manual, is incorrect <https://github.com/riscv/opensbi/pull/138> */
		assert_addr(0x4, 0xD4, &regs_interrupt_priorities);
		assert_addr(0x1000, 0x1004, &regs_pending_interrupts);
	}

	register_ranges.push_back(&regs_interrupt_priorities);
	register_ranges.push_back(&regs_pending_interrupts);

	/* create IRQ enable and context registers */
	create_hart_regs(ENABLE_BASE, ENABLE_PER_HART, enabled_irqs);
	create_hart_regs(CONTEXT_BASE, CONTEXT_PER_HART, hart_context);

	/* only supports "naturally aligned 32-bit memory accesses" */
	for (size_t i = 0; i < register_ranges.size(); i++) register_ranges[i]->alignment = sizeof(uint32_t);
}

void SIFIVE_PLIC::create_hart_regs(uint64_t addr, uint64_t inc, hartmap &map) {
	auto add_reg = [this, addr](unsigned int h, PrivilegeLevel l, uint64_t a) {
		RegisterRange *r = new RegisterRange(a, HART_REG_SIZE);
		if (addr == CONTEXT_BASE) {
			r->pre_read_callback = std::bind(&SIFIVE_PLIC::read_hartctx, this, std::placeholders::_1, h, l);
			r->post_write_callback = std::bind(&SIFIVE_PLIC::write_hartctx, this, std::placeholders::_1, h, l);
		}

		register_ranges.push_back(r);
		return r;
	};

	for (size_t hart = 0; hart < target_harts.size(); hart++) {
		RegisterRange *mreg, *sreg;

		mreg = add_reg(hart, MachineMode, addr);
		sreg = mreg; /* for hart0 */

		if (FU540_MODE && hart == 0) {
			/* fu540 hart 0 only supports m-mode interrupts -> do nothing */
		} else {
			addr += inc;
			sreg = add_reg(hart, SupervisorMode, addr);
		}

		map[hart] = new HartConfig(NUMIRQ, *mreg, *sreg);
		addr += inc;
	}
}

void SIFIVE_PLIC::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	delay += access_delay;
	vp::mm::route("SIFIVE_PLIC", register_ranges, trans, delay);
};

void SIFIVE_PLIC::gateway_trigger_interrupt(uint32_t irq) {
	if (irq == 0 || irq > NUMIRQ)
		throw std::invalid_argument("IRQ value is invalid");

	pending_interrupts[GET_IDX(irq)] |= GET_OFF(irq);
	e_run.notify(irq_trigger_delay);
};

bool SIFIVE_PLIC::read_hartctx(RegisterRange::ReadInfo t, unsigned int hart, PrivilegeLevel level) {
	assert(t.addr % sizeof(uint32_t) == 0);
	assert(t.size == sizeof(uint32_t));

	if (is_claim_access(t.addr)) {
		unsigned int irq = next_pending_irq(hart, level, true);

		/* if there is no pending irq zero needs to be written
		 * to the claim register. next_pending_irq returns 0 in
		 * this case so no special handling required. */

		switch (level) {
			case MachineMode:
				hart_context[hart]->m_mode[1] = irq;
				break;
			case SupervisorMode:
				hart_context[hart]->s_mode[1] = irq;
				break;
			default:
				assert(0);
				break;
		}

		/* successful claim also clears the pending bit */
		if (irq != 0)
			clear_pending(irq);
	}

	return true;
}

void SIFIVE_PLIC::write_hartctx(RegisterRange::WriteInfo t, unsigned int hart, PrivilegeLevel level) {
	assert(t.addr % sizeof(uint32_t) == 0);
	assert(t.size == sizeof(uint32_t));

	if (is_claim_access(t.addr)) {
		target_harts[hart]->clear_external_interrupt(level);
	} else { /* access to priority threshold */
		uint32_t *thr;

		switch (level) {
			case MachineMode:
				thr = &hart_context[hart]->m_mode[0];
				break;
			case SupervisorMode:
				thr = &hart_context[hart]->s_mode[0];
				break;
			default:
				assert(0);
				break;
		}

		*thr = std::min(*thr, uint32_t(MAX_THR));
	}
}

void SIFIVE_PLIC::write_irq_prios(RegisterRange::WriteInfo t) {
	size_t idx = t.addr / sizeof(uint32_t);
	assert(idx <= NUMIRQ);

	auto &elem = interrupt_priorities[idx];
	elem = std::min(elem, uint32_t(MAX_PRIO));
}

void SIFIVE_PLIC::run(void) {
	for (;;) {
		sc_core::wait(e_run);

		for (size_t i = 0; i < target_harts.size(); i++) {
			PrivilegeLevel lvl;
			if (has_pending_irq(i, &lvl)) {
				target_harts[i]->trigger_external_interrupt(lvl);
			}
		}
	}
}

/* Returns next enabled pending interrupt with highest priority */
unsigned int SIFIVE_PLIC::next_pending_irq(unsigned int hart, PrivilegeLevel lvl, bool ignth) {
	if (FU540_MODE) {
		/* fu540 hart 0 only supports m-mode interrupts */
		assert(!(hart == 0 && lvl == SupervisorMode));
	}

	HartConfig *conf = enabled_irqs[hart];
	unsigned int selirq = 0, maxpri = 0;

	for (unsigned irq = 1; irq <= NUMIRQ; irq++) {
		if (!conf->is_enabled(irq, lvl) || !is_pending(irq))
			continue;

		uint32_t prio = interrupt_priorities[irq - 1];
		if (!ignth && prio <= get_threshold(hart, lvl))
			continue;

		if (prio > maxpri) {
			maxpri = prio;
			selirq = irq;
		}
	}

	return selirq;
}

bool SIFIVE_PLIC::has_pending_irq(unsigned int hart, PrivilegeLevel *level) {
	if (FU540_MODE) {
		/* fu540 hart 0 only supports m-mode interrupts */
		if (hart != 0 && next_pending_irq(hart, SupervisorMode, false) > 0) {
			*level = SupervisorMode;
			return true;
		}
	} else {
		if (next_pending_irq(hart, SupervisorMode, false) > 0) {
			*level = SupervisorMode;
			return true;
		}
	}

	if (next_pending_irq(hart, MachineMode, false) > 0) {
		*level = MachineMode;
		return true;
	}

	return false;
}

uint32_t SIFIVE_PLIC::get_threshold(unsigned int hart, PrivilegeLevel level) {
	if (FU540_MODE) {
		/* fu540 hart 0 only supports m-mode interrupts */
		if (hart == 0 && level == SupervisorMode) {
			throw std::invalid_argument("hart0 doesn't support SupervisorMode");
		}
	}

	HartConfig *conf = hart_context[hart];
	switch (level) {
		case MachineMode:
			return conf->m_mode[0];
			break;
		case SupervisorMode:
			return conf->s_mode[0];
			break;
		default:
			throw std::invalid_argument("Invalid PrivilegeLevel");
	}
}

void SIFIVE_PLIC::clear_pending(unsigned int irq) {
	assert(irq > 0 && irq <= NUMIRQ);
	pending_interrupts[GET_IDX(irq)] &= ~(GET_OFF(irq));
}

bool SIFIVE_PLIC::is_pending(unsigned int irq) {
	assert(irq > 0 && irq <= NUMIRQ);
	return pending_interrupts[GET_IDX(irq)] & GET_OFF(irq);
}

bool SIFIVE_PLIC::is_claim_access(uint64_t addr) {
	unsigned idx = addr / sizeof(uint32_t);
	return (idx % 2) == 1;
}

bool SIFIVE_PLIC::HartConfig::is_enabled(unsigned int irq, PrivilegeLevel level) {
	assert(irq > 0 && irq <= NUMIRQ);

	unsigned int idx = GET_IDX(irq);
	unsigned int off = GET_OFF(irq);

	switch (level) {
		case MachineMode:
			return m_mode[idx] & off;
		case SupervisorMode:
			return s_mode[idx] & off;
		default:
			assert(0);
	}

	return false;
}
