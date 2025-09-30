#ifndef RISCV_VP_SIFIVE_SPI_H
#define RISCV_VP_SIFIVE_SPI_H

/*
 * SPI Host for SiFive HiFive
 */

#include <tlm_utils/simple_target_socket.h>

#include <map>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "platform/common/spi_if.h"
#include "util/tlm_map.h"

template <unsigned int FIFO_QUEUE_SIZE>
class SIFIVE_SPI : public sc_core::sc_module, public SPI_IF {
	// single queue for all targets
	static constexpr uint_fast8_t queue_size = FIFO_QUEUE_SIZE;
	std::queue<uint8_t> rxqueue;

	// number of chip selects
	const uint32_t cs_width;
	uint32_t cs_width_mask;

	const int interrupt = -1;

	// memory mapped configuration registers
	uint32_t sckdiv = 0;
	uint32_t sckmode = 0;
	uint32_t csid = 0;
	uint32_t csdef = 0;
	uint32_t csmode = 0;
	uint32_t delay0 = 0;
	uint32_t delay1 = 0;
	uint32_t fmt = 0;
	uint32_t txdata = 0;
	uint32_t rxdata = 0;
	uint32_t txmark = 0;
	uint32_t rxmark = 0;
	uint32_t fctrl = 1;
	uint32_t ffmt = 0;
	uint32_t ie = 0;
	uint32_t ip = 0;

	// set by csmode
	// assert chipselect at beginning of each frame
	bool cs_select;
	// deassert chipselect at end of each frame
	bool cs_deselect;

	enum {
		SCKDIV_REG_ADDR = 0x00,
		SCKMODE_REG_ADDR = 0x04,
		CSID_REG_ADDR = 0x10,
		CSDEF_REG_ADDR = 0x14,
		CSMODE_REG_ADDR = 0x18,
		DELAY0_REG_ADDR = 0x28,
		DELAY1_REG_ADDR = 0x2C,
		FMT_REG_ADDR = 0x40,
		TXDATA_REG_ADDR = 0x48,
		RXDATA_REG_ADDR = 0x4C,
		TXMARK_REG_ADDR = 0x50,
		RXMARK_REG_ADDR = 0x54,
		FCTRL_REG_ADDR = 0x60,
		FFMT_REG_ADDR = 0x64,
		IE_REG_ADDR = 0x70,
		IP_REG_ADDR = 0x74,
	};

	enum CSMODE_VALS { AUTO = 0, RESERVED = 1, HOLD = 2, OFF = 3 };

	static constexpr uint_fast8_t SIFIVE_SPI_IP_TXWM = 0x1;
	static constexpr uint_fast8_t SIFIVE_SPI_IP_RXWM = 0x2;

	vp::map::LocalRouter router = {"SIFIVE_SPI"};
	void trigger_interrupt() {
		if (plic == nullptr || interrupt < 0) {
			return;
		}
		plic->gateway_trigger_interrupt(interrupt);
	}

	void update_csmode() {
		/* reset selected -> TODO: check if behavior correct */
		device_deselect();

		switch (csmode) {
			case CSMODE_VALS::AUTO:
				/* Assert/deassert CS at the beginning/end of each frame */
				cs_select = true;
				cs_deselect = true;
				break;
			case CSMODE_VALS::HOLD:
				/* Keep CS continuously asserted after the initial frame */
				cs_select = true;
				cs_deselect = false;
				break;
			case CSMODE_VALS::OFF:
				/* Disable hardware control of the CS pin */
				cs_select = false;
				cs_deselect = false;
				break;
			default:
				std::cerr << "SIFIVE_SPI: Invalid value for csmod " << csmode << std::endl;
				cs_select = true;
				cs_deselect = true;
				break;
		}
	}

	void register_access_callback(const vp::map::register_access_t &r) {
		bool trigger_interrupt = false;

		if (r.read) {
			if (r.vptr == &rxdata) {
				if (rxqueue.empty()) {
					rxdata = 1 << 31;
				} else {
					rxdata = rxqueue.front();
					rxqueue.pop();
				}
			}
		}

		uint32_t csid_old = csid;
		r.fn();

		if (r.write) {
			if (r.vptr == &csdef) {
				csdef &= cs_width_mask;

			} else if (r.vptr == &csid) {
				/* on change: reset selected -> TODO: check if behavior correct */
				if (csid_old != csid) {
					device_deselect();
				}

			} else if (r.vptr == &csmode) {
				update_csmode();

			} else if (r.vptr == &txdata) {
				uint8_t rxdata;
				transfer(csid, cs_select, cs_deselect, txdata, rxdata);

				// add with overflow
				rxqueue.push(rxdata);
				/*
				 * QUEUE_FULL HANDLING
				 * drop oldest element in rxqueue if too many rx (i.e. tx without rx)
				 */
				if (rxqueue.size() > queue_size) {
					rxqueue.pop();
				}

				if (txmark > 0) {
					ip |= SIFIVE_SPI_IP_TXWM;
					if (ie & SIFIVE_SPI_IP_TXWM) {
						trigger_interrupt = true;
					}
				} else {
					ip &= ~SIFIVE_SPI_IP_TXWM;
				}

				// TODO: Model latency.
				txdata = 0;
			}
		}

		if (rxqueue.size() > rxmark) {
			ip |= SIFIVE_SPI_IP_RXWM;
			if (ie & SIFIVE_SPI_IP_RXWM) {
				trigger_interrupt = true;
			}
		} else {
			ip &= ~SIFIVE_SPI_IP_RXWM;
		}

		if (trigger_interrupt) {
			this->trigger_interrupt();
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		router.transport(trans, delay);
	}

   public:
	tlm_utils::simple_target_socket<SIFIVE_SPI> tsock;
	interrupt_gateway *plic = nullptr;

	bool is_chipselect_valid(unsigned int cs) override {
		/*
		 * TODO: from hifive -> adjust csdef
		 * if (cs == 1 || cs > 3) {
		 * 	return false;
		 */
		return (cs <= cs_width);
	}

	SIFIVE_SPI(sc_core::sc_module_name, unsigned int cs_width, int interrupt = -1)
	    : cs_width(cs_width), interrupt(interrupt) {
		/* apply cs_width */
		cs_width_mask = (1 << cs_width) - 1;
		csdef = cs_width_mask;

		/* reset chip select handling (csmode) */
		csmode = CSMODE_VALS::AUTO;
		update_csmode();

		tsock.register_b_transport(this, &SIFIVE_SPI::transport);

		router
		    .add_register_bank({
		        {SCKDIV_REG_ADDR, &sckdiv},
		        {SCKMODE_REG_ADDR, &sckmode},
		        {CSID_REG_ADDR, &csid},
		        {CSDEF_REG_ADDR, &csdef},
		        {CSMODE_REG_ADDR, &csmode},
		        {DELAY0_REG_ADDR, &delay0},
		        {DELAY1_REG_ADDR, &delay1},
		        {FMT_REG_ADDR, &fmt},
		        {TXDATA_REG_ADDR, &txdata},
		        {RXDATA_REG_ADDR, &rxdata},
		        {TXMARK_REG_ADDR, &txmark},
		        {RXMARK_REG_ADDR, &rxmark},
		        {FCTRL_REG_ADDR, &fctrl},
		        {FFMT_REG_ADDR, &ffmt},
		        {IE_REG_ADDR, &ie},
		        {IP_REG_ADDR, &ip},
		    })
		    .register_handler(this, &SIFIVE_SPI::register_access_callback);
	}
};

#endif  // RISCV_VP_SIFIVE_SPI_H
