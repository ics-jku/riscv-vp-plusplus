#pragma once

#include <linux/can.h>
#include <net/if.h>

#include <functional>
#include <thread>

#include "can/mcp_can_dfs.h"
#include "platform/common/sifive_spi.h"

class CAN {
	enum class State {
		init,
		readRegister,
		writeRegister,
		bitmod,

		loadTX0,
		loadTX1,
		loadTX2,

		sendTX0,
		sendTX1,
		sendTX2,
		sendALL,

		readRX0,
		readRX1,

		getStatus,

		shit,
		wank,
		fuck,
		arse,
		crap,
		dick,
	} state;

	std::thread listener;

	uint8_t registers[MCP_RXB1SIDH + 1];

	struct MCPFrame {
		union frame {
			uint8_t raw[5 + CANFD_MAX_DLEN];
			struct f_fields {
				union f_id {
					uint8_t id[4];
					struct fields {
						/*
						    MCP_SIDH        0
						    MCP_SIDL        1
						    MCP_EID8        2
						    MCP_EID0        3
						 */
						uint16_t sid;
						uint16_t eid;
					} fields;
				} f_id;
				uint8_t length;
				uint8_t payload[CANFD_MAX_DLEN];
			} f_fields;
		} frame;
	};

	MCPFrame txBuf[3];
	MCPFrame rxBuf[2];

	uint8_t status;

	int s;
	struct sockaddr_can addr;
	struct ifreq ifr;

	volatile bool stop;

   public:
	CAN();
	~CAN();

	uint8_t write(uint8_t byte);

	const char* registerName(uint8_t id);
	const char* regValueName(uint8_t id);
	const char* spiInstName(uint8_t id);

	void command(uint8_t byte);
	uint8_t readRegister(uint8_t byte);
	uint8_t writeRegister(uint8_t byte);
	uint8_t modifyRegister(uint8_t byte);

	uint8_t loadTxBuf(uint8_t no, uint8_t byte);
	uint8_t sendTxBuf(uint8_t no, uint8_t byte);

	uint8_t readRxBuf(uint8_t no, uint8_t byte);

	void mcp2515_id_to_buf(const unsigned long id, uint8_t* idField, const bool extended = false);
	void mcp2515_buf_to_id(unsigned& id, bool& extended, uint8_t* idField);

	void enqueueIncomingCanFrame(const struct can_frame& frame);
	void listen();
};
