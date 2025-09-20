#ifndef RISCV_VP_DUMMY_I2C_H
#define RISCV_VP_DUMMY_I2C_H

#pragma once

#include "i2c_if.h"

#define NOT_BUSY 0
#define START_RECEIVED 1

class DummyI2C : public I2C_Device_IF {
	uint8_t reg;
	uint8_t start_signal;

   protected:
	bool start() override;
	bool write(uint8_t data) override;
	bool read(uint8_t &data) override;
	bool stop() override;

   public:
	DummyI2C();
};

DummyI2C::DummyI2C() {
	start_signal = NOT_BUSY;
	reg = 0;
}

bool DummyI2C::start() {
	start_signal = START_RECEIVED;
	return true;
}

bool DummyI2C::write(uint8_t data) {
	if (start_signal == START_RECEIVED) {
		reg = data;
		return true;
	}
	return false;
}

bool DummyI2C::read(uint8_t &data) {
	if (start_signal == START_RECEIVED) {
		data = reg;
		return true;
	}
	return false;
}

bool DummyI2C::stop() {
	start_signal = NOT_BUSY;
	return true;
}

#endif  // RISCV_VP_DUMMY_I2C_H
