#pragma once

#include <gpio/gpio-client.hpp>

uint64_t translateGpioToExtPin(gpio::State reg);
gpio::PinNumber translatePinToGpioOffs(gpio::PinNumber pin);
