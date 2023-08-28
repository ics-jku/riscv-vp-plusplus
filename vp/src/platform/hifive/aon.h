#ifndef RISCV_VP_AON_H
#define RISCV_VP_AON_H

#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

struct AON : public sc_core::sc_module {
	tlm_utils::simple_target_socket<AON> tsock;

	uint32_t wdogcfg = 0;
	uint32_t wdogcount = 0;
	uint32_t wdogfeed = 0;
	uint32_t wdogkey = 0;
	uint32_t wdogcmp0 = 0;

	uint32_t lfrosccfg = 0;
	uint32_t pmucause = 1 << 8;

	uint32_t rtccfg = 0;
	uint32_t rtccountlo = 0;
	uint32_t rtccounthi = 0;
	uint32_t rtcs = 0;
	uint32_t rtccmp0 = 0;

	uint32_t backup0 = 0;
	uint32_t backup1 = 0;
	uint32_t backup2 = 0;
	uint32_t backup3 = 0;
	uint32_t backup4 = 0;
	uint32_t backup5 = 0;
	uint32_t backup6 = 0;
	uint32_t backup7 = 0;
	uint32_t backup8 = 0;
	uint32_t backup9 = 0;
	uint32_t backup10 = 0;
	uint32_t backup11 = 0;
	uint32_t backup12 = 0;
	uint32_t backup13 = 0;
	uint32_t backup14 = 0;
	uint32_t backup15 = 0;
	uint32_t backup16 = 0;
	uint32_t backup17 = 0;
	uint32_t backup18 = 0;
	uint32_t backup19 = 0;
	uint32_t backup20 = 0;
	uint32_t backup21 = 0;
	uint32_t backup22 = 0;
	uint32_t backup23 = 0;
	uint32_t backup24 = 0;
	uint32_t backup25 = 0;
	uint32_t backup26 = 0;
	uint32_t backup27 = 0;
	uint32_t backup28 = 0;
	uint32_t backup29 = 0;
	uint32_t backup30 = 0;
	uint32_t backup31 = 0;

	enum {
		WDOG_CFG_REG_ADDR = 0x000,
		WDOG_CNT_REG_ADDR = 0x010,
		WDOG_FEED_REG_ADDR = 0x018,
		WDOG_KEY_REG_ADDR = 0x01C,
		WDOG_CMP0_REG_ADDR = 0x020,

		RTC_CFG_REG_ADDR = 0x040,
		RTC_COUNT_LO_REG_ADDR = 0x048,
		RTC_COUNT_HI_REG_ADDR = 0x04C,
		RTC_S_REG_ADDR = 0x050,
		RTC_CMP0 = 0x060,

		LFROSCCFG_REG_ADDR = 0x70,

		BACKUP0_REG_ADDR = 0x80,
		BACKUP1_REG_ADDR = 0x84,
		BACKUP2_REG_ADDR = 0x88,
		BACKUP3_REG_ADDR = 0x8C,
		BACKUP4_REG_ADDR = 0x90,
		BACKUP5_REG_ADDR = 0x94,
		BACKUP6_REG_ADDR = 0x98,
		BACKUP7_REG_ADDR = 0x9C,
		BACKUP8_REG_ADDR = 0xA0,
		BACKUP9_REG_ADDR = 0xA4,
		BACKUP10_REG_ADDR = 0xA8,
		BACKUP11_REG_ADDR = 0xAC,
		BACKUP12_REG_ADDR = 0xB0,
		BACKUP13_REG_ADDR = 0xB4,
		BACKUP14_REG_ADDR = 0xB8,
		BACKUP15_REG_ADDR = 0xBC,
		BACKUP16_REG_ADDR = 0xC0,
		BACKUP17_REG_ADDR = 0xC4,
		BACKUP18_REG_ADDR = 0xC8,
		BACKUP19_REG_ADDR = 0xCC,
		BACKUP20_REG_ADDR = 0xD0,
		BACKUP21_REG_ADDR = 0xD4,
		BACKUP22_REG_ADDR = 0xD8,
		BACKUP23_REG_ADDR = 0xDC,
		BACKUP24_REG_ADDR = 0xE0,
		BACKUP25_REG_ADDR = 0xE4,
		BACKUP26_REG_ADDR = 0xE8,
		BACKUP27_REG_ADDR = 0xEC,
		BACKUP28_REG_ADDR = 0xF0,
		BACKUP29_REG_ADDR = 0xF4,
		BACKUP30_REG_ADDR = 0xF8,
		BACKUP31_REG_ADDR = 0xFC,

		PMUCAUSE_REG_ADDR = 0x144,
	};

	vp::map::LocalRouter router = {"AON"};

	AON(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &AON::transport);

		router
		    .add_register_bank({
		        {WDOG_CFG_REG_ADDR, &wdogcfg},
		        {WDOG_CNT_REG_ADDR, &wdogcount},
		        {WDOG_FEED_REG_ADDR, &wdogfeed},
		        {WDOG_KEY_REG_ADDR, &wdogkey},
		        {WDOG_CMP0_REG_ADDR, &wdogcmp0},

		        {RTC_CFG_REG_ADDR, &rtccfg},
		        {RTC_COUNT_LO_REG_ADDR, &rtccountlo},
		        {RTC_COUNT_HI_REG_ADDR, &rtccounthi},
		        {RTC_S_REG_ADDR, &rtcs},
		        {RTC_CMP0, &rtccmp0},

		        {LFROSCCFG_REG_ADDR, &lfrosccfg},
		        {PMUCAUSE_REG_ADDR, &pmucause},

		        {BACKUP0_REG_ADDR, &backup0},
		        {BACKUP1_REG_ADDR, &backup1},
		        {BACKUP2_REG_ADDR, &backup2},
		        {BACKUP3_REG_ADDR, &backup3},
		        {BACKUP4_REG_ADDR, &backup4},
		        {BACKUP5_REG_ADDR, &backup5},
		        {BACKUP6_REG_ADDR, &backup6},
		        {BACKUP7_REG_ADDR, &backup7},
		        {BACKUP8_REG_ADDR, &backup8},
		        {BACKUP9_REG_ADDR, &backup9},
		        {BACKUP10_REG_ADDR, &backup10},
		        {BACKUP11_REG_ADDR, &backup11},
		        {BACKUP12_REG_ADDR, &backup12},
		        {BACKUP13_REG_ADDR, &backup13},
		        {BACKUP14_REG_ADDR, &backup14},
		        {BACKUP15_REG_ADDR, &backup15},
		        {BACKUP16_REG_ADDR, &backup16},
		        {BACKUP17_REG_ADDR, &backup17},
		        {BACKUP18_REG_ADDR, &backup18},
		        {BACKUP19_REG_ADDR, &backup19},
		        {BACKUP20_REG_ADDR, &backup20},
		        {BACKUP21_REG_ADDR, &backup21},
		        {BACKUP22_REG_ADDR, &backup22},
		        {BACKUP23_REG_ADDR, &backup23},
		        {BACKUP24_REG_ADDR, &backup24},
		        {BACKUP25_REG_ADDR, &backup25},
		        {BACKUP26_REG_ADDR, &backup26},
		        {BACKUP27_REG_ADDR, &backup27},
		        {BACKUP28_REG_ADDR, &backup28},
		        {BACKUP29_REG_ADDR, &backup29},
		        {BACKUP30_REG_ADDR, &backup30},
		        {BACKUP31_REG_ADDR, &backup31},
		    })
		    .register_handler(this, &AON::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		r.fn();
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};

#endif  // RISCV_VP_AON_H
