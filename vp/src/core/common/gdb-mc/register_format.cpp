#include "register_format.h"

#include <byteswap.h>

#include <iomanip>

RegisterFormater::RegisterFormater(Architecture arch) {
	this->arch = arch;
	this->stream << std::setfill('0') << std::hex;
}

void RegisterFormater::formatRegister(uint64_t value) {
	switch (arch) {
		case RV32:
			stream << std::setw(8) << bswap_32(value);
			break;
		case RV64:
			stream << std::setw(16) << bswap_64(value);
			break;
		default:
			throw std::invalid_argument("Architecture not implemented");
	}
}

std::string RegisterFormater::str(void) {
	return this->stream.str();
}
