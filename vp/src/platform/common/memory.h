#ifndef RISCV_ISA_MEMORY_H
#define RISCV_ISA_MEMORY_H

#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>

#include <boost/iostreams/device/mapped_file.hpp>
#include <fstream>
#include <iostream>
#include <systemc>

#include "bus.h"
#include "load_if.h"
#include "util/propertymap.h"

struct SimpleMemory : public sc_core::sc_module, public load_if {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_access_clock_cycles = 1;

	sc_core::sc_time access_delay;

	tlm_utils::simple_target_socket<SimpleMemory> tsock;

	uint8_t *data;
	uint64_t size;
	bool read_only;

	SimpleMemory(sc_core::sc_module_name, uint64_t size, bool read_only = false)
	    : data(new uint8_t[size]()), size(size), read_only(read_only) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("SimpleMemory." + name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);
		VPPP_PROPERTY_GET("SimpleMemory." + name(), "access_clock_cycles", uint64_t, prop_access_clock_cycles);

		access_delay = prop_access_clock_cycles * prop_clock_cycle_period;

		tsock.register_b_transport(this, &SimpleMemory::transport);
		tsock.register_get_direct_mem_ptr(this, &SimpleMemory::get_direct_mem_ptr);
		tsock.register_transport_dbg(this, &SimpleMemory::transport_dbg);
	}

	~SimpleMemory(void) {
		delete[] data;
	}

	void load_data(const char *src, uint64_t dst_addr, size_t n) override {
		assert(dst_addr + n <= size);
		memcpy(&data[dst_addr], src, n);
	}

	void load_zero(uint64_t dst_addr, size_t n) override {
		assert(dst_addr + n <= size);
		memset(&data[dst_addr], 0, n);
	}

	void load_binary_file(const std::string &filename, uint64_t addr) {
		/*
		 * check, if file exists, is readable and don't has zero size
		 * (prevent segfault on mapped_source_file)
		 */
		std::ifstream file;
		file.open(filename, std::ifstream::in | std::ifstream::binary | std::ios::ate);
		if (file.fail() || file.tellg() == 0) {
			std::cerr << name() << ": ERROR: Open: \"" << filename << "\"!" << std::endl;
			assert(0);
		}
		file.close();

		boost::iostreams::mapped_file_source mf(filename);
		assert(mf.is_open());
		write_data(addr, (const uint8_t *)mf.data(), mf.size());
	}

	void write_data(uint64_t addr, const uint8_t *src, unsigned num_bytes) {
		assert(addr + num_bytes <= size);

		memcpy(data + addr, src, num_bytes);
	}

	void read_data(uint64_t addr, uint8_t *dst, unsigned num_bytes) {
		assert(addr + num_bytes <= size);

		memcpy(dst, data + addr, num_bytes);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		transport_dbg(trans);
		delay += access_delay;
	}

	unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
		tlm::tlm_command cmd = trans.get_command();
		unsigned addr = trans.get_address();
		auto *ptr = trans.get_data_ptr();
		auto len = trans.get_data_length();

		assert(addr < size);

		if (cmd == tlm::TLM_WRITE_COMMAND) {
			write_data(addr, ptr, len);
		} else if (cmd == tlm::TLM_READ_COMMAND) {
			read_data(addr, ptr, len);
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
};

#endif  // RISCV_ISA_MEMORY_H
