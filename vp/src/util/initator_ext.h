#pragma once

#include "initiator_if.h"

class initiator_ext : public tlm::tlm_extension<initiator_ext> {
   public:
	initiator_if *initiator = nullptr;

	initiator_ext(initiator_if *init) : initiator(init) {}

	tlm::tlm_extension_base *clone() const {
		return new initiator_ext(*this);
	}

	void copy_from(tlm::tlm_extension_base const &that) {
		*this = static_cast<const initiator_ext &>(that);
	}
};