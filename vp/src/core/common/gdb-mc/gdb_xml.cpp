#include "gdb_xml.h"

#include <stdexcept>
#include <string>

namespace gdb_xml {

const char *target_file(const TargetConfig &cfg) {
	if (cfg.xlen == 32 && !cfg.cheri)
		return cfg.embedded ? "target-rv32e.xml" : "target-rv32.xml";
	if (cfg.xlen == 64 && !cfg.cheri)
		return cfg.embedded ? "target-rv64e.xml" : "target-rv64.xml";
	if (cfg.xlen == 64 && cfg.cheri && !cfg.embedded)
		return "target-rv64y.xml";

	throw std::invalid_argument("GDB: no target description for RV" + std::to_string(cfg.xlen) +
	                            (cfg.embedded ? "E" : "I") + (cfg.cheri ? "Y" : ""));
}

std::string_view lookup(const TargetConfig &cfg, std::string_view annex) {
	if (annex == "target.xml")
		annex = target_file(cfg);

	for (size_t i = 0; i < num_files; i++) {
		if (annex == files[i].name)
			return files[i].content;
	}
	return {};
}

}  // namespace gdb_xml
