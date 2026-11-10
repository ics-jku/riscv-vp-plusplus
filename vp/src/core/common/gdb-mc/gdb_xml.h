#ifndef GDB_XML_H
#define GDB_XML_H

#include <cstddef>
#include <string_view>

/*
 * GDB target descriptions (served via qXfer:features:read)
 *
 * The descriptions are stored as XML files in gdb-mc/xml/ and embedded into the binary at build time
 * (see embed_xml.cmake). The register numbers used in the XML files must match gdb_regs.h.
 */
namespace gdb_xml {

struct XmlFile {
	const char *name;
	std::string_view content;
};

// generated: gdb_xml_data.cpp
extern const XmlFile files[];
extern const size_t num_files;

// Describes the debugged target, selects the top level target description
struct TargetConfig {
	unsigned xlen;  // 32 or 64
	bool embedded;  // RV E base ISA (x0-x15 only)
	bool cheri;     // RVY extension

	bool operator==(const TargetConfig &o) const {
		return xlen == o.xlen && embedded == o.embedded && cheri == o.cheri;
	}
	bool operator!=(const TargetConfig &o) const {
		return !(*this == o);
	}
};

// Name of the top level target description file for the given config, throws std::invalid_argument if unsupported
const char *target_file(const TargetConfig &cfg);

// Content of the requested annex ("target.xml" is mapped to target_file(cfg)), empty if not found
std::string_view lookup(const TargetConfig &cfg, std::string_view annex);

}  // namespace gdb_xml

#endif
