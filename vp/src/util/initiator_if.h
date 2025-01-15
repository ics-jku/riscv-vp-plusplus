#pragma once

struct initiator_if {
   public:
	virtual ~initiator_if() {}

	virtual std::string name() = 0;
	virtual void halt() = 0;
};