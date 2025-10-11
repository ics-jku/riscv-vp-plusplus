/*
 * Copyright (C) 2025 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 *
 * Property Map
 * A data structure which allows to set and retrieve hierarchical, model-wide
 * configuration properties (e.g. clock cycle periods).
 * Properties can be added and retrieved using hierarchical descriptors ("model.class.instance") and names.
 * On retrieval, the data structure walks down the hierarchy to find a match.
 * This makes it possible e.g. to define generic properties for multiple instances, classes, etc.
 *
 * Examples:
 * 1. Direct match
 *  * Add property (top-level) "vppp.ISS.core0.clock_cycle_period" = 10ns
 *  * Get property (modules) "vppp.ISS.core0.clock_cycle_period" -> 10ns
 *
 * 2. Generic match 1
 *  * Add property (top-level) "vppp.ISS.clock_cycle_period" = 10ns
 *  * Get property (modules) "vppp.ISS.core0.clock_cycle_period"
 *    * check "vppp.ISS.core0.clock_cycle_period" -> no match
 *    * check "vppp.ISS.clock_cycle_period" -> match -> 10ns
 *    * return 10ns
 *
 * 2. Generic match 2
 *  * Add property (top-level) "vppp.clock_cycle_period" = 10ns
 *  * Get property (module) "vppp.ISS.core0.clock_cycle_period"
 *    * check "vppp.ISS.core0.clock_cycle_period" -> no match
 *    * check "vppp.ISS.clock_cycle_period" -> no match
 *    * check "vppp.clock_cycle_period" -> match -> 10ns
 *    * return 10ns
 *
 * Practical examples:
 * 1. Set the clock cycle period for all modules to 20ns
 *  * Add property "vppp.clock_cycle_period" = 20ns
 *
 *  * Get property "vppp.ISS.core0.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 20ns
 *  * Get property "vppp.ISS.core1.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 20ns
 *  * Get property "vppp.ISS.core2.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 20ns
 *  * Get property "vppp.SimpleMemory.mem0.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 20ns
 *
 * 2. Set the clock cycle period for the core2 to 5ns and
 *    for all other cores and modules to 10ns.
 *  * Add property "vppp.clock_cycle_period" = 10ns
 *  * Add property "vppp.ISS.core2.clock_cycle_period" = 5ns
 *
 *  * Get property "vppp.ISS.core0.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 10ns
 *  * Get property "vppp.ISS.core1.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 10ns
 *  * Get property "vppp.ISS.core2.clock_cycle_period"
 *    -> match "vppp.ISS.core2.clock_cycle_period" -> 5ns (!)
 *  * Get property "vppp.SimpleMemory.mem0.clock_cycle_period"
 *    -> match "vppp.clock_cycle_period" -> 10ns
 *
 * For more examples see VP++ module and top-level implementations
 *
 * TODO/Ideas:
 *  * add properties if missing on get -> would be useful to create a
 *    property map from actual gets of a model (e.g. for validation)
 */
#ifndef RISCV_UTIL_PROPERTYMAP_H
#define RISCV_UTIL_PROPERTYMAP_H

#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <systemc>

#include "util/common.h"

class PropertyMap {
	/* static global map (automatically initialized) */
	static PropertyMap globalPropertyMap;

	std::map<std::string, std::string> pmap;
	bool debug = false;

	void set_raw(const std::string& desc, const std::string& name, const std::string& val, bool force = false);

	template <typename RET>
	RET get_raw(std::function<RET(std::string)> fconv, const std::string& desc, const std::string& name, bool def,
	            const RET& def_val) {
		std::string str = "";

		if (debug) {
			std::cout << "PropertyMap::get_raw: start: " << desc << "." << name << std::endl;
		}

		/* search */
		std::string cd = desc;
		while (true) {
			const auto it = pmap.find(cd + "." + name);
			if (it != pmap.end()) {
				str = it->second;
				if (debug) {
					std::cout << "PropertyMap::get_raw: match: " << cd << "." << name << " => " << str << std::endl;
				}
				break;
			}

			/* try next generic desc */
			size_t pos = cd.rfind(".");
			if (pos == std::string::npos) {
				/* no more desc */
				break;
			}
			cd = cd.substr(0, pos);
		}

		/* handle default */
		if (str.empty()) {
			/* if not found and default value given -> return default */
			if (def) {
				if (debug) {
					std::cout << "PropertyMap::get_raw: no-match, use default: " << desc << "." << name << " => "
					          << def_val << std::endl;
				}
				return def_val;
			}
			throw std::runtime_error("PropertyMap::get_raw: Missing property \"" + desc + "." + name + "\"");
		}

		/* handle conversion */
		try {
			return fconv(str);
		} catch (const std::invalid_argument& e) {
			throw std::runtime_error("PropertyMap::get_raw: Invalid value \"" + str + "\": not a valid number");
		} catch (const std::out_of_range& e) {
			throw std::runtime_error("PropertyMap::get_raw: Invalid value \"" + str +
			                         "\": number out of range for uint64_t");
		}
	}

	/* variant without default value */
	template <typename RET>
	RET get_raw(std::function<RET(std::string)> fconv, const std::string& desc, const std::string& name) {
		return get_raw(fconv, desc, name, false, RET());
	}

	/* variant with default value */
	template <typename RET>
	RET get_raw(std::function<RET(std::string)> fconv, const std::string& desc, const std::string& name, RET def_val) {
		return get_raw(fconv, desc, name, true, def_val);
	}

   public:
	static void init(const std::string& filename = "");
	static void deinit();
	static PropertyMap* global() {
		return &globalPropertyMap;
	}

	PropertyMap(const std::string& filename = "");

	bool is_debug() const {
		return debug;
	}

	void set_debug(bool debug) {
		this->debug = debug;
	}

	void clear();

	// Save the map to a JSON file
	void save_json(const std::string& filename) const;

	// Load the map from a JSON file
	void load_json(const std::string& filename);

	void dump(void) const;

	bool contains(const std::string& desc, const std::string& name) const {
		return pmap.count(desc + "." + name) > 0;
	}

	template <typename VAL>
	void set(const std::string& desc, const std::string& name, VAL val) {
		std::stringstream stream;

		if constexpr (std::is_same_v<VAL, std::string>) {
			stream << val;

		} else if constexpr (std::is_same_v<VAL, bool>) {
			stream << (val ? "true" : "false");

		} else if constexpr (std::is_same_v<VAL, uint64_t>) {
			stream << "0x" << std::hex << val;

		} else if constexpr (std::is_same_v<VAL, int64_t>) {
			stream << val;

		} else if constexpr (std::is_same_v<VAL, sc_core::sc_time>) {
			stream << val.value();

		} else {
			throw std::runtime_error(std::string("PropertyMap::set: Not implemented for type ") + typeid(VAL).name());
		}

		set_raw(desc, name, stream.str());
	}

	template <typename RET, typename... Args>
	RET get(Args... args) {
		std::function<RET(std::string)> fconv;

		if constexpr (std::is_same_v<RET, std::string>) {
			fconv = [](const std::string& str) -> std::string { return str; };

		} else if constexpr (std::is_same_v<RET, bool>) {
			fconv = [](const std::string& str) -> bool {
				if (!str.compare("false")) {
					return false;
				} else if (!str.compare("true")) {
					return true;
				} else {
					throw std::runtime_error("PropertyMap::get: Invalid value \"" + str +
					                         "\": not \"true\" or \"false\"");
				}
			};

		} else if constexpr (std::is_same_v<RET, uint64_t>) {
			fconv = [](const std::string& str) -> int64_t { return std::stoll(str, nullptr, 0); };

		} else if constexpr (std::is_same_v<RET, int64_t>) {
			fconv = [](const std::string& str) -> int64_t { return std::stoll(str, nullptr, 0); };

		} else if constexpr (std::is_same_v<RET, sc_core::sc_time>) {
			fconv = [](const std::string& str) -> sc_core::sc_time {
				return sc_core::sc_time(std::stoll(str, nullptr, 0), sc_core::SC_PS);
			};

		} else {
			throw std::runtime_error(std::string("PropertyMap::get: Not implemented for type ") + typeid(RET).name());
		}

		return get_raw<RET>(fconv, args...);
	}

	/* vp++ related -> could be factored out in a subclass / include file */

	/* helper */
#define VPPP_PROPERTY__GEN_FULL_DESC_STR(_desc)                            \
	(std::string("vppp") + ((std::string(std::string("") + _desc).empty()) \
	                            ? ""                                       \
	                            : (std::string(".") + std::string(std::string("") + _desc))))
	/* api */

#define VPPP_PROPERTY_GET(_desc, _property_name, _type, _property) \
	(_property) =                                                  \
	    PropertyMap::global()->get<_type>(VPPP_PROPERTY__GEN_FULL_DESC_STR(_desc), (_property_name), (_property))

#define VPPP_PROPERTY_SET(_desc, _property_name, _type, _val) \
	PropertyMap::global()->set<_type>(VPPP_PROPERTY__GEN_FULL_DESC_STR(_desc), (_property_name), (_val))
};

#endif /* RISCV_UTIL_PROPERTYMAP_H */
