#pragma once

#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <systemc>

#include "core/common/load_if.h"
#include "platform/common/bus.h"
#include "util/propertymap.h"
#include "util/tlm_ext_tag.h"

struct TaggedMemory : public sc_core::sc_module, public load_if {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_access_clock_cycles = 1;

	sc_core::sc_time access_delay;

	tlm_utils::simple_target_socket<TaggedMemory> tsock;

	uint8_t *data;
	uint32_t size;
	bool read_only;
	std::vector<bool> tag_bits;
	const uint8_t CLEN = 16;  // TODO Only true for RV64 // TODO Get this from ISS // CLEN = 2*MXLEN
	uint64_t mem_start_addr;
	uint64_t mem_end_addr;

	TaggedMemory(sc_core::sc_module_name, uint32_t size, bool read_only = false)
	    : data(new uint8_t[size]()), size(size), read_only(read_only) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("TaggedMemory." + name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);
		VPPP_PROPERTY_GET("TaggedMemory." + name(), "access_clock_cycles", uint64_t, prop_access_clock_cycles);

		access_delay = prop_access_clock_cycles * prop_clock_cycle_period;

		// Initialize tag_bits after printing the value of CLEN
		tag_bits = std::vector<bool>(size / CLEN);
		tsock.register_b_transport(this, &TaggedMemory::transport);
		tsock.register_get_direct_mem_ptr(this, &TaggedMemory::get_direct_mem_ptr);
		tsock.register_transport_dbg(this, &TaggedMemory::transport_dbg);
	}

	~TaggedMemory(void) {
		delete[] data;
	}

	uint64_t get_size() override {
		return size;
	}

	void load_data(const char *src, uint64_t dst_addr, size_t n) override {
		assert(dst_addr + n <= size);
		memcpy(&data[dst_addr], src, n);
	}

	void load_zero(uint64_t dst_addr, size_t n) override {
		assert(dst_addr + n <= size);
		memset(&data[dst_addr], 0, n);
	}

	void write_data(unsigned addr, const uint8_t *src, unsigned num_bytes, bool tag) {
		if (tag && num_bytes != CLEN) {
			assert(0 && "Tagged data must be CLEN bytes");
		}
		if (tag && addr % CLEN != 0) {
			assert(0 && "Tagged data must be aligned to CLEN");
		}
		assert(addr + num_bytes <= size);

		memcpy(data + addr, src, num_bytes);
		tag_bits[(addr / CLEN)] = tag;
	}

	void write_data(unsigned addr, const uint8_t *src, unsigned num_bytes) {
		assert(addr + num_bytes <= size);

		memcpy(data + addr, src, num_bytes);
		for (unsigned i = 0; i < num_bytes / 16; i++) {
			tag_bits[(addr + i * 16) / CLEN] = false;
		}
		if (num_bytes < 16) {
			// Not a full capability --> clear tag
			tag_bits[(addr / CLEN)] = false;
		}
	}

	bool read_data(unsigned addr, uint8_t *dst, unsigned num_bytes) {
		assert(addr + num_bytes <= size);
		memcpy(dst, data + addr, num_bytes);
		return tag_bits[(addr / CLEN)];
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		transport_dbg(trans);
		delay += access_delay;
	}

	unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
		//	printf("TaggedMemory transport_dbg\n");
		tlm::tlm_command cmd = trans.get_command();
		unsigned addr = trans.get_address();
		auto *ptr = trans.get_data_ptr();
		auto len = trans.get_data_length();
		tlm_ext_tag *ext;
		trans.get_extension(ext);
		if (ext == nullptr) {
			// TODO Error handling
			assert(0);
		}
		if (!ext) {
			// TODO Error handling
			assert(0);
		}

		assert(addr < size);

		if (cmd == tlm::TLM_WRITE_COMMAND) {
			write_data(addr, ptr, len, ext->tag);
		} else if (cmd == tlm::TLM_READ_COMMAND) {
			ext->tag = read_data(addr, ptr, len);
		} else {
			sc_assert(false && "unsupported tlm command");
		}

		return len;
	}

	bool get_direct_mem_ptr(tlm::tlm_generic_payload &trans, tlm::tlm_dmi &dmi) {
		(void)trans;
		dmi.set_start_address(0);
		dmi.set_end_address(size);
		dmi.set_dmi_ptr(data);
		if (read_only)
			dmi.allow_read();
		else
			dmi.allow_read_write();
		return true;
	}

	void reset() {
		memset(data, 0, size);
		tag_bits = std::vector<bool>(size / CLEN);
	}
};
