#pragma once

#include <systemc>
#include <tlm>

#include "initiator_if.h"

class tlm_ext_initiator : public tlm::tlm_extension<tlm_ext_initiator> {
   public:
	initiator_if *initiator = nullptr;

	tlm_ext_initiator(initiator_if *init) : initiator(init) {}

	tlm::tlm_extension_base *clone() const override {
		return new tlm_ext_initiator(*this);
	}

	void copy_from(tlm::tlm_extension_base const &that) override {
		*this = static_cast<const tlm_ext_initiator &>(that);
	}
};
