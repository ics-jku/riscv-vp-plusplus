/*
 * gpio.hpp
 *
 *  Created on: 7 Nov 2018
 *      Author: dwd
 */

#pragma once

#include <inttypes.h>
#include <stddef.h>


void hexPrint(unsigned char* buf, size_t size);
void bitPrint(unsigned char* buf, size_t size);

struct GpioCommon {
	static constexpr unsigned default_port = 1400;
	static constexpr unsigned max_num_pins = 64;

	typedef uint64_t Reg;

	enum class Tristate : uint8_t {
		LOW = 0,
		HIGH,
		IOF,
		UNSET
	};

	struct State {
		//TODO
		// somehow packed
		Tristate pins[max_num_pins];
	};

	struct Response {
		enum class Type : uint8_t {
			ICH_WEIS_AUCH_NICHT,
			DAS_KOMMT_VON_ALLEINEEE
		};
	};

	struct Request {
		enum class Type : uint8_t {
			GET_BANK = 1,
			SET_BIT,
			REQ_LOGSTATE,
			END_LOGSTATE
		} op;
		union {
			struct {
				uint8_t pin : 6;	// max num pins: 64
				Tristate val : 2;
			} setBit;

			struct
			{
				uint8_t pin;
			} reqLog;
		};
	};

	Reg state;
	void printRequest(Request* req);
	GpioCommon();
};
