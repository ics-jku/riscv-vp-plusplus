#ifndef RISCV_VP_DS1307_H
#define RISCV_VP_DS1307_H

#pragma once
#include <stdint.h>

#include <ctime>
#include <fstream>

#include "chrono"
#include "i2c_if.h"
#include "time.h"

#define DS1307_SIZE_REG_RAM 64

class DS1307 : public I2C_Device_IF {
	uint8_t registers[DS1307_SIZE_REG_RAM];
	uint8_t reg_pointer;
	uint8_t start_signal;

   protected:
	/* I2C_Device_IF implementation */
	bool start() override;
	bool write(uint8_t data) override;
	bool read(uint8_t& data) override;
	bool stop() override;

	// internal functions for handling time and date
	struct tm get_date_time();
	struct tm get_utc_date_time();
	std::time_t convert_tm_to_seconds(struct tm date_time);
	long long diff_date_time(struct tm t1, struct tm t2);
	bool save_diff(long long& diff, const char* filename);
	bool load_diff(long long& diff, const char* filename);
	bool save_state(uint8_t* state, const char* filename);
	bool load_state(uint8_t* state, const char* filename);
	void update_date_time(long long diff, uint8_t mode_12h, uint8_t CH_bit);
	void reset_rtc();

   public:
	DS1307();
};

#endif /* RISCV_VP_DS1307_H */
