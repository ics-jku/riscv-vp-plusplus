#include "net_trace.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <systemc>

NetTrace::NetTrace(int port) {
	this->port = port;
	create_sock();
	client_sock = -1;
}

void NetTrace::add_arch(std::vector<std::string> modules) {
	for (auto m : modules) {
		std::string msg = "I " + m + "\n";
		std::replace(msg.begin(), msg.end(), ' ', ';');
		memmap.push_back(msg);
	}
}

void NetTrace::dump_arch() {
	if (client_sock == -1) {
		wait_for_client();
	}
	if (memmap.size() > 0) {
		for (auto m : memmap) {
			send_packet(m.c_str());
		}
	}
}

void NetTrace::dump_transaction(bool is_read, std::string initiator, int target, uint64_t glob_addr,
                                unsigned char* data_ptr, unsigned int data_length, sc_core::sc_time delay) {
	std::string time_stamp = std::to_string(static_cast<unsigned long long>(
	    sc_core::sc_time_stamp().to_default_time_units() + delay.to_default_time_units()));
	std::string msg = is_read ? "R;" : "W;";
	std::stringstream addr;
	addr << std::hex << glob_addr;

	uint8_t* buffer = new unsigned char[data_length];
	mempcpy(buffer, data_ptr, data_length);

	std::string result;
	result.reserve(data_length * 2);

	for (unsigned int i = 0; i < data_length; i++) {
		result.push_back(hex[buffer[i] / 16]);
		result.push_back(hex[buffer[i] % 16]);
	}

	msg += initiator + ';' + std::to_string(target) + ';' + addr.str() + ';' + time_stamp + ';' +
	       std::to_string(data_length) + ';' + result + '\n';

	delete[] buffer;

	send_packet(msg.c_str());
}

void NetTrace::send_packet(const char* data) {
	if (client_sock == -1) {
		return;
	}
	ssize_t ret, w;
	size_t len = std::strlen(data);

	w = 0;
	do {
		assert(len >= (size_t)w);
		ret = write(client_sock, &data[w], len - (size_t)w);
		if (ret < 0) {
			throw std::system_error(errno, std::generic_category());
		}
		w += ret;
	} while ((size_t)w < len);
}

void NetTrace::wait_for_client() {
	do {
		std::cerr << "Waiting for connection on port " << port << std::endl;
		client_sock = accept(sockfd, NULL, NULL);
	} while (client_sock == -1);

	std::cerr << "Connection established" << std::endl;
}

void NetTrace::create_sock() {
	struct sockaddr_in addr;
	int reuse;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		throw std::system_error(errno, std::generic_category());

	reuse = 1;
	if (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &reuse, sizeof(reuse)) == -1)
		goto err;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1)
		goto err;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		std::cout << "Error: Could not bind to socket";
		goto err;
	}

	if (listen(sockfd, 0) == -1) {
		std::cout << "Error: Could not listen on socket";
		goto err;
	}

	return;

err:
	close(sockfd);
	throw std::system_error(errno, std::generic_category());
	std::terminate();
}
