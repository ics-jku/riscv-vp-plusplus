#pragma once

#include <functional>
#include <vector>

#include "common.h"

struct RegisterRange {
	struct WriteInfo {
		uint64_t addr;
		size_t size;
		tlm::tlm_generic_payload &trans;
		sc_core::sc_time &delay;
	};

	struct ReadInfo {
		uint64_t addr;
		size_t size;
		tlm::tlm_generic_payload &trans;
		sc_core::sc_time &delay;
	};

	typedef std::function<bool(WriteInfo)> PreWriteCallback;
	typedef std::function<void(WriteInfo)> PostWriteCallback;
	typedef std::function<bool(ReadInfo)> PreReadCallback;
	typedef std::function<void(ReadInfo)> PostReadCallback;

	uint64_t start;
	uint64_t end;
	std::vector<uint8_t> mem;
	bool readonly = false;
	uint64_t alignment = 1;

	PreWriteCallback pre_write_callback;
	PostWriteCallback post_write_callback;
	PreReadCallback pre_read_callback;
	PostReadCallback post_read_callback;

	RegisterRange(uint64_t start, uint64_t size) : start(start) {
		assert(size > 0);

		end = start + size - 1;
		assert(end >= start);

		mem = std::vector<uint8_t>(size);
	}

	static RegisterRange make_start_end(uint64_t start, uint64_t end) {
		assert(end >= start);
		return {start, end - start + 1};
	}

	static RegisterRange make_start_size(uint64_t start, uint64_t size) {
		assert(size > 0);
		return {start, size};
	}

	template <typename T>
	static RegisterRange make_array(uint64_t start, uint64_t num_elems) {
		static_assert(std::is_integral<T>::value, "integer type required");

		assert(num_elems > 0);
		return {start, sizeof(T) * num_elems};
	}

	virtual bool contains(uint64_t addr) {
		return addr >= start && addr <= end;
	}

	virtual uint64_t to_local(uint64_t addr) {
		return addr - start;
	}

	virtual void assert_range(size_t len, uint64_t local_addr) {
		assert(local_addr + len <= mem.size());
	}

	void write(uint64_t addr, const uint8_t *src, size_t len, tlm::tlm_generic_payload &trans,
	           sc_core::sc_time &delay) {
		assert(contains(addr));

		auto local_addr = to_local(addr);
		assert_range(len, local_addr);

		if (pre_write_callback)
			if (!pre_write_callback({local_addr, len, trans, delay}))
				return;

		memcpy(mem.data() + local_addr, src, len);

		if (post_write_callback)
			post_write_callback({local_addr, len, trans, delay});
	}

	void read(uint64_t addr, uint8_t *dst, size_t len, tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		assert(contains(addr));

		auto local_addr = to_local(addr);
		assert_range(len, local_addr);

		if (pre_read_callback)
			if (!pre_read_callback({local_addr, len, trans, delay}))
				return;

		memcpy(dst, mem.data() + local_addr, len);

		if (post_read_callback)
			post_read_callback({local_addr, len, trans, delay});
	}

	bool match(tlm::tlm_generic_payload &trans) {
		return contains(trans.get_address());
	}

	void process(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		auto addr = trans.get_address();
		auto cmd = trans.get_command();
		auto len = trans.get_data_length();
		auto ptr = trans.get_data_ptr();

		ensure((addr % alignment == 0) && (len % alignment == 0));

		if (cmd == tlm::TLM_READ_COMMAND) {
			read(addr, ptr, len, trans, delay);
		} else if (cmd == tlm::TLM_WRITE_COMMAND) {
			ensure(!readonly);
			write(addr, ptr, len, trans, delay);
		} else {
			throw std::runtime_error("unsupported TLM command");
		}
	}
};

template <typename T>
struct IntegerView {
	static_assert(std::is_integral<T>::value, "integer type required");

	T *ptr;

	IntegerView(RegisterRange &r) {
		assert(((uintptr_t)r.mem.data()) % sizeof(T) == 0);
		assert(r.mem.size() >= sizeof(T));
		ptr = reinterpret_cast<T *>(r.mem.data());
	}

	T read() const {
		return *ptr;
	}

	void write(T value) {
		*ptr = value;
	}

	operator T() const {
		return read();
	}

	IntegerView<T> &operator=(uint64_t value) {
		write(value);
		return *this;
	}
};

template <typename T, unsigned RowSize = 1>
struct ArrayView {
	static_assert((std::is_trivial_v<T> || std::is_integral_v<T>), "integral or trivial type required");

	RegisterRange &regs;
	T *ptr;
	const size_t size;

	ArrayView(RegisterRange &r) : regs(r), size(r.mem.size() / sizeof(T)) {
		assert(r.mem.size() % sizeof(T) == 0);
		assert(size > 0);

		ptr = reinterpret_cast<T *>(r.mem.data());
	}

	T &at(size_t idx) {
		if (idx >= size)
			throw std::out_of_range("ArrayView index out-of-range");
		else
			return ptr[idx];
	}

	T &operator[](unsigned idx) {
		assert(idx < size);
		return ptr[idx];
	}

	// use for 2d array access
	T &operator()(unsigned idx, unsigned row_idx) {
		return ptr[idx * RowSize + row_idx];
	}

	T *begin() {
		return &ptr[0];
	}
	const T *begin() const {
		return &ptr[0];
	}
	T *end() {
		return &ptr[size - 1];
	}
	const T *end() const {
		return &ptr[size - 1];
	}
};

namespace vp {
namespace mm {

template <typename Iter>
void route(const char *name, Iter &iterable_mm, tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	for (auto e : iterable_mm) {
		if (e->match(trans)) {
			e->process(trans, delay);
			return;
		}
	}

	throw std::runtime_error(std::string(name) + " unable to route address " + std::to_string(trans.get_address()));
}

}  // namespace mm
}  // namespace vp
