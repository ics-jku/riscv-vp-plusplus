/*
 * oled.hpp
 *
 *  Created on: 20 Sep 2019
 *      Author: dwd
 *
 * This class models the SH1106 oled Display driver.
 */

#pragma once
#include "common.hpp"

#include <map>
#include <functional>

class SS1106 {
public:
	typedef std::function<bool()> GetDCPin_function;
private:

	static const std::map<ss1106::Operator, uint8_t> opcode;

	struct Command
	{
		ss1106::Operator op;
		uint8_t payload;
	};

	enum class Mode : uint_fast8_t
	{
		normal,
		second_arg
	} mode = Mode::normal;

	void *sharedSegment = nullptr;
	ss1106::State* state;

	Command last_cmd = Command{ss1106::Operator::NOP, 0};

	GetDCPin_function getDCPin;


	uint8_t mask(ss1106::Operator op);
	Command match(uint8_t cmd);

public:
	SS1106(GetDCPin_function getDCPin, ss1106::State* state_memory_override = nullptr);
	~SS1106();

	uint8_t write(uint8_t byte);
};
