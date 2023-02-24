#include "real_clint.h"

#include <assert.h>
#include <stddef.h>

enum {
	MSIP_BASE = 0,
	MSIP_SIZE = 4,

	MTIMECMP_BASE = 0x4000,
	MTIMECMP_SIZE = 8,

	MTIME_BASE = 0xBFF8,
	MTIME_SIZE = 8,
};

enum {
	MSIP_MASK = 0x1,  // The upper MSIP bits are tied to zero
};

/* This is used to quantize a 1MHz value to the closest 32768Hz value */
#define DIVIDEND (uint64_t(15625) / uint64_t(512))

static void timercb(void *arg) {
	AsyncEvent *event = (AsyncEvent *)arg;
	event->notify();
}

RealCLINT::RealCLINT(sc_core::sc_module_name, std::vector<clint_interrupt_target *> &_harts)
    : regs_msip(MSIP_BASE, MSIP_SIZE * _harts.size()),
      regs_mtimecmp(MTIMECMP_BASE, MTIMECMP_SIZE * _harts.size()),
      regs_mtime(MTIME_BASE, MTIME_SIZE),

      msip(regs_msip),
      mtimecmp(regs_mtimecmp),
      mtime(regs_mtime),

      harts(_harts) {
	for (size_t i = 0; i < harts.size(); i++) {
		Timer *timer = new Timer(timercb, &event);
		timers.push_back(timer);
	}

	register_ranges.insert(register_ranges.end(), {&regs_mtimecmp, &regs_msip, &regs_mtime});
	for (auto reg : register_ranges) reg->alignment = 4;

	regs_mtimecmp.post_write_callback = std::bind(&RealCLINT::post_write_mtimecmp, this, std::placeholders::_1);
	regs_msip.post_write_callback = std::bind(&RealCLINT::post_write_msip, this, std::placeholders::_1);

	regs_mtime.pre_read_callback = std::bind(&RealCLINT::pre_read_mtime, this, std::placeholders::_1);
	regs_mtime.post_write_callback = std::bind(&RealCLINT::post_write_mtime, this, std::placeholders::_1);

	first_mtime = std::chrono::high_resolution_clock::now();
	tsock.register_b_transport(this, &RealCLINT::transport);

	SC_METHOD(interrupt);
	sensitive << event;
	dont_initialize();
}

RealCLINT::~RealCLINT(void) {
	for (auto timer : timers) delete timer;
}

uint64_t RealCLINT::update_and_get_mtime(void) {
	time_point now = std::chrono::high_resolution_clock::now();
	usecs duration = std::chrono::duration_cast<usecs>(now - first_mtime);

	mtime = usec_to_ticks(duration);
	return mtime;
}

uint64_t RealCLINT::usec_to_ticks(usecs usec) {
	// This algorithm is inspired by the implementation provided by RIOT-OS.
	// https://github.com/RIOT-OS/RIOT/blob/d382bd656569599691c1a3e1c9b1662e07cf1a42/sys/include/xtimer/tick_conversion.h#L100-L106

	uint64_t microseconds = usec.count();
	return microseconds / DIVIDEND;
}

RealCLINT::usecs RealCLINT::ticks_to_usec(uint64_t ticks) {
	// See comment in RealCLINT::usec_to_ticks
	return usecs(ticks * DIVIDEND);
}

void RealCLINT::post_write_mtimecmp(RegisterRange::WriteInfo info) {
	assert(info.addr % 4 == 0);
	unsigned hart = info.addr / MTIMECMP_SIZE;

	uint64_t cmp = mtimecmp.at(hart);
	uint64_t time = update_and_get_mtime();

	Timer *timer = timers.at(hart);
	timer->pause();

	if (time >= cmp) {
		harts.at(hart)->trigger_timer_interrupt();
		return;
	}
	harts.at(hart)->clear_timer_interrupt();

	uint64_t goal_ticks = cmp - time;
	usecs duration = ticks_to_usec(goal_ticks);

	timer->start(duration);
}

void RealCLINT::post_write_msip(RegisterRange::WriteInfo info) {
	assert(info.addr % 4 == 0);
	unsigned hart = info.addr / MSIP_SIZE;

	msip.at(hart) &= MSIP_MASK;
	if (msip.at(hart) != 0) {
		harts.at(hart)->trigger_software_interrupt();
	} else {
		harts.at(hart)->clear_software_interrupt();
	}
}

void RealCLINT::post_write_mtime(RegisterRange::WriteInfo info) {
	/* TODO:
	 *  1. Adjust first_mtime to reflect new mtime register value.
	 *  2. Notify asyncEvent to check if a timmer intr must be raised.
	 *     Potentially requires stopping existing timers. */
	(void)info;
}

bool RealCLINT::pre_read_mtime(RegisterRange::ReadInfo info) {
	(void)info;

	update_and_get_mtime();
	return true;
}

void RealCLINT::interrupt(void) {
	update_and_get_mtime();

	for (size_t i = 0; i < harts.size(); i++) {
		auto cmp = mtimecmp.at(i);
		if (mtime >= cmp)
			harts.at(i)->trigger_timer_interrupt();
	}
}

void RealCLINT::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	vp::mm::route("RealCLINT", register_ranges, trans, delay);
}
