#include "propertytree.h"

PropertyTree PropertyTree::globalPropertyTree = PropertyTree();

PropertyTree::PropertyTree(const std::string& filename) {
	if (filename != "") {
		load_json(filename);
	} else {
		pmap.clear();
	}
}

void PropertyTree::clear() {
	pmap.clear();
}

void PropertyTree::save_json(const std::string& filename) const {
	nlohmann::json jsonObj(pmap);  // Convert the map to a JSON object
	std::ofstream outFile(filename);
	if (!outFile) {
		throw std::runtime_error("PropertyTree::save_json: Error open \"" + filename + "\"");
	}
	outFile << jsonObj.dump(4);  // Pretty-print JSON with 4 spaces indentation
}

void PropertyTree::load_json(const std::string& filename) {
	std::ifstream inFile(filename);
	if (!inFile) {
		throw std::runtime_error("PropertyTree::load_json: Error open \"" + filename + "\"");
	}
	nlohmann::json jsonObj;
	inFile >> jsonObj;                                         // Parse the JSON file
	pmap = jsonObj.get<std::map<std::string, std::string>>();  // Convert JSON back to a map
}

void PropertyTree::dump(void) const {
	std::cout << "[KEY] = VALUE" << "\n";
	for (const auto& [key, value] : pmap) {
		std::cout << '[' << key << "] = " << value << "\n";
	}
	std::cout << std::endl;
}

void PropertyTree::set_raw(const std::string& desc, const std::string& name, const std::string& val, bool force) {
	if (desc.empty() || name.empty() || val.empty()) {
		throw std::runtime_error("PropertyTree::set_raw: Invalid argument(s): empty (\"\") desc, name or val");
	}
	if (contains(desc, name)) {
		throw std::runtime_error("PropertyTree::set_raw: Duplicate property \"" + desc + "." + name + "\"");
	}
	pmap.insert_or_assign(desc + "." + name, val);

	if (debug) {
		std::cout << "PropertyTree::set_raw: " << desc << "." << name << " <= " << val << std::endl;
		dump();
	}
}
