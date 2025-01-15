#pragma once

#include <cstdint>
#include <string>
#include <systemc>
#include <vector>

class NetTrace {
   public:
	NetTrace(int port);
	void dump_arch();
	void add_arch(std::vector<std::string> modules);
	void dump_transaction(bool is_read, std::string initiator, int target, uint64_t glob_addr, unsigned char *data_ptr,
	                      unsigned int data_length, sc_core::sc_time delay);

   private:
	int port;
	int sockfd;
	int client_sock;
	std::vector<std::string> memmap;
	static constexpr char hex[] = "0123456789ABCDEF";

	void send_packet(const char *data);
	void wait_for_client();
	void create_sock();
};