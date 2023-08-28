#include "debug_memory.h"

#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

unsigned DebugMemoryInterface::_do_dbg_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data,
                                                   unsigned num_bytes) {
	tlm::tlm_generic_payload trans;
	trans.set_command(cmd);
	trans.set_address(addr);
	trans.set_data_ptr(data);
	trans.set_data_length(num_bytes);
	trans.set_response_status(tlm::TLM_OK_RESPONSE);

	unsigned nbytes = isock->transport_dbg(trans);
	if (trans.is_response_error())
		throw std::runtime_error("debug transaction failed");

	return nbytes;
}

std::string DebugMemoryInterface::read_memory(uint64_t start, unsigned nbytes) {
	std::vector<uint8_t> buf(nbytes);  // NOTE: every element is zero-initialized by default

	unsigned nbytes_read = _do_dbg_transaction(tlm::TLM_READ_COMMAND, start, buf.data(), buf.size());
	assert(nbytes_read == nbytes && "not all bytes read");

	std::stringstream stream;
	stream << std::setfill('0') << std::hex;
	for (uint8_t byte : buf) {
		stream << std::setw(2) << (unsigned)byte;
	}

	return stream.str();
}

void DebugMemoryInterface::write_memory(uint64_t start, unsigned nbytes, const std::string &data) {
	unsigned i, j;
	std::vector<uint8_t> buf(data.length() / 2);

	assert(data.length() % 2 == 0);
	assert(buf.size() == nbytes);

	for (j = 0, i = 0; i + 1 < nbytes; j++, i += 2) {
		long num;
		char hex[3];

		hex[0] = data.at(i);
		hex[1] = data.at(i + 1);
		hex[2] = '\0';

		errno = 0;
		num = strtol(hex, NULL, 16);
		if (num == 0 && errno != 0)
			throw std::system_error(errno, std::generic_category());

		assert(num >= 0 && num <= UINT8_MAX);
		buf.at(j) = (uint8_t)num;
	}

	unsigned nbytes_write = _do_dbg_transaction(tlm::TLM_WRITE_COMMAND, start, buf.data(), buf.size());
	assert(nbytes_write == nbytes && "not all data written");
}
