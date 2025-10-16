#pragma once

#include <systemc>
#include <tlm>

class tlm_ext_tag : public tlm::tlm_extension<tlm_ext_tag> {
   public:
	bool tag;

	tlm_ext_tag(bool tag) : tag(tag) {}

	tlm::tlm_extension_base *clone() const override {
		return new tlm_ext_tag(*this);
	}

	void copy_from(tlm::tlm_extension_base const &ext) override {
		tag = static_cast<tlm_ext_tag const &>(ext).tag;
	}
};
