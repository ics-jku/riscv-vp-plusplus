/*
 * common.hpp
 *
 *  Created on: 24 Sep 2019
 *      Author: dwd
 */

#pragma once
#include <inttypes.h>

#include "util/elegantEnums.hpp"

namespace ss1106 {

static constexpr uint8_t width = 132;
static constexpr uint8_t padding_lr = 2;
static constexpr uint8_t height = 64;
static_assert(height % 8 == 0, "invalid height");
static constexpr uint16_t shm_key = 1339;

struct State {
	uint8_t changed : 1;  // beh, this is for syncing

	uint8_t column;
	uint8_t page;
	uint8_t pump_voltage : 2;
	uint8_t display_startline : 6;
	uint8_t contrast;
	uint8_t segment_remap : 1;
	uint8_t entire_disp_on : 1;
	uint8_t invert_color : 1;
	uint8_t multiplex_whatever : 6;
	uint8_t display_on : 1;
	uint8_t frame[(height / 8)][width];
};

State* getSharedState();

DECLARE_ENUM(Operator, COL_LOW, COL_HIGH, PUMP_VOLTAGE, DISPLAY_START_LINE,
             CONTRAST_MODE_SET,  // Double Command
             SEGMENT_REMAP, ENTIRE_DISPLAY, NORMAL_INVERSE,
             MULTIPLEX_RATIO,  // Double command
             DC_DC_VOLTAGE, DISPLAY_ON, PAGE_ADDR, COMMON_OUTPUT_DIR,
             DISPLAY_OFFSET,         // Double command
             DISPLAY_DIVIDE_RATIO,   // Double command
             DIS_PRE_CHARGE_PERIOD,  // Double command
             COMMON_PADS_STUFF,      // DC
             VCOM_DESELECT,          // DC
             RMW, RMW_END, NOP);

}  // namespace ss1106
