#pragma once

#include <tlm_utils/simple_target_socket.h>

#include <queue>
#include <systemc>

#include "util/tlm_map.h"

class SpiInterface {
   public:
	virtual ~SpiInterface(){};
	virtual uint8_t write(uint8_t byte) = 0;
};

typedef uint32_t Pin;

struct SPI : public sc_core::sc_module {
	tlm_utils::simple_target_socket<SPI> tsock;

	static constexpr uint_fast8_t buffer_size = 1;
	std::queue<uint16_t> buffer;

	// memory mapped configuration registers
	uint32_t spi_ctl0 = 0x0000;
	uint32_t spi_ctl1 = 0x0000;
	uint32_t spi_stat = 0x0002;
	uint32_t spi_data = 0x0000;
	uint32_t spi_crcpoly = 0x0007;
	uint32_t spi_rcrc = 0x0000;
	uint32_t spi_tcrc = 0x0000;
	uint32_t spi_i2sctl = 0x0000;
	uint32_t spi_i2spsc = 0x0002;

	enum {
		SPI_CTL0_REG_ADDR = 0x00,
		SPI_CTL1_REG_ADDR = 0x04,
		SPI_STAT_REG_ADDR = 0x08,
		SPI_DATA_REG_ADDR = 0x0C,
		SPI_CRCPOLY_REG_ADDR = 0x10,
		SPI_RCRC_REG_ADDR = 0x14,
		SPI_TCRC_REG_ADDR = 0x18,
		SPI_I2SCTL_REG_ADDR = 0x1C,
		SPI_I2SPSC_REG_ADDR = 0x20,
	};

	vp::map::LocalRouter router = {"SPI"};

	SPI(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &SPI::transport);

		router
		    .add_register_bank({
		        {SPI_CTL0_REG_ADDR, &spi_ctl0},
		        {SPI_CTL1_REG_ADDR, &spi_ctl1},
		        {SPI_STAT_REG_ADDR, &spi_stat},
		        {SPI_DATA_REG_ADDR, &spi_data},
		        {SPI_CRCPOLY_REG_ADDR, &spi_crcpoly},
		        {SPI_RCRC_REG_ADDR, &spi_rcrc},
		        {SPI_TCRC_REG_ADDR, &spi_tcrc},
		        {SPI_I2SCTL_REG_ADDR, &spi_i2sctl},
		        {SPI_I2SPSC_REG_ADDR, &spi_i2spsc},
		    })
		    .register_handler(this, &SPI::register_access_callback);
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		// TODO csid & interrupts
		if (r.read) {
			if (r.vptr == &spi_data) {
				if (!buffer.empty()) {
					spi_data = buffer.front();
					buffer.pop();
				}
				spi_stat = spi_stat & 0xFE;  // set RBNE flag to 0
			}
		}

		r.fn();

		if (r.write) {
			if (r.vptr == &spi_data) {
				buffer.push(spi_data);
				// TODO write/transmit to spi device
				spi_stat = spi_stat | 0x02;  // set TBE flag to 1
			}
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}
};
