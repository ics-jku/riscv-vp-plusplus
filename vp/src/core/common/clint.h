#ifndef RISCV_ISA_CLINT_H
#define RISCV_ISA_CLINT_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "clint_if.h"
#include "irq_if.h"
#include "util/memory_map.h"

template <unsigned NumberOfCores>
struct CLINT : public clint_if, public sc_core::sc_module {
	//
	// core local interrupt controller (provides local timer interrupts with
	// memory mapped configuration)
	//
	// send out interrupt if: *mtime >= mtimecmp* and *mtimecmp != 0*
	//
	// *mtime* is a read-only 64 bit timer register shared by all CPUs (accessed
	// by MMIO and also mapped into the CSR address space of each CPU)
	//
	// for every CPU a *mtimecmp* register (read/write 64 bit) is available
	//
	//
	// Note: the software is responsible to ensure that no overflow occurs when
	// writing *mtimecmp* (see RISC-V privileged ISA spec.):
	//
	// # New comparand is in a1:a0.
	// li t0, -1
	// sw t0, mtimecmp    # No smaller than old value.
	// sw a1, mtimecmp+4  # No smaller than new value.
	// sw a0, mtimecmp    # New value.
	//

	static_assert(NumberOfCores < 4096, "out of bound");  // stay within the allocated address range

	static constexpr uint64_t scaler = 1000000;  // scale from PS resolution (default in SystemC) to US
	                                             // resolution (apparently required by FreeRTOS)

	tlm_utils::simple_target_socket<CLINT> tsock;

	sc_core::sc_time clock_cycle = sc_core::sc_time(10, sc_core::SC_NS);
	sc_core::sc_event irq_event;

	RegisterRange regs_mtime{0xBFF8, 8};
	IntegerView<uint64_t> mtime{regs_mtime};

	RegisterRange regs_mtimecmp{0x4000, 8 * NumberOfCores};
	ArrayView<uint64_t> mtimecmp{regs_mtimecmp};

	RegisterRange regs_msip{0x0, 4 * NumberOfCores};
	ArrayView<uint32_t> msip{regs_msip};

	std::vector<RegisterRange *> register_ranges{&regs_mtime, &regs_mtimecmp, &regs_msip};

	std::array<clint_interrupt_target *, NumberOfCores> target_harts{};

	SC_HAS_PROCESS(CLINT);

	CLINT(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &CLINT::transport);

		regs_mtimecmp.alignment = 4;
		regs_msip.alignment = 4;
		regs_mtime.alignment = 4;

		regs_mtime.pre_read_callback = std::bind(&CLINT::pre_read_mtime, this, std::placeholders::_1);
		regs_mtimecmp.post_write_callback = std::bind(&CLINT::post_write_mtimecmp, this, std::placeholders::_1);
		regs_msip.post_write_callback = std::bind(&CLINT::post_write_msip, this, std::placeholders::_1);

		SC_THREAD(run);
	}

	uint64_t update_and_get_mtime() override {
		auto now = sc_core::sc_time_stamp().value() / scaler;
		if (now > mtime)
			mtime = now;  // do not update backward in time (e.g. due to local quantums in tlm transaction processing)
		return mtime;
	}

	void run() {
		while (true) {
			sc_core::wait(irq_event);

			update_and_get_mtime();

			for (unsigned i = 0; i < NumberOfCores; ++i) {
				auto cmp = mtimecmp[i];
				// std::cout << "[vp::clint] process mtimecmp[" << i << "]=" << cmp << ", mtime=" << mtime << std::endl;
				if (cmp > 0 && mtime >= cmp) {
					// std::cout << "[vp::clint] set timer interrupt for core " << i << std::endl;
					target_harts[i]->trigger_timer_interrupt();
				} else {
					// std::cout << "[vp::clint] unset timer interrupt for core " << i << std::endl;
					target_harts[i]->clear_timer_interrupt();
					if (cmp > 0 && cmp < UINT64_MAX) {
						auto time = sc_core::sc_time::from_value(mtime * scaler);
						auto goal = sc_core::sc_time::from_value(cmp * scaler);
						// std::cout << "[vp::clint] time=" << time << std::endl;
						// std::cout << "[vp::clint] goal=" << goal << std::endl;
						// std::cout << "[vp::clint] goal-time=delay=" << goal-time << std::endl;
						irq_event.notify(goal - time);
					}
				}
			}
		}
	}

	bool pre_read_mtime(RegisterRange::ReadInfo t) {
		sc_core::sc_time now = sc_core::sc_time_stamp() + t.delay;

		mtime.write(now.value() / scaler);

		return true;
	}

	void post_write_mtimecmp(RegisterRange::WriteInfo t) {
		// std::cout << "[vp::clint] write mtimecmp[addr=" << t.addr << "]=" << mtimecmp[t.addr / 8] << ", mtime=" <<
		// mtime << std::endl;
		irq_event.notify(t.delay);
	}

	void post_write_msip(RegisterRange::WriteInfo t) {
		assert(t.addr % 4 == 0);
		unsigned idx = t.addr / 4;
		msip[idx] &= 0x1;
		if (msip[idx] != 0) {
			target_harts[idx]->trigger_software_interrupt();
		} else {
			target_harts[idx]->clear_software_interrupt();
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		delay += 2 * clock_cycle;

		vp::mm::route("CLINT", register_ranges, trans, delay);
	}
};

#endif  // RISCV_ISA_CLINT_H
