#ifndef RISCV_VP_LOAD_IF_H
#define RISCV_VP_LOAD_IF_H

#include <stddef.h>
#include <stdint.h>

#include <boost/iostreams/device/mapped_file.hpp>
#include <iostream>

class load_if {
   public:
	virtual uint64_t get_size() = 0;
	virtual void load_data(const char *src, uint64_t dst_addr, size_t n) = 0;
	virtual void load_zero(uint64_t dst_addr, size_t n) = 0;

	void load_binary_file(const std::string &filename, uint64_t addr) {
		/*
		 * check, if file exists, is readable and don't has zero size
		 * (prevent segfault on mapped_source_file)
		 */
		std::ifstream file;
		file.open(filename, std::ifstream::in | std::ifstream::binary | std::ios::ate);
		if (file.fail() || file.tellg() == 0) {
			std::cerr << ": ERROR: Open: \"" << filename << "\"!" << std::endl;
			assert(0);
		}
		file.close();

		boost::iostreams::mapped_file_source mf(filename);
		assert(mf.is_open());
		load_data((const char *)mf.data(), addr, mf.size());
	}
};

#endif
