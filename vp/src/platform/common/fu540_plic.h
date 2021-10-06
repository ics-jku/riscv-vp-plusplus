#pragma once

#include <stdint.h>
#include <map>

/**
 * This class implements a Platform-Level Interrupt Controller (PLIC) as
 * defined in chapter 10 of the SiFive FU540-C000 manual.
 */
struct FU540_PLIC : public sc_core::sc_module, public interrupt_gateway {
public:
	static constexpr int      NUMIRQ   = 53;
	static constexpr uint32_t MAX_THR  = 7;
	static constexpr uint32_t MAX_PRIO = 7;

	static constexpr uint32_t ENABLE_BASE = 0x2000;
	static constexpr uint32_t ENABLE_PER_HART = 0x80;
	static constexpr uint32_t CONTEXT_BASE = 0x200000;
	static constexpr uint32_t CONTEXT_PER_HART = 0x1000;
	static constexpr uint32_t HART_REG_SIZE = 2 * sizeof(uint32_t);

	tlm_utils::simple_target_socket<FU540_PLIC> tsock;
	std::vector<external_interrupt_target *> target_harts{};

	FU540_PLIC(sc_core::sc_module_name, unsigned harts = 5);
	void gateway_trigger_interrupt(uint32_t);

	SC_HAS_PROCESS(FU540_PLIC);

private:
	class HartConfig {
	  public:
		ArrayView<uint32_t> m_mode;
		ArrayView<uint32_t> s_mode; /* same as m_mode for hart0 */

		HartConfig(RegisterRange &r1, RegisterRange &r2) : m_mode(r1), s_mode(r2) {
			return;
		}

		bool is_enabled(unsigned int, PrivilegeLevel);
	};

	sc_core::sc_event e_run;
	sc_core::sc_time clock_cycle;

	std::vector<RegisterRange*> register_ranges;

	/* hart_id (0..4) → hart_config */
	typedef std::map<unsigned int, HartConfig*> hartmap;
	hartmap enabled_irqs;
	hartmap hart_context;

	/* See Section 10.3 */
	RegisterRange regs_interrupt_priorities{0x4, sizeof(uint32_t) * NUMIRQ};
	ArrayView<uint32_t> interrupt_priorities{regs_interrupt_priorities};

	/* See Section 10.4 */
	RegisterRange regs_pending_interrupts{0x1000, sizeof(uint32_t) * 2};
	ArrayView<uint32_t> pending_interrupts{regs_pending_interrupts};
	
	void create_registers(void);
	void create_hart_regs(uint64_t, uint64_t, hartmap&);
	void transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
	bool read_hartctx(RegisterRange::ReadInfo, unsigned int, PrivilegeLevel);
	void write_hartctx(RegisterRange::WriteInfo, unsigned int, PrivilegeLevel);
	void write_irq_prios(RegisterRange::WriteInfo);
	void run(void);
	unsigned int next_pending_irq(unsigned int, PrivilegeLevel, bool);
	bool has_pending_irq(unsigned int, PrivilegeLevel*);
	uint32_t get_threshold(unsigned int, PrivilegeLevel);
	void clear_pending(unsigned int);
	bool is_pending(unsigned int);
	bool is_claim_access(uint64_t addr);
};
