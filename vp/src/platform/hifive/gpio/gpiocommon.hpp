/*
 * gpio.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <limits>       // std::numeric_limits

void hexPrint(unsigned char* buf, size_t size);
void bitPrint(unsigned char* buf, size_t size);

namespace gpio {
	enum class Tristate : uint8_t {
		LOW = 0,
		HIGH,
		UNSET,

		IOF_SPI = 4,
		IOF_I2C,	// not yet used
		IOF_PWM,	// planned to be used
		IOF_UART,	// not yet used
	};

	bool isIOF(const Tristate s);

	static constexpr unsigned default_port = 1400;
	static constexpr unsigned max_num_pins = 64;

	typedef uint8_t PinNumber;
	static_assert(std::numeric_limits<PinNumber>::max() >= max_num_pins);

	typedef uint8_t SPI_Command;
	typedef uint8_t SPI_Response;

	struct State {
		//TODO somehow packed?
		gpio::Tristate pins[max_num_pins];
	};

	struct Request {
		enum class Type : uint8_t {
			GET_BANK = 1,
			SET_BIT,
			REQ_IOF,
			REQ_LOGSTATE = REQ_IOF,
			END_IOF,
			END_LOGSTATE = END_IOF
		} op;
		union {
			struct {
				uint8_t pin : 6;	// max num pins: 64
				gpio::Tristate val : 2;
			} setBit;

			struct {
				// Todo: Decide how to determine SPI's Chip Select
				// Perhaps pin shall be one of the hardware CS pins
				uint8_t pin;
			} reqIOF;
		};
	};

	struct Req_IOF_Response {
		uint16_t port = 0;	// zero is error condition
	};
};

struct GpioCommon {

	static void printRequest(const gpio::Request& req);
	static void printState(const gpio::State& state);

	gpio::State state;

	GpioCommon();
};
