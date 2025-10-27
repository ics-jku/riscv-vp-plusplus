#include "cheri_cap_common.h"

std::string uint128_to_string(__uint128_t data) {
	std::string result;
	for (int i = 127; i >= 0; i--) {
		result += std::to_string(static_cast<uint8_t>((data >> i) & 1));
	}
	return result;
}

std::string capToString(Capability cap) {
	std::string len_str = uint128_to_string(cap.getLength());
	std::string otype64 = cap.hasReservedOType() ? std::bitset<cCapOTypeWidth>(cap.fields.otype).to_string()
	                                             : std::bitset<cCapOTypeWidth>(cap.fields.otype).to_string();
	std::string perms = std::bitset<cCapPermsWidth>(cap.getCapPerms()).to_string();
	std::string result = "Capability:\n    t:       0b" + std::to_string(cap.fields.tag) + "\n    sealed:  0b" +
	                     std::to_string(cap.isSealed()) + "\n    perms:   0b" + perms + "\n    type:    0b" + otype64 +
	                     "\n    flag_cap_mode: " + std::to_string(cap.fields.flag_cap_mode) + "\n    address: 0b" +
	                     std::bitset<cCapAddrWidth>(cap.fields.address).to_string() + "(" +
	                     std::to_string(cap.fields.address) + ")" + "\n    base:    0b" +
	                     std::bitset<cCapAddrWidth>(cap.getBase()).to_string() + "(" + std::to_string(cap.getBase()) +
	                     ")" + "\n    length:  0b" + len_str;

	return result;
}

std::string capToShortString(Capability cap) {
	std::string len_str = uint128_to_string(cap.getLength());
	std::string otype64 = cap.hasReservedOType() ? std::bitset<cCapOTypeWidth>(cap.fields.otype).to_string()
	                                             : std::bitset<cCapOTypeWidth>(cap.fields.otype).to_string();
	std::string perms = std::bitset<cEncCapPermsWidth>(cap.getCapPerms()).to_string();
	std::string result = std::to_string(cap.fields.tag) + " " + std::to_string(cap.isSealed()) + " " + perms + " " +
	                     otype64 + " " + std::to_string(cap.getBase()) + " " + len_str + " " +
	                     std::to_string(cap.fields.address);
	return result;
}