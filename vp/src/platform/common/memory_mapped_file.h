#pragma once

#include <stdint.h>
#include <unistd.h>  //truncate
#include <fstream>   //file IO
#include <iostream>

#include <tlm_utils/simple_target_socket.h>
#include <systemc>

#include "bus.h"

using namespace std;
using namespace sc_core;
using namespace tlm_utils;

struct MemoryMappedFile : public sc_core::sc_module {
	simple_target_socket<MemoryMappedFile> tsock;

	string mFilepath;
	uint32_t mSize;
	fstream file;

	MemoryMappedFile(sc_module_name, string &filepath, uint32_t size) : mFilepath(filepath), mSize(size) {
		tsock.register_b_transport(this, &MemoryMappedFile::transport);

		if (filepath.size() == 0 || size == 0) {  // no file
			return;
		}
		file.open(mFilepath, ofstream::in | ofstream::out | ofstream::binary);
		if (!file.is_open() || !file.good()) {
			// cout << "Failed to open " << mFilepath << ": " << strerror(errno)
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
		file.seekg(addr, file.beg);
		file.write(reinterpret_cast<char *>(src), num_bytes);
		if (!file.is_open() || !file.good()) {
			cout << "Failed to write " << mFilepath << ": " << strerror(errno) << endl;
		}
	}

	void read_data(unsigned addr, uint8_t *dst, unsigned num_bytes) {
		assert(addr + num_bytes <= mSize);
		file.seekg(addr, file.beg);
		file.read(reinterpret_cast<char *>(dst), num_bytes);
		if (!file.is_open() || !file.good()) {
			cout << "Failed to read " << mFilepath << ": " << strerror(errno) << endl;
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

		delay += sc_core::sc_time(len * 30, sc_core::SC_NS);
	}
};
