#include "options.h"

uint64_t parse_uint64_option(const std::string &s) {
	bool is_hex = false;
	if (s.size() >= 2) {
		if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X')))
			is_hex = true;
	}

	try {
		if (is_hex)
			return stoull(s, 0, 16);
		return stoull(s);
	} catch (std::exception &e) {
		throw std::runtime_error(std::string("unable to parse option '") + s + "' into a number");
	}
}
