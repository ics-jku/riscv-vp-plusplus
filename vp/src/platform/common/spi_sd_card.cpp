#include "spi_sd_card.h"

/*
 * Implementation static helpers
 */

static uint8_t crc7(const uint8_t data[], unsigned int len) {
	const uint8_t p = 0b10001001;
	uint8_t crc = 0;
	while (len--) {
		crc ^= *data++;
		for (unsigned int j = 0; j < 8; j++) {
			crc = (crc >> 7) ? ((crc << 1) ^ (p << 1)) : (crc << 1);
		}
	}
	return crc;
}

static uint16_t crc16(uint8_t *data, unsigned int len) {
	uint16_t crc = 0;
	while (len--) {
		crc = (uint8_t)(crc >> 8) | (crc << 8);
		crc ^= *data++;
		crc ^= (uint8_t)(crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}
	return crc;
}

/*
 * Implementation SPI_SD_Card
 */

SPI_SD_Card::SPI_SD_Card(SPI_IF *spi, unsigned int spi_cs, GPIO_IF *gpio_cd, unsigned int gpio_cd_nr,
                         bool gpio_cd_active_high)
    : spi(spi),
      spi_cs(spi_cs),
      gpio_cd(gpio_cd),
      gpio_cd_nr(gpio_cd_nr),
      gpio_cd_active_high(gpio_cd_active_high),
      receiver(this),
      transmitter(this) {
	/*
	 * init OCR register
	 */
	ocr = 0;
	ocr |= (1 << 31);           /* power up */
	ocr |= (1 << 30);           /* sdhc/sdxc */
	ocr |= (0 << 29);           /* UHS-II */
	ocr |= (0b111111111 << 15); /* all voltage ranges */

	/*
	 * init const CID register
	 */
	/* Manufacturer ID */
	cid[0] = 0x91;
	/* OEM/Application ID */
	cid[1] = 'M';
	cid[2] = 'S';
	/* Product name */
	cid[3] = 'R';
	cid[4] = 'V';
	cid[5] = 'V';
	cid[6] = 'P';
	cid[7] = '2';
	/* Product revision */
	cid[8] = 0x00;
	/* Product serial number (random) */
	cid[9] = 0x3f;
	cid[10] = 0x31;
	cid[11] = 0x01;
	cid[12] = 0xa6;
	/* Manufacturing date 2024.12 */
	cid[13] = 0x01;
	cid[14] = 0x8C;
	/* always crc: is done once -> no cost */
	cid[15] = crc7(cid, sizeof(cid) - 1) | 0b1;

	/*
	 * init constant parts of CSD register (SDHC card)
	 * capacity and crc is done in csd_update
	 */
	memset(csd, 0, sizeof(csd));
	csd[0] |= 0x40;         /* CSD version 2 */
	csd[1] |= 0x0e;         /* TAAC */
	csd[2] |= 0x00;         /* NSAC */
	csd[3] |= 0x32;         /* TRAN_SPEED */
	csd[4] |= 0x51;         /* CCC[11:4] */
	csd[5] |= 0x5 << 4;     /* CCC[3:0] */
	csd[5] |= 0x9;          /* READ_BL_LEN */
	csd[6] |= 0b0000 << 4;  /* READ_BL_P | WR_BLK_MA | RD_BLK_MA | DSR_IMP */
	csd[10] |= 0b1 << 6;    /* ERASE_BLK_LEN */
	csd[10] |= 0b11111;     /* SECTOR_SIZE[6:1] */
	csd[11] |= 0b1 << 7;    /* SECTOR_SIZE[0] */
	csd[11] |= 0x0;         /* WP_GRP_SIZE */
	csd[12] |= 0b0 << 7;    /* WP_GRP_EN */
	csd[12] |= 0b010 << 2;  /* R2W_FACTOR */
	csd[12] |= 0b10;        /* WRITE_BL_LEN[3:2] */
	csd[13] |= 0b01 << 6;   /* WRITE_BL_LEN[1:0] */
	csd[13] |= 0b0 << 5;    /* WRITE_BL_PARTIAL */
	csd[14] |= 0b0000 << 4; /* FILE_FORMAT_GRP | COPY | PERM_WRITE_PROTECT | TEMP_WRITE_PROTECT */
	csd[14] |= 0b00 << 2;   /* FILE_FORMAT */

	/*
	 * init const SCR register
	 * TODO: rework
	 */
	memset(scr, 0, sizeof(scr));
	scr[0] |= 0b0000 << 4; /* SCR_STRUCTURE */
	scr[0] |= 0x2;         /* SD_SPEC */
	scr[1] |= 0b1 << 7;    /* DATA_STAT_AFTER_ERASE */
	scr[1] |= 3 << 4;      /* SD_SECURITY */
	scr[1] |= 0b0101;      /* SD_BUS_WIDTHS */
	scr[2] |= 0b0 << 7;    /* SD_SPEC3 */
	scr[2] |= 0b0000 << 3; /* EX_SECURITY */
	scr[2] |= 0b0 << 2;    /* SD_SPEC4 */
	scr[2] |= 0b00;        /* SD_SPECX[3:2] */
	scr[3] |= 0b00 << 6;   /* SD_SPECX[1:0] */
	scr[3] |= 0b0000;      /* CMD_SUPPORT */

	/*
	 * init SD_STATUS
	 * zero is a valid and safe choice for all fields
	 */
	memset(sd_status, 0, sizeof(sd_status));

	/*
	 * init const SWITCH_FUNCTION
	 * zero is a valid and safe choice for all fields
	 */
	memset(switch_function, 0, sizeof(switch_function));
	uint16_t crc = crc16(switch_function, sizeof(switch_function) - 2);
	switch_function[64] = crc >> 8;
	switch_function[65] = crc & 0xff;

	/*
	 * init card
	 */
	select(false);
	remove();
	spi->connect_device(spi_cs, this);
}

SPI_SD_Card::~SPI_SD_Card() {
	card_file.close();
}

bool SPI_SD_Card::insert(std::string card_file_name) {
	remove();

	card_file.open(card_file_name, std::ofstream::in | std::ofstream::out | std::ofstream::binary);
	if (!card_file.is_open() || !card_file.good()) {
		std::cerr << "SPI_SD_Card: ERROR: Failed to open " << card_file_name << ": " << strerror(errno) << std::endl;
		return false;
	}

	std::streampos fsize = card_file.tellg();
	card_file.seekg(0, std::ios::end);
	size_t capacity = card_file.tellg() - fsize;
	if (capacity == 0 || capacity & 0x1FF) {
		std::cerr << "SPI_SD_Card: ERROR: Size of " << card_file_name << " is zero or not a multiple of 512!"
		          << std::endl;
		card_file.close();
		return false;
	}

	this->card_file_name = card_file_name;
	set_capacity(capacity);
	// TODO: set card detect after delay (in SystemC process)?
	set_card_detect(true);
	receiver.reset();
	transmitter.reset();

	return true;
}

void SPI_SD_Card::remove() {
	status_R1 = STATUS_R1_IDLE;
	status_R2 = 0;
	acmd_en = false;
	cur_addr = 0;
	num_wr_blocks = 0;
	// TODO: deassert card detect after delay (in SystemC process)?
	set_card_detect(false);
	card_file_name = "";
	set_capacity(0);
	receiver.reset();
	transmitter.reset();
	if (card_file.is_open()) {
		card_file.close();
	}
}

void SPI_SD_Card::csd_update() {
	uint32_t c_size = (capacity / (block_size * 1024)) - 1;
	csd[7] = (c_size >> 16) & 0x3f;
	csd[8] = (c_size >> 8) & 0xff;
	csd[9] = (c_size >> 0) & 0xff;
	/* always crc: is done only on insert -> ~no cost */
	csd[15] = crc7(csd, sizeof(csd) - 1) | 0b1;
}

void SPI_SD_Card::set_capacity(size_t capacity) {
	this->capacity = capacity;
	csd_update();
}

uint8_t SPI_SD_Card::get_reset_status_R1() {
	uint8_t ret = status_R1;
	status_R1 &= ~(STATUS_R1_ILLEGAL | STATUS_R1_PARA_ERROR);
	return ret;
}

uint8_t SPI_SD_Card::get_status_R2() {
	return status_R2;
}

uint8_t SPI_SD_Card::get_reset_status_R2() {
	uint8_t ret = status_R2;
	status_R2 = 0;
	return ret;
}

bool SPI_SD_Card::do_acmd(uint8_t acmd, uint32_t arg) {
	// std::cout << "SPI_SD_Card: DEBUG: ACMD" << (unsigned int)acmd << std::endl;
	switch (acmd) {
		// SD_STATUS
		case 13:
			// see also CMD13
			transmitter.set_R2_payload(sd_status, sizeof(sd_status));
			break;

		// SEND_NUM_RW_BLOCKS
		case 22:
			/* we use block as intermediate buffer here */
			block[0] = ((num_wr_blocks >> 24) & 0xff);
			block[1] = ((num_wr_blocks >> 16) & 0xff);
			block[2] = ((num_wr_blocks >> 8) & 0xff);
			block[3] = ((num_wr_blocks >> 0) & 0xff);
			if (crc_enabled) {
				uint16_t crc = crc16(block, 4);
				block[4] = ((crc >> 8) & 0xff);
				block[5] = ((crc >> 0) & 0xff);
			} else {
				block[4] = 0xff;
				block[5] = 0xff;
			}
			transmitter.set_R1_payload(block, 6);
			break;

		// SET_WR_BLK_ERASE_COUNT
		case 23:
			// unused
			transmitter.set_R1();
			break;

		// SD_SEND_OP_COND
		case 41:
			transmitter.set_R1();
			break;

		// SET_CLR_CARD_DETECT
		case 42:
			// unused
			transmitter.set_R1();
			break;

		// SEND_SCR
		case 51:
			transmitter.set_R1_payload(scr, sizeof(scr));
			break;

		default:
			// see spec 4.3.9.1: When an ACMD is not defined, the card treats it as regular command.
			// is specified behaviour - only debug message
			// std::cout << "SPI_SD_Card: DEBUG: unknown ACMD" << (unsigned int)acmd << " -> Fall back to CMD
			// handling" << std::endl;
			return false;
	}

	/* handled */
	return true;
}

void SPI_SD_Card::do_cmd(uint8_t cmd, uint32_t arg) {
	if (acmd_en) {
		acmd_en = false;
		if (do_acmd(cmd, arg)) {
			/* handled */
			return;
		}
		/* try as CMD (see acmd handling) */
	}

	// std::cout << "SPI_SD_Card: DEBUG: CMD" << (unsigned int)cmd << std::endl;
	switch (cmd) {
		// GO_IDLE_STATE
		case 0:
			status_R1 |= STATUS_R1_IDLE;
			crc_enabled = false;
			transmitter.set_R1();
			break;

		// SEND_OP_COND
		case 1:
			// SD, not MMC -> invalid for SD
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// Reserved for I/O Mode (optional)
		case 5:
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// SWITCH_FUNC
		case 6:
			// TODO: only partial implementation
			transmitter.set_R1_payload(switch_function, sizeof(switch_function));
			break;

		// SEND_IF_COND
		case 8:
			/* valid -> Spec 2.0 */

			/*
			 * we are simulating a high capacity card
			 * hence CMD8 before ACMD41 is mandatory
			 * realized by resetting idle here instead of ACMD41
			 */
			status_R1 &= ~STATUS_R1_IDLE;

			transmitter.set_R7(0);
			break;

		// SEND_CSD
		case 9:
			transmitter.set_R1_payload(csd, sizeof(csd));
			break;

		// SEND_CID
		case 10:
			transmitter.set_R1_payload(cid, sizeof(cid));
			break;

		// STOP_TRANSMISSION
		case 12:
			/* generating a response will automatically abort running mult block reads */
			transmitter.set_R1();
			break;

		// SEND_STATUS
		case 13:
			transmitter.set_R2();
			break;

		// SET_BLOCKLEN
		case 16:
			/*
			 * TODO: Not implemented yet
			 * On sdhc/sdxc only used to set length of lock_unlock (CMD43) which is not implemented yet
			 */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// READ_SINGLE_BLOCK
		case 17:
			transmitter.set_R1_block(arg * block_size, false);
			break;

		// READ_MULTIPLE_BLOCK
		case 18:
			transmitter.set_R1_block(arg * block_size, true);
			break;

		// WRITE_BLOCK
		case 24:
		// WRITE_MULTIPLE_BLOCK
		case 25: {
			bool mult = cmd == 25;
			num_wr_blocks = 0;
			if (block_seek(arg * block_size)) {
				/* ok */
				receiver.switch_data_mode(mult);
			}
			transmitter.set_R1();
		} break;

		// PROGRAM_CSD
		case 27:
			/* TODO: Not implemented yet */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// SET_WRITE_PROT (optional)
		case 28:
		// CLR_WRITE_PROT (optional)
		case 29:
		// SEND_WRITE_PROT (optional)
		case 30:
			// Optional; This is not supported on sdhc and sdxc cards -> Not implemented
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// ERASE_WR_BLK_START_ADDR
		case 32:
			/* TODO: Not implemented yet */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// ERASE_WR_BLK_END_ADDR
		case 33:
			/* TODO: Not implemented yet */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// Reserved for each command system (optional)
		case 34:
		case 35:
		case 36:
		case 37:
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// ERASE
		case 38:
			/* TODO: Not implemented yet */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// LOCK_UNLOCK
		case 42:
			/* TODO: Not implemented yet */
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// Reserved for each command system (optional)
		case 50:
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// Reserved for I/O Mode (optional)
		case 52:
		case 53:
		case 54:
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// APP_CMD
		case 55:
			acmd_en = true;
			transmitter.set_R1();
			break;

		// GEN_CMD
		case 56:
			/* TODO: Not implemented yet */
			// write = arg & 1
			std::cerr << "SPI_SD_Card: WARNING: Missing implementation for CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// Reserved for each command system (optional)
		case 57:
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
			break;

		// READ_OCR
		case 58:
			transmitter.set_R3(ocr);
			break;

		case 59:
			// std::cout << "SPI_SD_Card: crc " << (arg & 1 ? "enabled" : "disabled") << std::endl;
			crc_enabled = true;
			transmitter.set_R1();
			break;

		default:
			std::cerr << "SPI_SD_Card: ERROR: Unknown CMD" << (unsigned int)cmd << std::endl;
			status_R1 |= STATUS_R1_ILLEGAL;
			transmitter.set_R1();
	}
}

bool SPI_SD_Card::block_seek(size_t addr) {
	if (addr + block_size > capacity) {
		status_R1 |= STATUS_R1_PARA_ERROR;
		status_R2 |= STATUS_R2_OUT_OF_RANGE;
		return false;
	}

	card_file.seekg(addr, card_file.beg);
	if (!card_file.is_open() || !card_file.good()) {
		// TODO: strerror check
		std::cerr << "SPI_SD_Card: ERROR: Failed to seek on " << card_file_name << ": " << strerror(errno) << std::endl;
		status_R1 |= STATUS_R1_PARA_ERROR;  // TODO: maybe use other flag?
		status_R2 |= STATUS_R2_ERROR;
		return false;
	}
	cur_addr = addr;

	return true;
}

bool SPI_SD_Card::block_read_next() {
	if (cur_addr + block_size > capacity) {
		status_R1 |= STATUS_R1_PARA_ERROR;
		status_R2 |= STATUS_R2_OUT_OF_RANGE;
		return false;
	}

	card_file.read(reinterpret_cast<char *>(block), block_size);
	if (!card_file.is_open() || !card_file.good()) {
		// TODO: strerror check
		std::cerr << "SPI_SD_Card: ERROR: Failed to read from " << card_file_name << ": " << strerror(errno)
		          << std::endl;
		status_R1 |= STATUS_R1_PARA_ERROR;  // TODO: maybe use other flag?
		status_R2 |= STATUS_R2_ERROR;
		return false;
	}
	cur_addr += block_size;

	if (crc_enabled) {
		uint16_t crc = crc16(block, block_size);
		block[block_size + 0] = (crc >> 8) & 0xff;
		block[block_size + 1] = (crc >> 0) & 0xff;
	} else {
		/* don't care */
		block[block_size + 0] = 0xff;
		block[block_size + 1] = 0xff;
	}

	return true;
}

bool SPI_SD_Card::block_write_next() {
	if (cur_addr + block_size > capacity) {
		status_R1 |= STATUS_R1_PARA_ERROR;
		status_R2 |= STATUS_R2_OUT_OF_RANGE;
		return false;
	}

	card_file.write(reinterpret_cast<char *>(block), block_size);
	card_file.flush();
	if (!card_file.is_open() || !card_file.good()) {
		// TODO: strerror check
		std::cerr << "SPI_SD_Card: ERROR: Failed to write to " << card_file_name << ": " << strerror(errno)
		          << std::endl;
		status_R1 |= STATUS_R1_PARA_ERROR;  // TODO: maybe use other flag?
		status_R2 |= STATUS_R2_ERROR;
		return false;
	}
	cur_addr += block_size;
	num_wr_blocks++;

	return true;
}

/* SPI_Device transfer method */
uint8_t SPI_SD_Card::transfer(uint8_t mosi) {
	uint8_t miso;
	if (selected) {
		receiver.sm(mosi);
		miso = transmitter.sm();
	} else {
		std::cerr << "SPI_SD_Card: WARNING: spi transfer on unselected device - should not happen!" << std::endl;
		miso = 0xff;
	}
	// std::cout << "SPI_SD_Card: DEBUG: mmc_write: " << std::hex << (uint32_t)mosi << " " << (uint32_t)miso <<
	// std::endl;
	return miso;
}

/* SPI Chipselect method */
void SPI_SD_Card::select(bool ena) {
	selected = ena;

	/*
	 * According to the specification, the host can issue reads/writes and the deselect the card.
	 * The card then continues the read/write operation while deselected. This allows for example a multiple
	 * interleaved card setup for increasing performance. However, this indicates, that the card must not loose any
	 * state on deselect. i.e. We don't reset the receiver and transmitter here
	 */
	// receiver.reset();
	// transmitter.reset();
}

void SPI_SD_Card::set_card_detect(bool ena) {
	if (gpio_cd == nullptr) {
		return;
	}
	gpio_cd->set_gpio(gpio_cd_nr, ena ^ (!gpio_cd_active_high));
}

/*
 * Implementation class Receiver
 */

SPI_SD_Card::Receiver::Receiver(SPI_SD_Card *card) : card(card) {
	/* block len + ecc */
	data_len = sizeof(card->block);
}

/* for block transfer */
void SPI_SD_Card::Receiver::sm_data(uint8_t mosi) {
	if (state == 0) {
		/* wait for token */
		switch (mosi) {
			/* start block single */
			case 0b11111110:
				if (mult) {
					std::cerr << "SPI_SD_Card: ERROR: Invalid single block start in mult write" << std::endl;
					card->transmitter.set_data_response_token(DRESP_TOKEN_REJECTED_WRITE_ERROR);
					reset();
				} else {
					/* block started */
					state++;
				}
				break;

			/* start block mult */
			case 0b11111100:
				if (!mult) {
					std::cerr << "SPI_SD_Card: ERROR: Invalid mult block start in single write" << std::endl;
					card->transmitter.set_data_response_token(DRESP_TOKEN_REJECTED_WRITE_ERROR);
					reset();
				} else {
					/* block started */
					state++;
				}
				break;

			/* stop block mult */
			case 0b11111101:
				if (!mult) {
					std::cerr << "SPI_SD_Card: ERROR: Invalid mult block stop in single write" << std::endl;
				}
				/* end of data transmission */
				reset();
				break;

			/* wait? -> TODO: how long? */
			case 0xff:
				break;

			default:
				std::cerr << "SPI_SD_Card: ERROR: Invalid token" << std::endl;
				card->transmitter.set_data_response_token(DRESP_TOKEN_REJECTED_WRITE_ERROR);
				reset();
		}

	} else if (state < data_len + 1) {
		/* store data */
		card->block[state - 1] = mosi;
		state++;

	} else {
		/* full block received -> check crc */
		if (card->crc_enabled) {
			uint16_t crc_rx = card->block[data_len - 2] << 8 | card->block[data_len - 1];
			uint16_t crc_calc = crc16(card->block, data_len - 2);
			if (crc_rx != crc_calc) {
				std::cerr << "SPI_SD_Card: WARNING: CRC Error" << std::endl;
				card->transmitter.set_data_response_token(DRESP_TOKEN_REJECTED_CRC);
				reset();
			}
		}

		if (!card->block_write_next()) {
			/* error */
			card->transmitter.set_data_response_token(DRESP_TOKEN_REJECTED_WRITE_ERROR);
			reset();
		}

		card->transmitter.set_data_response_token(DRESP_TOKEN_ACCEPTED);

		state = 0;
	}
}

void SPI_SD_Card::Receiver::sm_cmd(uint8_t mosi) {
	/* record data*/
	data[state] = mosi;

	switch (state) {
		/* start command */
		case 0:
			if ((mosi >> 6) == 0b01) {
				state = 1;
			}
			break;

		/* receive argument */
		case 1:
		case 2:
		case 3:
		case 4:
			state++;
			break;

		/* receive end, interpret, check and handle cmd */
		case 5: {
			/* interpret */
			uint8_t cmd = data[0] & 0b111111;
			uint32_t arg = data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4];
			uint8_t crc = data[5] & ~0b1;
			bool valid = data[5] & ~0b1;

			// debug
			// std::cout << "SPI_SD_Card: Receiver: received command: " << "valid = " << valid << ", (A)CMD" << std::dec
			// << (int)cmd << "(0x" << std::hex << arg << "), crc = 0x" << (unsigned int)crc << std::dec << std::endl;

			/* check */

			if (!valid) {
				std::cerr << "SPI_SD_Card: WARNING: Receiver: Command is not valid -> RESET!" << std::endl;
				reset();
			}
			/* always check crc for CMD0 */
			if (cmd == 0 || card->crc_enabled) {
				if (crc != crc7(data, 5)) {
					std::cerr << "SPI_SD_Card: WARNING: Receiver: crc error (calc = 0x" << std::hex
					          << (unsigned int)crc7(data, 5) << " vs. rcv = 0x" << (unsigned int)crc << ") -> RESET!"
					          << std::dec << std::endl;
					reset();
				}
			}

			/* handle cmd */
			card->do_cmd(cmd, arg);

			state = 0;
		} break;

		/* should never happen */
		default:
			std::cerr << "SPI_SD_Card: ERROR: Receiver: Invalid state (should never happen) -> RESET!" << std::endl;
			reset();
			break;
	}
}

void SPI_SD_Card::Receiver::reset() {
	mode = MODE_CMD;
	state = 0;
}

void SPI_SD_Card::Receiver::switch_data_mode(bool mult) {
	mode = MODE_DATA;
	state = 0;
	this->mult = mult;
}

void SPI_SD_Card::Receiver::sm(uint8_t mosi) {
	if (mode == MODE_CMD) {
		sm_cmd(mosi);
	} else {
		sm_data(mosi);
	}
}

/*
 * Implementation class Transmitter
 */
SPI_SD_Card::Transmitter::Transmitter(SPI_SD_Card *card) : card(card) {
	reset();
}

void SPI_SD_Card::Transmitter::gen_R1() {
	data[0] = card->get_reset_status_R1();
	data_len = 1;
}

void SPI_SD_Card::Transmitter::gen_R2() {
	gen_R1();
	data[1] = card->get_reset_status_R2();
	data_len = 2;
}

void SPI_SD_Card::Transmitter::gen_R3_R7(uint32_t val) {
	gen_R1();
	data[1] = (val >> 24) & 0xff;
	data[2] = (val >> 16) & 0xff;
	data[3] = (val >> 8) & 0xff;
	data[4] = (val >> 0) & 0xff;
	data_len = 5;
}

void SPI_SD_Card::Transmitter::reset() {
	idle = true;
	mult = false;
	data_len = 0;
	pdata_len = 0;
	state = 0;
}

void SPI_SD_Card::Transmitter::start() {
	idle = false;
}

void SPI_SD_Card::Transmitter::set_data_response_token(uint8_t status) {
	reset();
	data[0] = 0b11100001 | ((status & 0x7) << 1);
	data_len = 1;
	start();
}

void SPI_SD_Card::Transmitter::set_R1() {
	reset();
	gen_R1();
	start();
}

void SPI_SD_Card::Transmitter::set_R2() {
	reset();
	gen_R2();
	start();
}

void SPI_SD_Card::Transmitter::set_R3(uint32_t ocr) {
	reset();
	gen_R3_R7(ocr);
	start();
}

void SPI_SD_Card::Transmitter::set_R7(uint32_t val) {
	reset();
	gen_R3_R7(val);
	start();
}

void SPI_SD_Card::Transmitter::set_R1_payload(uint8_t *data, unsigned int len, bool mult) {
	reset();
	gen_R1();
	pdata = data;
	pdata_len = len;
	this->mult = mult;
	start();
}

void SPI_SD_Card::Transmitter::set_R1_block(size_t addr, bool mult) {
	/* respond on errors of first block directly in R1
	 * later errors are reported via data error token (see state machine)
	 */
	if (!card->block_seek(addr)) {
		// error
		set_R1();
		return;
	}

	if (!card->block_read_next()) {
		// error
		set_R1();
		return;
	}

	set_R1_payload(card->block, sizeof(card->block), mult);
}

void SPI_SD_Card::Transmitter::set_R2_payload(uint8_t *data, unsigned int len) {
	reset();
	gen_R2();
	pdata = data;
	pdata_len = len;
	start();
}

uint8_t SPI_SD_Card::Transmitter::sm() {
	uint8_t ret = 0xff;

	if (idle) {
		return ret;
	}

	/* start */
	if (state < IDLE_LEN) {
		ret = 0xff;

		/* Rx */
	} else if (state < IDLE_LEN + data_len) {
		ret = data[state - IDLE_LEN];

		/* start block token */
	} else if (pdata_len && state < IDLE_LEN + data_len + TOKEN_LEN) {
		ret = 0xfe;

		/* block data */
	} else if (pdata_len && state < IDLE_LEN + data_len + TOKEN_LEN + pdata_len) {
		ret = pdata[state - (IDLE_LEN + data_len + TOKEN_LEN)];

		/* done */
	} else {
		if (mult) {
			/* read next block and transmit */
			if (!card->block_read_next()) {
				/* error -> send data error token (flags from R2) */
				uint8_t statR2 = card->get_status_R2();
				ret = 0x00;
				if (statR2 & STATUS_R2_ERROR) {
					ret |= DERR_TOKEN_ERROR;
				}
				if (statR2 & STATUS_R2_CC_ERROR) {
					ret |= DERR_TOKEN_CC_ERROR;
				}
				if (statR2 & STATUS_R2_CARD_ECC_FAILED) {
					ret |= DERR_TOKEN_CARD_ECC_FAILED;
				}
				if (statR2 & STATUS_R2_OUT_OF_RANGE) {
					ret |= DERR_TOKEN_OUT_OF_RANGE;
				}
				/* stop */
				idle = true;
				state = 0;
			} else {
				/* restart at block token */
				state = IDLE_LEN + data_len - TOKEN_LEN;
			}
		} else {
			/* stop */
			idle = true;
			state = 0;
		}
	}

	state++;
	return ret;
}
