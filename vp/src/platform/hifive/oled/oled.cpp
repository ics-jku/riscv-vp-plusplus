/*
 * oled.cpp
 *
 *  Created on: 20 Sep 2019
 *      Author: dwd
 */


#include <platform/hifive/oled/oled.hpp>
#include <cstdio>
#include <exception>
#include <cstring>
#include <sys/types.h>
#include <sys/shm.h>

using namespace ss1106;

const std::map<ss1106::Operator, uint8_t> SS1106::opcode =
{
	{ss1106::Operator::COL_LOW, 0x00},
	{ss1106::Operator::COL_HIGH, 0x10},
	{ss1106::Operator::PUMP_VOLTAGE, 0b00110000},
	{ss1106::Operator::DISPLAY_START_LINE, 0b01000000},
	{ss1106::Operator::CONTRAST_MODE_SET, 0b10000001},
	{ss1106::Operator::SEGMENT_REMAP, 0xA0},
	{ss1106::Operator::ENTIRE_DISPLAY, 0xA4},
	{ss1106::Operator::NORMAL_INVERSE, 0xA6},
	{ss1106::Operator::MULTIPLEX_RATIO, 0b10101000},
	{ss1106::Operator::DC_DC_VOLTAGE, 0x8B},
	{ss1106::Operator::DISPLAY_ON, 0xAE},
	{ss1106::Operator::PAGE_ADDR, 0xB0},
	{ss1106::Operator::COMMON_OUTPUT_DIR, 0xC0},
	{ss1106::Operator::DISPLAY_OFFSET, 0b11010011},
	{ss1106::Operator::DISPLAY_DIVIDE_RATIO, 0b11010101},
	{ss1106::Operator::DIS_PRE_CHARGE_PERIOD, 0b11011001},
	{ss1106::Operator::COMMON_PADS_STUFF, 0b11011010},
	{ss1106::Operator::VCOM_DESELECT, 0b11011011},
	{ss1106::Operator::RMW, 0b11100000},
	{ss1106::Operator::RMW_END, 0b11101110},
	{ss1106::Operator::NOP, 0b11100011},
};

uint8_t SS1106::mask(Operator op)
{
	switch(op)
	{
	case Operator::DISPLAY_START_LINE:
		return 0b11000000;
	case Operator::COL_LOW:
	case Operator::COL_HIGH:
	case Operator::PAGE_ADDR:
	case Operator::COMMON_OUTPUT_DIR:
		return 0b11110000;
	case Operator::PUMP_VOLTAGE:
		return 0b11111100;
	case Operator::SEGMENT_REMAP:
	case Operator::ENTIRE_DISPLAY:
	case Operator::NORMAL_INVERSE:
	case Operator::DISPLAY_ON:
		return 0b11111110;
	case Operator::CONTRAST_MODE_SET:
	case Operator::MULTIPLEX_RATIO:
	case Operator::DC_DC_VOLTAGE:
	case Operator::DISPLAY_OFFSET:
	case Operator::DISPLAY_DIVIDE_RATIO:
	case Operator::DIS_PRE_CHARGE_PERIOD:
	case Operator::COMMON_PADS_STUFF:
	case Operator::VCOM_DESELECT:
	case Operator::RMW:
	case Operator::RMW_END:
	case Operator::NOP:
		return 0b11111111;
	}

	return 0xff;
}

SS1106::Command SS1106::match(uint8_t cmd)
{
	//printf("CMD: %02X\n", cmd);
	for(uint16_t opu = 0; opu < *Operator(); opu++)
	{
		Operator op = static_cast<Operator>(opu);		//Bwah, ugly
		//printf("%d: \tMask %02X, opcode %02X %s\n", opu, mask(op), opcode[op], (~Operator(op)).c_str());
		if(((cmd ^ opcode.at(op)) & mask(op)) == 0)
			return Command{op, static_cast<uint8_t>(cmd & ~mask(op))};
	}
	return Command{Operator::NOP, 0};
}

SS1106::SS1106(std::function<bool()> getDCPin, ss1106::State* state_memory_override) : getDCPin(getDCPin)
{
	if(!state_memory_override) {
		sharedSegment = ss1106::getSharedState();
		if(sharedSegment == nullptr) {
			throw std::runtime_error("[OLED] shared Memory unexpectedly NULL");
		}
		state = reinterpret_cast<State*>(sharedSegment);
	} else {
		// Memory is somewhere else
		state = state_memory_override;
	}
	memset(state, 0, sizeof(State));
};
SS1106::~SS1106(){
	if (sharedSegment && shmdt(sharedSegment))
		perror("shmdt");
};


uint8_t SS1106::write(uint8_t byte)
{
	if(getDCPin())
	{
		//Data
		//std::cout << "Got Data " << std::hex << (unsigned) byte << std::endl;
		if(state->column > width)
		{
			std::cerr << "OLED: Warning, exceeding column width (" << (int)state->column << " of " << (int)width << ")" << std::endl;
			return -1;		//this is not in spec.
		}
		if(state->page > height/8)
		{
			std::cerr << "OLED: Warning, exceeding page (" << (int)state->page << " of " << (int)height/8 << ")" << std::endl;
			return -1;		//this is not in spec.
		}
		state->frame[state->page][state->column] = byte;
		state->changed = 1;
		state->column++;
	}
	else
	{	//Command
		switch(mode)
		{
		case Mode::normal:
			last_cmd = match(byte);
			//std::cout << "OLED: " << ~last_cmd.op << " " << std::hex << (unsigned)last_cmd.payload << std::endl;
			switch(last_cmd.op)
			{
			case Operator::COL_LOW:
				state->column = (state->column & 0xf0) | last_cmd.payload;
				break;
			case Operator::COL_HIGH:
				state->column = (state->column & 0x0f) | (last_cmd.payload << 4);
				break;
			case Operator::PUMP_VOLTAGE:
				state->pump_voltage = last_cmd.payload;
				break;
			case Operator::CONTRAST_MODE_SET:
				mode = Mode::second_arg;
				break;
			case Operator::NORMAL_INVERSE:
				state->invert_color = last_cmd.payload;
				break;
			//stuff inbetween...
			case Operator::DISPLAY_ON:
				state->display_on = last_cmd.payload;
				break;
			case Operator::PAGE_ADDR:
				state->page = last_cmd.payload;
				break;
			default:
				std::cerr << "OLED: Unhandled Operator " << ~last_cmd.op << std::endl;
			}
			break;
		case Mode::second_arg:
			//std::cout << "OLED: " << ~last_cmd.op << " " << std::hex << (unsigned)byte << std::endl;
			switch(last_cmd.op)
			{
			case Operator::CONTRAST_MODE_SET:
				state->contrast = byte;
				state->changed = 1;
				break;
			default:
				std::cerr << "OLED: Unhandled Dual-command-Operator " << ~last_cmd.op << std::endl;
			}
			mode = Mode::normal;
			break;
		}
	}
	//state->changed = 1;		//This may be optimizable
	return 0;
}



