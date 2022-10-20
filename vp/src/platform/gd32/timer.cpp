#include "timer.h"

enum {
	MTIME_BASE = 0x0,
	MTIME_SIZE = 8,

	MTIMECMP_BASE = 0x8,
	MTIMECMP_SIZE = 8,

	MSFTRST_BASE = 0xFF0,
	MSFTRST_SIZE = 4,

	MTIMECTL_BASE = 0xFF8,
	MTIMECTL_SIZE = 4,

	MSIP_BASE = 0xFFC,
	MSIP_SIZE = 4,

	MSIP_HART0_BASE = 0x1000,
	MSIP_HART0_SIZE = 4,

	MSIP_HART1_BASE = 0x1004,
	MSIP_HART1_SIZE = 4,

	MSIP_HART2_BASE = 0x1008,
	MSIP_HART2_SIZE = 4,

	MSIP_HART3_BASE = 0x100C,
	MSIP_HART3_SIZE = 4,

	MTIMECMP_HART0_BASE = 0x5000,
	MTIMECMP_HART0_SIZE = 8,

	MTIMECMP_HART1_BASE = 0x5008,
	MTIMECMP_HART1_SIZE = 8,

	MTIMECMP_HART2_BASE = 0x5010,
	MTIMECMP_HART2_SIZE = 8,

	MTIMECMP_HART3_BASE = 0x50018,
	MTIMECMP_HART3_SIZE = 8,

	MTIME_CLINT_BASE = 0xCFF8,
	MTIME_CLINT_SIZE = 8,
};

TIMER::TIMER(sc_core::sc_module_name)
    : regs_mtime(MTIME_BASE, MTIME_SIZE),
      regs_mtimecmp(MTIMECMP_BASE, MTIMECMP_SIZE),
      regs_msftrst(MSFTRST_BASE, MSFTRST_SIZE),
      regs_mtimectl(MTIMECTL_BASE, MTIMECTL_SIZE),
      regs_msip(MSIP_BASE, MSIP_SIZE),
      regs_msip_hart0(MSIP_HART0_BASE, MSIP_HART0_SIZE),
      regs_msip_hart1(MSIP_HART1_BASE, MSIP_HART1_SIZE),
      regs_msip_hart2(MSIP_HART2_BASE, MSIP_HART2_SIZE),
      regs_msip_hart3(MSIP_HART3_BASE, MSIP_HART3_SIZE),
      regs_mtimecmp_hart0(MTIMECMP_HART0_BASE, MTIMECMP_HART0_SIZE),
      regs_mtimecmp_hart1(MTIMECMP_HART1_BASE, MTIMECMP_HART1_SIZE),
      regs_mtimecmp_hart2(MTIMECMP_HART2_BASE, MTIMECMP_HART2_SIZE),
      regs_mtimecmp_hart3(MTIMECMP_HART3_BASE, MTIMECMP_HART3_SIZE),
      regs_mtime_clint(MTIME_CLINT_BASE, MTIME_CLINT_SIZE),

      mtime(regs_mtime),
      mtimecmp(regs_mtimecmp),
      msftrst(regs_msftrst),
      mtimectl(regs_mtimectl),
      msip(regs_msip),
      msip_hart0(regs_msip_hart0),
      msip_hart1(regs_msip_hart1),
      msip_hart2(regs_msip_hart2),
      msip_hart3(regs_msip_hart3),
      mtimecmp_hart0(regs_mtimecmp_hart0),
      mtimecmp_hart1(regs_mtimecmp_hart1),
      mtimecmp_hart2(regs_mtimecmp_hart2),
      mtimecmp_hart3(regs_mtimecmp_hart3),
      mtime_clint(regs_mtime_clint) {
	for (auto reg : register_ranges) reg->alignment = 4;
	tsock.register_b_transport(this, &TIMER::transport);

	regs_mtime.pre_read_callback = std::bind(&TIMER::pre_read_mtime, this, std::placeholders::_1);
	regs_mtime.post_write_callback = std::bind(&TIMER::post_write_mtime, this, std::placeholders::_1);
	regs_mtimectl.post_write_callback = std::bind(&TIMER::post_write_mtimectl, this, std::placeholders::_1);
	regs_msip.post_write_callback = std::bind(&TIMER::post_write_msip, this, std::placeholders::_1);
	regs_mtimecmp.post_write_callback = std::bind(&TIMER::post_write_mtimecmp, this, std::placeholders::_1);

	SC_THREAD(run);
}

bool TIMER::pre_read_mtime(RegisterRange::ReadInfo t) {
	update_and_get_mtime();
	return true;
}

void TIMER::post_write_mtime(RegisterRange::WriteInfo t) {
	const uint32_t nv = *t.trans.get_data_ptr();
	base = sc_core::sc_time_stamp().value() - nv;
	reset_mtime = true;
	update_and_get_mtime();
	irq_event.notify(t.delay);
}

void TIMER::post_write_mtimecmp(RegisterRange::WriteInfo t) {
	irq_event.notify(t.delay);
}

void TIMER::post_write_mtimectl(RegisterRange::WriteInfo t) {
	const uint32_t nv = *t.trans.get_data_ptr();
	if (nv & 1) {
		pause = sc_core::sc_time_stamp().value();
		pause_mtime = true;
	} else {
		auto now = sc_core::sc_time_stamp().value();
		base = base + (now - pause);
		pause_mtime = false;
		update_and_get_mtime();
	}
	irq_event.notify(t.delay);
}

void TIMER::post_write_msip(RegisterRange::WriteInfo t) {
	const uint32_t nv = *t.trans.get_data_ptr();
	if (nv & 1)
		eclic->gateway_trigger_interrupt(SW_INT_NUM);
	else
		eclic->gateway_clear_interrupt(SW_INT_NUM);
}

uint64_t TIMER::update_and_get_mtime() {
	const auto now = (sc_core::sc_time_stamp().value() - base) / scaler;
	if (!pause_mtime && (now > mtime || reset_mtime)) {
		// only update time if timer is not paused and
		// do not update backward in time (e.g. due to local quantums in tlm transaction processing)
		// unless mtime is reset
		mtime.write(now);
		reset_mtime = false;
	}
	return mtime;
}

void TIMER::notify_later() {
	if (mtimecmp > 0 && mtimecmp < UINT64_MAX) {
		const auto time = sc_core::sc_time::from_value(mtime * scaler);
		const auto goal = sc_core::sc_time::from_value(mtimecmp * scaler);
		irq_event.notify(goal - time);
	}
}

void TIMER::run() {
	while (true) {
		sc_core::wait(irq_event);

		update_and_get_mtime();

		if (mtimecmp > 0 && mtime >= mtimecmp) {
			eclic->gateway_trigger_interrupt(TIMER_INT_NUM);
			if (mtimectl & 0b10) {  // reset mtime
				base = sc_core::sc_time_stamp().value();
				reset_mtime = true;
				update_and_get_mtime();
				notify_later();
			}
		} else {
			notify_later();
		}
	}
}

void TIMER::transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
	vp::mm::route("TIMER", register_ranges, trans, delay);
}
