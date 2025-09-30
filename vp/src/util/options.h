#pragma once

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

unsigned long parse_ulong_option(const std::string &s);
