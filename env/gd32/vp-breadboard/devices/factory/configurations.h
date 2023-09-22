#pragma once

#include <stdexcept>

#include "devices/configurations.h"

class device_not_found_error : std::runtime_error {
   public:
	device_not_found_error(DeviceClass name) : std::runtime_error("Device " + name + " could not be found") {}
};
