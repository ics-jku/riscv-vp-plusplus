#ifndef RISCV_ISA_DMA_H
#define RISCV_ISA_DMA_H

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <systemc>
#include <unordered_map>

#include "util/initiator_if.h"
#include "util/propertytree.h"
#include "util/tlm_ext_initiator.h"

/*
 * Register size and offsets
 * RV64Y uses 64-bit registers (e.g. to hold full 64-bit addresses),
 * all other targets keep the 32-bit register layout for compatibility.
 */
#if defined(TARGET_RV64Y)
#define DMA_REG_TYPE uint64_t
#define DMA_REG_SIZE 8
#else
#define DMA_REG_TYPE uint32_t
#define DMA_REG_SIZE 4
#endif

struct SimpleDMA : public sc_core::sc_module, public initiator_if {
	/* config properties */
	sc_core::sc_time prop_clock_cycle_period = sc_core::sc_time(10, sc_core::SC_NS);

	tlm_utils::simple_initiator_socket<SimpleDMA> isock;
	tlm_utils::simple_target_socket<SimpleDMA> tsock;

	tlm::tlm_generic_payload trans;
	tlm_ext_initiator *ext;

	interrupt_gateway *plic = 0;
	uint32_t irq_number = 0;

	std::array<uint8_t, 64> buffer;

	DMA_REG_TYPE src = 0;
	DMA_REG_TYPE dst = 0;
	DMA_REG_TYPE len = 0;
	DMA_REG_TYPE op = 0;
	DMA_REG_TYPE stat = 0;

	std::unordered_map<uint64_t, DMA_REG_TYPE *> addr_to_reg;

	enum {
		OP_NOP = 0,
		OP_MEMCPY = 1,
		OP_MEMSET = 2,
		OP_MEMCMP = 3,
		OP_MEMCHR = 4,
		OP_MEMMOVE = 5,
	};

	enum {
		SRC_ADDR = 0,
		DST_ADDR = DMA_REG_SIZE,
		LEN_ADDR = DMA_REG_SIZE * 2,
		OP_ADDR = DMA_REG_SIZE * 3,
		STAT_ADDR = DMA_REG_SIZE * 4,
	};

	sc_core::sc_event run_event;

	SC_HAS_PROCESS(SimpleDMA);

	SimpleDMA(sc_core::sc_module_name, uint32_t irq_number) : irq_number(irq_number) {
		/* get config properties from global property tree (or use default) */
		VPPP_PROPERTY_GET("SimpleDMA." + name(), "clock_cycle_period", sc_core::sc_time, prop_clock_cycle_period);

		tsock.register_b_transport(this, &SimpleDMA::transport);

		ext = new tlm_ext_initiator(this);  // tlm_generic_payload frees all extension objects in destructor, therefore
		                                    // dynamic allocation is needed
		trans.set_extension<tlm_ext_initiator>(ext);

		SC_THREAD(run);

		addr_to_reg = {
		    {SRC_ADDR, &src},
		    {DST_ADDR, &dst},
		    {LEN_ADDR, &len},
		    {OP_ADDR, &op},
		};
	}

	void _copy_block(uint32_t off, uint32_t n) {
		do_transaction(tlm::TLM_READ_COMMAND, src + off, &buffer[0], n);
		do_transaction(tlm::TLM_WRITE_COMMAND, dst + off, &buffer[0], n);
	}

	void _perform_memcpy() {
		auto n = len;
		uint32_t off = 0;

		while (n > buffer.size()) {
			_copy_block(off, buffer.size());
			n -= buffer.size();
			off += buffer.size();
		}

		_copy_block(off, n);
	}

	void run() {
		while (true) {
			sc_core::wait(run_event);

			switch (op) {
				case OP_NOP:
					break;

				case OP_MEMCPY:
					_perform_memcpy();
					break;

				case OP_MEMSET:
					assert(false && "not implemented");
					break;

				case OP_MEMCMP:
					assert(false && "not implemented");
					break;

				case OP_MEMCHR:
					assert(false && "not implemented");
					break;

				case OP_MEMMOVE:
					assert(false && "not implemented");
					break;

				default:
					assert(false && "unknown operation requested by software");
			}

			plic->gateway_trigger_interrupt(irq_number);
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();

		assert(len == DMA_REG_SIZE);  // NOTE: only allow to read/write whole register

		auto it = addr_to_reg.find(addr);
		assert(it != addr_to_reg.end());  // access to non-mapped address

		// actual read/write
		if (cmd == tlm::TLM_READ_COMMAND) {
			*((DMA_REG_TYPE *)ptr) = *it->second;
		} else if (cmd == tlm::TLM_WRITE_COMMAND) {
			*it->second = *((DMA_REG_TYPE *)ptr);
		} else {
			assert(false && "unsupported tlm command for dma access");
		}

		// post read/write actions
		if ((cmd == tlm::TLM_WRITE_COMMAND) && (addr == OP_ADDR)) {
			run_event.notify(prop_clock_cycle_period);
		}

		(void)delay;  // zero delay
	}

	void do_transaction(tlm::tlm_command cmd, uint64_t addr, uint8_t *data, unsigned num_bytes) {
		sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

		trans.set_command(cmd);
		trans.set_address(addr);
		trans.set_data_ptr(data);
		trans.set_data_length(num_bytes);

		isock->b_transport(trans, delay);

		if (delay != sc_core::SC_ZERO_TIME)
			sc_core::wait(delay);
	}

	void halt() override {
		std::cerr << "[VP] cannot halt initiator " << this->name() << std::endl;
	}

	std::string name() override {
		return sc_core::sc_module::name();
	}
};

#endif  // RISCV_ISA_DMA_H
