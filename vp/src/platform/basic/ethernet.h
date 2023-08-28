#ifndef RISCV_VP_ETHERNET_H
#define RISCV_VP_ETHERNET_H

#include <tlm_utils/simple_target_socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <ios>
#include <list>
#include <map>
#include <systemc>
#include <unordered_map>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

struct arp_eth_header {
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t oper;
	uint8_t sender_mac[6];
	uint8_t sender_ip[4];
	uint8_t target_mac[6];
	uint8_t target_ip[4];
};

struct EthernetDevice : public sc_core::sc_module {
	tlm_utils::simple_target_socket<EthernetDevice> tsock;

	interrupt_gateway *plic = 0;
	uint32_t irq_number = 0;
	sc_core::sc_event run_event;

	// memory mapped configuration registers
	uint32_t status = 0;
	uint32_t receive_size = 0;
	uint32_t receive_dst = 0;
	uint32_t send_src = 0;
	uint32_t send_size = 0;
	uint32_t mac[2];

	uint8_t *VIRTUAL_MAC_ADDRESS = reinterpret_cast<uint8_t *>(mac);
	uint8_t BROADCAST_MAC_ADDRESS[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	uint8_t *mem = nullptr;

	vp::map::LocalRouter router;

	int sockfd = 0;
	int interfaceIdx = 0;

	static const uint16_t MTU_SIZE = 1500;
	static const uint16_t FRAME_SIZE = MTU_SIZE + 14;

	uint8_t recv_frame_buf[FRAME_SIZE];
	bool has_frame;
	bool disabled;

	static const uint16_t STATUS_REG_ADDR = 0x00;
	static const uint16_t RECEIVE_SIZE_REG_ADDR = STATUS_REG_ADDR + sizeof(uint32_t);
	static const uint16_t RECEIVE_DST_REG_ADDR = RECEIVE_SIZE_REG_ADDR + sizeof(uint32_t);
	static const uint16_t SEND_SRC_REG_ADDR = RECEIVE_DST_REG_ADDR + sizeof(uint32_t);
	static const uint16_t SEND_SIZE_REG_ADDR = SEND_SRC_REG_ADDR + sizeof(uint32_t);
	static const uint16_t MAC_HIGH_REG_ADDR = SEND_SIZE_REG_ADDR + sizeof(uint32_t);
	static const uint16_t MAC_LOW_REG_ADDR = MAC_HIGH_REG_ADDR + sizeof(uint32_t);

	enum : uint16_t {
		RECV_OPERATION = 1,
		SEND_OPERATION = 2,
	};

	SC_HAS_PROCESS(EthernetDevice);

	EthernetDevice(sc_core::sc_module_name, uint32_t irq_number, uint8_t *mem, std::string clonedev);

	void init_network(std::string clonedev);
	void add_all_if_ips();

	void send_raw_frame();
	bool try_recv_raw_frame();
	bool isPacketForUs(uint8_t *packet, ssize_t size);

	void register_access_callback(const vp::map::register_access_t &r) {
		assert(mem);
		assert(!disabled && "Tried accessing disabled network device");

		r.fn();

		if (r.write && r.vptr == &status) {
			if (r.nv == RECV_OPERATION) {
				assert(has_frame);
				memcpy(&mem[receive_dst - 0x80000000], recv_frame_buf, receive_size);
				has_frame = false;
				receive_size = 0;
			} else if (r.nv == SEND_OPERATION) {
				send_raw_frame();
			} else {
				throw std::runtime_error("unsupported operation");
			}
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}

	void run() {
		while (!disabled) {
			run_event.notify(sc_core::sc_time(10000, sc_core::SC_US));
			sc_core::wait(run_event);  // 10000 times per second by default

			// check if data is available on the socket, if yes store it in an
			// internal buffer
			if (!has_frame) {
				while (!try_recv_raw_frame())
					;
				if (has_frame)
					plic->gateway_trigger_interrupt(irq_number);
			}
		}
	}
};

#endif  // RISCV_VP_ETHERNET_H
