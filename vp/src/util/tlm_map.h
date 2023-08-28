#ifndef RISCV_TLM_MAP_H
#define RISCV_TLM_MAP_H

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <boost/format.hpp>
#include <functional>
#include <systemc>
#include <unordered_map>

/*
 * Optional modelling layer to simplify TLM register and memory access.
 * sensor2.h demonstrates how to use it.
 */

namespace vp {
namespace map {

struct access_mode {
	bool allow_read = true;
	bool allow_write = true;

	static access_mode make_writeonly() {
		return access_mode({false, true});
	}

	static access_mode make_readonly() {
		return access_mode({true, false});
	}

	bool can_read() {
		return allow_read;
	}

	bool can_write() {
		return allow_write;
	}

	bool is_readonly() {
		return allow_read && !allow_write;
	}
};

constexpr access_mode read_write = {true, true};
constexpr access_mode read_only = {true, false};
constexpr access_mode write_only = {false, true};

struct AbstractMapping {
	virtual ~AbstractMapping() {}

	virtual bool try_handle(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) = 0;
};

inline void execute_memory_access(tlm::tlm_generic_payload &trans, uint8_t *local_memory) {
	if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
		memcpy(&local_memory[trans.get_address()], trans.get_data_ptr(), trans.get_data_length());
	} else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
		memcpy(trans.get_data_ptr(), &local_memory[trans.get_address()], trans.get_data_length());
	} else {
		throw std::runtime_error("unsupported TLM command detected");
	}
}

struct AddressMapping : public AbstractMapping {
	typedef std::function<void(tlm::tlm_generic_payload &, sc_core::sc_time &)> fn_transport_t;

	uint64_t start;
	uint64_t end;
	access_mode mode;
	fn_transport_t handler;

	template <typename Module, typename MemberFun>
	AddressMapping &register_handler(Module *this_, MemberFun fn) {
		assert(!handler);
		handler = std::bind(fn, this_, std::placeholders::_1, std::placeholders::_2);
		return *this;
	}

	AddressMapping &register_handler(AddressMapping::fn_transport_t fn) {
		assert(!handler);
		handler = fn;
		return *this;
	}

	bool try_handle(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto len = trans.get_data_length();
		auto cmd = trans.get_command();

		if (addr >= start && addr < end) {
			assert((addr + len <= end) && "memory out of bounds access");
			assert(mode.can_read() || cmd != tlm::TLM_READ_COMMAND);
			assert(mode.can_write() || cmd != tlm::TLM_WRITE_COMMAND);

			trans.set_address(addr - start);

			handler(trans, delay);
			return true;
		}
		return false;
	}
};

struct reg_mapping_t {
	uint64_t addr;
	uint32_t *vptr;
	access_mode mode = read_write;
	uint32_t mask = 0xffffffff;

	uint32_t value() {
		return *vptr;
	}

	void bus_write(uint32_t new_value) {
		assert(mode.can_write());
		*vptr = new_value & mask;
	}

	uint32_t bus_read() {
		assert(mode.can_read());
		return *vptr;
	}
};

struct register_access_t {
	typedef std::function<void()> callback_t;

	/*
	 * vptr points to the actual register.
	 * nv is the new value that will be assigned to the register (only valid for
	 * write access). calling fn will perform the actual read/write operation.
	 */
	bool read;
	bool write;
	uint32_t *vptr;
	uint32_t nv;
	callback_t fn;
	sc_core::sc_time &delay;
	uint64_t addr;
};

struct RegisterMapping : public AbstractMapping {
	typedef std::function<void()> callback_t;
	typedef std::function<void(const register_access_t &)> handler_t;

	std::unordered_map<uint64_t, reg_mapping_t> addr_to_reg;
	handler_t handler;

	RegisterMapping &add_register(reg_mapping_t m) {
		addr_to_reg.insert(std::make_pair(m.addr, m));
		return *this;
	}

	template <typename Module, typename MemberFun>
	RegisterMapping &register_handler(Module *this_, MemberFun fn) {
		assert(!handler);
		handler = std::bind(fn, this_, std::placeholders::_1);
		return *this;
	}

	RegisterMapping &register_handler(RegisterMapping::handler_t fn) {
		assert(!handler);
		handler = fn;
		return *this;
	}

	bool try_handle(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto new_vptr = (uint32_t *)trans.get_data_ptr();
		auto len = trans.get_data_length();
		auto cmd = trans.get_command();

		auto it = addr_to_reg.find(addr - addr % 4);  // clamp to nearest register
		if (it == addr_to_reg.end())
			return false;

		assert(len + (addr % 4) <= 4);  // do not allow access beyond the register
		assert(cmd == tlm::TLM_READ_COMMAND || cmd == tlm::TLM_WRITE_COMMAND);

		reg_mapping_t &r = it->second;

		assert(r.mode.can_read() || cmd != tlm::TLM_READ_COMMAND);
		assert(r.mode.can_write() || cmd != tlm::TLM_WRITE_COMMAND);

		auto fn = [cmd, &r, new_vptr, &trans]() {
			(void)new_vptr;
			auto off = trans.get_address() % 4;
			if (cmd == tlm::TLM_READ_COMMAND) {
				uint32_t n = r.bus_read();
				memcpy(trans.get_data_ptr(), ((uint8_t *)&n) + off, trans.get_data_length());
				//*new_vptr = r.bus_read();
			} else if (cmd == tlm::TLM_WRITE_COMMAND) {
				uint32_t n = r.value();
				memcpy(((uint8_t *)&n) + off, trans.get_data_ptr(), trans.get_data_length());
				r.bus_write(n);
				// r.bus_write(*new_vptr);
			} else {
				throw std::runtime_error("unsupported TLM command detected");
			}
		};

		assert(handler && "no callback function provided");

		// introduce *nv* to get rid of "use of uninitialized value" warnings
		uint32_t nv = 0;
		if (cmd == tlm::TLM_WRITE_COMMAND)
			nv = *new_vptr;

		handler({cmd == tlm::TLM_READ_COMMAND, cmd == tlm::TLM_WRITE_COMMAND, r.vptr, nv, fn, delay, addr});
		return true;
	}
};

struct LocalRouter {
	std::string name;
	std::vector<AbstractMapping *> maps;

	LocalRouter(const std::string &name = "unamed") : name(name) {}

	~LocalRouter() {
		for (auto p : maps) {
			assert(p);
			delete p;
		}
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		for (auto &m : maps) {
			if (m->try_handle(trans, delay))
				return;
		}
		throw std::runtime_error("access of unmapped address (local TLM router): name=" + name + ", addr=0x" +
		                         (boost::format("%X") % trans.get_address()).str());
	}

	RegisterMapping &add_register_bank(const std::vector<reg_mapping_t> &regs) {
		auto p = new RegisterMapping();
		for (auto &m : regs) {
			assert(p->addr_to_reg.find(m.addr) == p->addr_to_reg.end() && "register at this address already available");
			p->addr_to_reg.insert(std::make_pair(m.addr, m));
		}
		maps.push_back(p);
		return *p;
	}

	AddressMapping &add_start_end_mapping(uint64_t start, uint64_t end, const access_mode &m) {
		auto p = new AddressMapping();
		p->start = start;
		p->end = end;
		p->mode = m;
		maps.push_back(p);
		return *p;
	}

	AddressMapping &add_start_size_mapping(uint64_t start, uint32_t size, const access_mode &m) {
		return add_start_end_mapping(start, start + size, m);
	}
};

}  // namespace map
}  // namespace vp

#endif  // RISCV_TLM_MAP_H
