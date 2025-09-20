#include "ds1307.h"

#include <iostream>

#define DS1307_ADDRESS_SECONDS 0x00
#define DS1307_ADDRESS_MINUTES 0x01
#define DS1307_ADDRESS_HOURS 0x02
#define DS1307_ADDRESS_DAY 0x03
#define DS1307_ADDRESS_DATE 0x04
#define DS1307_ADDRESS_MONTH 0x05
#define DS1307_ADDRESS_YEAR 0x06
#define DS1307_ADDRESS_CONTROL 0x07
#define DS1307_ADDRESS_RAM_BEGIN 0x08
#define DS1307_ADDRESS_RAM_END 0x3F

#define DS1307_BIT_12_24_MASK 0x40
#define DS1307_BIT_PM_AM_MASK 0x20
#define DS1307_BIT_CH_MASK 0x80

#define AWAITING_START 0
#define START_RECEIVED 1
#define DATA_PHASE 2

#define DIFF_DATE_TIME_FILE "ds_1307_date_time_diff"
#define DS1307_STATE_FILE "ds_1307_state"

DS1307::DS1307() {
	reg_pointer = 0;
	start_signal = AWAITING_START;
	if (!load_state(registers, DS1307_STATE_FILE)) {
		for (int i = 0; i < 64; i++) {
			registers[i] = 0;
		}
		save_state(registers, DS1307_STATE_FILE);
	}
	long long diff;
	uint8_t mode_12h = (registers[DS1307_ADDRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
	uint8_t CH_bit = (registers[DS1307_ADDRESS_SECONDS] & DS1307_BIT_CH_MASK) >> 7;
	if (!CH_bit) {  // if CH bit is not set clock is not stopped
		if (!load_diff(diff, DIFF_DATE_TIME_FILE)) {
			diff = 0;  // diff 0 = current UTC time
			save_diff(diff, DIFF_DATE_TIME_FILE);
		}
		update_date_time(diff, mode_12h, CH_bit);

	} else {  // time stopped calculate current diff and set CH bit to 1
		diff = diff_date_time(get_date_time(), get_utc_date_time());
		save_diff(diff, DIFF_DATE_TIME_FILE);
		update_date_time(diff, mode_12h, CH_bit);
	}
}

bool DS1307::start() {
	start_signal = START_RECEIVED;
	long long diff;
	// load current diff time and date from file and update registers
	if (!(registers[DS1307_ADDRESS_SECONDS] & DS1307_BIT_CH_MASK)) {  // Clock is halted, do not update time
		load_diff(diff, DIFF_DATE_TIME_FILE);
		uint8_t mode_12h = (registers[DS1307_ADDRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
		uint8_t CH_bit = (registers[DS1307_ADDRESS_SECONDS] & DS1307_BIT_CH_MASK) >> 7;
		update_date_time(diff, mode_12h, CH_bit);
	}
	return true;
}

bool DS1307::write(uint8_t data) {
	if (start_signal == START_RECEIVED) {
		reg_pointer = data;
		start_signal = DATA_PHASE;
	} else if (start_signal == DATA_PHASE) {
		registers[reg_pointer] = data;
		if (reg_pointer == DS1307_ADDRESS_RAM_END) {
			reg_pointer = 0;  // reset reg_pointer to 0 after pointer reaches RAM end
		} else {
			reg_pointer++;
		}
	} else {
		return false;
	}

	save_state(registers, DS1307_STATE_FILE);  // save state here or at stop?
	struct tm set_time = get_date_time();
	struct tm local_time = get_utc_date_time();
	long long diff = diff_date_time(set_time, local_time);
	save_diff(diff, DIFF_DATE_TIME_FILE);
	return true;
}

bool DS1307::read(uint8_t& data) {
	if (start_signal == START_RECEIVED) {
		data = registers[reg_pointer];
		if (reg_pointer == DS1307_ADDRESS_RAM_END) {
			reg_pointer = 0;  // reset reg_pointer to 0 after pointer reaches RAM end
		} else {
			reg_pointer++;
		}
		return true;
	}
	return false;
}

bool DS1307::stop() {
	start_signal = AWAITING_START;
	// update time diff
	struct tm set_time = get_date_time();
	struct tm local_time = get_utc_date_time();
	long long diff = diff_date_time(set_time, local_time);
	// save current diff, registers, and ram
	save_diff(diff, DIFF_DATE_TIME_FILE);
	save_state(registers, DS1307_STATE_FILE);
	return true;
}

/* convert time and date information saved in registers into struct tm */
struct tm DS1307::get_date_time() {
	struct tm current;
	current.tm_isdst = 0;
	current.tm_sec =
	    (registers[DS1307_ADDRESS_SECONDS] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_SECONDS] & 0x70) >> 4);
	current.tm_min =
	    (registers[DS1307_ADDRESS_MINUTES] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_MINUTES] & 0x70) >> 4);
	// check if rtc is set to 12h or 24h mode
	uint8_t hour_mode = (registers[DS1307_ADDRESS_HOURS] & DS1307_BIT_12_24_MASK) >> 6;
	if (hour_mode) {  // 12h mode
		uint8_t pm = (registers[DS1307_ADDRESS_HOURS] & DS1307_BIT_PM_AM_MASK) >> 5;
		current.tm_hour =
		    (registers[DS1307_ADDRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_HOURS] & 0x10) >> 4) + pm * 12;
	} else {  // 24 h mode
		current.tm_hour =
		    (registers[DS1307_ADDRESS_HOURS] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_HOURS] & 0x30) >> 4);
	}
	// use correct time format according to struct tm specification
	current.tm_wday = ((registers[DS1307_ADDRESS_DAY] & 0x07) == 7) ? 0 : (registers[DS1307_ADDRESS_DAY] & 0x07);
	current.tm_mday = (registers[DS1307_ADDRESS_DATE] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_DATE] & 0x30) >> 4);
	current.tm_mon =
	    (registers[DS1307_ADDRESS_MONTH] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_MONTH] & 0x10) >> 4) - 1;
	current.tm_year =
	    (registers[DS1307_ADDRESS_YEAR] & 0x0F) + 10 * ((registers[DS1307_ADDRESS_YEAR] & 0xF0) >> 4) + 100;

	return current;
}

std::time_t DS1307::convert_tm_to_seconds(struct tm date_time) {
	// Convert tm to time_t (seconds since epoch)
	std::time_t time = timegm(&date_time);
	return time;
}

long long DS1307::diff_date_time(struct tm t1, struct tm t2) {
	// Convert both tm structures to time_points
	std::time_t time1 = convert_tm_to_seconds(t1);
	std::time_t time2 = convert_tm_to_seconds(t2);

	return time1 - time2;
}

bool DS1307::save_diff(long long& diff, const char* filename) {
	std::ofstream outFile(filename, std::ios::binary);
	if (!outFile) {
		return false;
	}
	outFile.write(reinterpret_cast<const char*>(&diff), sizeof(long long));
	outFile.close();
	return true;
}

bool DS1307::load_diff(long long& diff, const char* filename) {
	std::ifstream inFile(filename, std::ios::binary);
	if (!inFile) {
		return false;
	}
	inFile.read(reinterpret_cast<char*>(&diff), sizeof(long long));
	inFile.close();
	return true;
}

// save current date registers + RAM
bool DS1307::save_state(uint8_t* state, const char* filename) {
	std::ofstream outFile(filename, std::ios::binary);
	if (!outFile) {
		return false;
	}
	outFile.write(reinterpret_cast<const char*>(state), DS1307_SIZE_REG_RAM * sizeof(uint8_t));
	return true;
}

// load date registers + RAM from file
bool DS1307::load_state(uint8_t* state, const char* filename) {
	std::ifstream inFile(filename, std::ios::binary);
	if (!inFile) {
		return false;
	}

	inFile.read(reinterpret_cast<char*>(state), DS1307_SIZE_REG_RAM * sizeof(uint8_t));
	return true;
}

/* update time and date saved in registers according to set time diff and 12 or 24h mode */
void DS1307::update_date_time(long long diff, uint8_t mode_12h, uint8_t CH_bit) {
	// struct tm current = get_date_time();
	std::time_t current = DS1307::convert_tm_to_seconds(get_utc_date_time());
	std::time_t new_datetime = current + diff;

	struct tm ds1307_new_reg_vals;
	memcpy(&ds1307_new_reg_vals, gmtime(&new_datetime), sizeof(struct tm));

	// adjust values to fit format of ds1307
	if (ds1307_new_reg_vals.tm_wday == 0) {
		ds1307_new_reg_vals.tm_wday = 7;
	}
	ds1307_new_reg_vals.tm_mon += 1;
	ds1307_new_reg_vals.tm_year -= 100;

	registers[DS1307_ADDRESS_SECONDS] =
	    (CH_bit << 7) + (((ds1307_new_reg_vals.tm_sec / 10) << 4) & 0x70) + (ds1307_new_reg_vals.tm_sec % 10);
	registers[DS1307_ADDRESS_MINUTES] =
	    (0 << 7) + (((ds1307_new_reg_vals.tm_min / 10) << 4) & 0x70) + (ds1307_new_reg_vals.tm_min % 10);
	if (mode_12h) {
		uint8_t pm = 0;
		uint8_t bit_mask_10h = 0x30;
		if (ds1307_new_reg_vals.tm_hour >= 12) {
			pm = 1;
			if (ds1307_new_reg_vals.tm_hour > 12) {
				ds1307_new_reg_vals.tm_hour -= 12;
			}
			bit_mask_10h = 0x10;  // adjust 10h register bit mask
		} else if (ds1307_new_reg_vals.tm_hour == 0) {
			ds1307_new_reg_vals.tm_hour = 12;  // set 12h for midnight
		}
		registers[DS1307_ADDRESS_HOURS] = (0 << 7) + (1 << 6) + (pm << 5) +
		                                  (((ds1307_new_reg_vals.tm_hour / 10) << 4) & bit_mask_10h) +
		                                  (ds1307_new_reg_vals.tm_hour % 10);
	} else {
		registers[DS1307_ADDRESS_HOURS] =
		    (0 << 7) + (((ds1307_new_reg_vals.tm_hour / 10) << 4) & 0x30) + (ds1307_new_reg_vals.tm_hour % 10);
	}
	registers[DS1307_ADDRESS_DAY] = (ds1307_new_reg_vals.tm_wday) & 0x07;
	registers[DS1307_ADDRESS_DATE] =
	    (0 << 7) + (((ds1307_new_reg_vals.tm_mday / 10) << 4) & 0x30) + (ds1307_new_reg_vals.tm_mday % 10);
	registers[DS1307_ADDRESS_MONTH] =
	    (0 << 7) + ((((ds1307_new_reg_vals.tm_mon) / 10) << 4) & 0x10) + ((ds1307_new_reg_vals.tm_mon) % 10);
	registers[DS1307_ADDRESS_YEAR] =
	    ((((ds1307_new_reg_vals.tm_year) / 10) << 4) & 0xF0) + ((ds1307_new_reg_vals.tm_year) % 10);
}

struct tm DS1307::get_utc_date_time() {
	time_t now = time(nullptr);
	struct tm current_date_time;
	current_date_time = *gmtime(&now);
	return current_date_time;
}

void DS1307::reset_rtc() {
	long long diff = 0;
	save_diff(diff, DIFF_DATE_TIME_FILE);  // initialize to 0 difference
	for (int i = 0; i < 64; i++) {
		registers[i] = 0;
	}
	save_state(registers, DS1307_STATE_FILE);
}
