#pragma once

#include <stdint.h>
#include <tlm_utils/simple_target_socket.h>
#include <unistd.h>  //truncate

#include <fstream>  //file IO
#include <iostream>
#include <systemc>

#include "platform/common/bus.h"
#include "util/propertymap.h"

using namespace std;
using namespace sc_core;
using namespace tlm_utils;

struct MemoryMappedFile : public sc_core::sc_module {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);
	unsigned int prop_access_clock_cycles = 3;

	sc_core::sc_time access_delay_base;

	simple_target_socket<MemoryMappedFile> tsock;

	string mFilepath;
	uint32_t mSize;
	fstream file;

	MemoryMappedFile(sc_module_name, string &filepath, uint32_t size) : mFilepath(filepath), mSize(size) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("MemoryMappedFile." + name(), "clock_cycle_period", sc_core::sc_time,
		                  prop_clock_cycle_period);
		VPPP_PROPERTY_GET("MemoryMappedFile." + name(), "access_clock_cycles", uint64_t, prop_access_clock_cycles);

		access_delay_base = prop_access_clock_cycles * prop_clock_cycle_period;

		tsock.register_b_transport(this, &MemoryMappedFile::transport);

		if (filepath.size() == 0 || size == 0) {  // no file
			return;
		}
		file.open(mFilepath, ofstream::in | ofstream::out | ofstream::binary);
		if (!file.is_open() || !file.good()) {
			// cerr << "Failed to open " << mFilepath << ": " << strerror(errno)
			// << endl;
			file.open(mFilepath, ofstream::in | ofstream::out | ofstream::binary | ios_base::trunc);
		}
		int stat = truncate(mFilepath.c_str(), mSize);
		assert(stat == 0 && "truncate failed");
		assert(file.is_open() && file.good() && "File could not be opened");
	}

	~MemoryMappedFile() {
		file.close();
	}

	void write_data(unsigned addr, uint8_t *src, unsigned num_bytes) {
		assert(addr + num_bytes <= mSize);
		if (!file.is_open()) {
			cerr << name() << ": ERROR: Write: No file mapped!" << endl;
			return;
		}
		file.seekg(addr, file.beg);
		if (!file.fail()) {
			file.write(reinterpret_cast<char *>(src), num_bytes);
		}
		if (file.fail()) {
			cerr << name() << ": ERROR: Failed to write to \"" << mFilepath << "\"!" << endl;
			file.clear();
		}
	}

	void read_data(unsigned addr, uint8_t *dst, unsigned num_bytes) {
		assert(addr + num_bytes <= mSize);
		if (!file.is_open()) {
			cerr << name() << ": ERROR: Read: No file mapped!" << endl;
			memset(dst, 0, num_bytes);
		}
		file.seekg(addr, file.beg);
		if (!file.fail()) {
			file.read(reinterpret_cast<char *>(dst), num_bytes);
		}
		if (file.fail()) {
			cerr << name() << ": ERROR: Failed to read from \"" << mFilepath << "\"!" << endl;
			file.clear();
			memset(dst, 0, num_bytes);
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		tlm::tlm_command cmd = trans.get_command();
		unsigned addr = trans.get_address();
		auto *ptr = trans.get_data_ptr();
		auto len = trans.get_data_length();

		assert(addr < mSize);

		if (cmd == tlm::TLM_WRITE_COMMAND) {
			write_data(addr, ptr, len);
		} else if (cmd == tlm::TLM_READ_COMMAND) {
			read_data(addr, ptr, len);
		} else {
			sc_assert(false && "unsupported tlm command");
		}

		delay += len * access_delay_base;
	}
};
