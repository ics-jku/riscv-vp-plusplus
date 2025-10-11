#pragma once

#include <arpa/inet.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/irq_if.h"
#include "util/propertymap.h"
#include "util/tlm_map.h"

struct MaskROM : public sc_core::sc_module {
	tlm_utils::simple_target_socket<MaskROM> tsock;

	const uint32_t baseAddr = 0x1000;
	const uint32_t configStringOffs = 0x03b0;
	const char *configString =
	    "/cs-v1/;"
	    "/{"
	    "model = \"SiFive,FE310G-0000-Z0\";"
	    "compatible = \"sifive,fe300\";"
	    "/include/ 0x20004;"
	    "};";

	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);

	sc_core::sc_time access_delay_base;

	MaskROM(sc_core::sc_module_name) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("MaskROM." + name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);

		access_delay_base = prop_clock_cycle_period / 2;

		tsock.register_b_transport(this, &MaskROM::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		tlm::tlm_command cmd = trans.get_command();
		unsigned addr = trans.get_address();
		auto *ptr = trans.get_data_ptr();
		auto len = trans.get_data_length();

		if (cmd != tlm::TLM_READ_COMMAND) {
			throw(std::runtime_error("invalid write to MROM"));
		}

		if (addr < 0x000C) {  // should contain jump to OTP
			memset(ptr, 0, len);
			return;
		}

		if (addr + len <= 0x000C + sizeof(uint32_t)) {
			uint32_t buf = baseAddr + configStringOffs;
			memcpy(ptr, &buf, sizeof(uint32_t));
			delay += len * access_delay_base;
			return;
		}

		if (addr < configStringOffs) {
			std::cerr << "invalid access to Mask-ROM at " << std::hex << addr << std::endl;
			assert(false);
			return;
		}

		if (addr < configStringOffs + strlen(configString)) {
			uint32_t offs = addr - configStringOffs;
			uint8_t cut = offs + len <= strlen(configString) ? len : (offs + len) - strlen(configString);
			memcpy(ptr, &configString[offs], cut);
			if (cut != len) {
				memset(&ptr[cut], 0, len - cut);
			}
			return;
		}

		memset(ptr, 0, len);
		delay += len * access_delay_base;

		return;
	}
};
