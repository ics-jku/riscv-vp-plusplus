#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

template <typename T>
struct OptionValue {
	bool available = false;
	T value{};
	std::string option;

	bool finalize(std::function<T(const std::string &)> parser) {
		if (!option.empty()) {
			value = parser(option);
			available = true;
		}
		return available;
	}
};

uint64_t parse_uint64_option(const std::string &s);
