#ifndef RISCV_VP_SPI_SD_CARD_H
#define RISCV_VP_SPI_SD_CARD_H

#include <stdint.h>

#include <fstream>

#include "platform/common/gpio_if.h"
#include "platform/common/spi_if.h"

/*
 * SD Card (sdhc) connected via spi
 * implemented after
 * "
 * SD Specifications Part 1 Physical Layer Simplified Specification
 * Version 6.00
 * August 29, 2018
 * "
 *
 * Motivation was simple mass storage in Linux -> Rudimentary support
 * Tested on linux-6.10.X
 *  * detection -> OK
 *  * read(dd) -> OK
 *  * write(dd) -> OK
 *  * mount/blkdev readonly -> OK
 *  * mount/blkdev readwrite -> OK
 */

class SPI_SD_Card : SPI_Device_IF {
	/* sdhc -> fixed block size is 512 byte */
	const static size_t block_size = 512;

	/* VP SPI Interface (spi, chipselect, card detect gpio) */
	SPI_IF *const spi;
	const unsigned int spi_cs;
	GPIO_IF *const gpio_cd;
	const unsigned int gpio_cd_nr;
	const bool gpio_cd_active_high;

	/* file backend */
	std::string card_file_name;
	std::fstream card_file;

	/* selected via chip select */
	bool selected;

	/* size of file backend [bytes] */
	size_t capacity = 0;
	/* keep track of current address (seek) */
	size_t cur_addr = 0;

	/* sd card registers (and register-like data) */
	uint32_t ocr = 0;
	uint8_t cid[16];
	uint8_t csd[16];
	uint8_t scr[8];
	/* see ACMD13 */
	uint8_t sd_status[64];
	/* see CMD6 (64 byte + 16 bit crc) */
	uint8_t switch_function[64 + 2];
	/* see ACMD22 */
	uint32_t num_wr_blocks = 0;

	/* buffer for block read/writes (512 bytes + 16bit crc) */
	uint8_t block[block_size + 2];

	bool acmd_en = false;
	bool crc_enabled = false;

	/* status for R1 reply (used bits only) */
	enum STATUS_R1_BITS {
		STATUS_R1_IDLE = (1 << 0),
		STATUS_R1_ILLEGAL = (1 << 2),
		STATUS_R1_PARA_ERROR = (1 << 6),
	};
	uint8_t status_R1 = STATUS_R1_IDLE;

	/* status for R2 reply (used bits only) */
	enum STATUS_R2_BITS {
		STATUS_R2_ERROR = (1 << 2),
		STATUS_R2_CC_ERROR = (1 << 3),
		STATUS_R2_CARD_ECC_FAILED = (1 << 4),
		STATUS_R2_OUT_OF_RANGE = (1 << 7),
	};
	uint8_t status_R2 = 0;

	/* data error token (same meanings as in status_R2) */
	enum DERR_TOKEN {
		DERR_TOKEN_ERROR = (1 << 0),
		DERR_TOKEN_CC_ERROR = (1 << 1),
		DERR_TOKEN_CARD_ECC_FAILED = (1 << 2),
		DERR_TOKEN_OUT_OF_RANGE = (1 << 3),
	};

	/* data response token */
	enum DRESP_TOKEN {
		DRESP_TOKEN_ACCEPTED = 0b010,
		DRESP_TOKEN_REJECTED_CRC = 0b101,
		DRESP_TOKEN_REJECTED_WRITE_ERROR = 0b110,
	};

	void csd_update();
	void set_capacity(size_t capacity);

	uint8_t get_reset_status_R1();
	uint8_t get_status_R2();
	uint8_t get_reset_status_R2();

	bool do_acmd(uint8_t acmd, uint32_t arg);
	void do_cmd(uint8_t cmd, uint32_t arg);

	class Receiver {
		friend class SPI_SD_Card;
		enum MODE {
			MODE_CMD = 0,
			MODE_DATA = 1,
		};

		SPI_SD_Card *card;
		MODE mode = MODE_CMD;
		bool mult = false;
		unsigned int state = 0;
		uint8_t data[6];
		unsigned int data_len = 0;

		/* for block transfer */
		void sm_data(uint8_t mosi);
		void sm_cmd(uint8_t mosi);

	   protected:
		Receiver(SPI_SD_Card *card);

		void reset();
		void switch_data_mode(bool mult);
		void sm(uint8_t mosi);
	};
	Receiver receiver;

	class Transmitter {
		friend class SPI_SD_Card;
		SPI_SD_Card *card;

		const unsigned int IDLE_LEN = 2;
		const unsigned int TOKEN_LEN = 1;

		bool idle = true;
		bool mult = false;
		unsigned int state = 0;
		uint8_t data[7];
		unsigned int data_len = 0;
		uint8_t *pdata = nullptr;
		unsigned int pdata_len = 0;

		void gen_R1();
		void gen_R2();
		void gen_R3_R7(uint32_t val);

	   protected:
		Transmitter(SPI_SD_Card *card);

		void reset();
		void start();
		void set_data_response_token(uint8_t status);
		void set_R1();
		void set_R2();
		void set_R3(uint32_t ocr);
		void set_R7(uint32_t val);
		void set_R1_payload(uint8_t *data, unsigned int len, bool mult = false);
		void set_R1_block(size_t addr, bool mult);
		void set_R2_payload(uint8_t *data, unsigned int len);

		uint8_t sm();
	};
	Transmitter transmitter;

	bool block_seek(size_t addr);
	bool block_read_next();
	bool block_write_next();

	void set_card_detect(bool ena);

	/* SPI_Device_IF implementation */
	uint8_t transfer(uint8_t mosi) override;
	void select(bool ena) override;

   public:
	SPI_SD_Card(SPI_IF *spi, unsigned int spi_cs, GPIO_IF *gpio_cd, unsigned int gpio_cd_nr, bool gpio_cd_active_high);
	~SPI_SD_Card();

	/*
	 * insert a card (safe to call while in simulation)
	 * card_file_name gives path/filename to card image file
	 * Note: the size of the card file must be a multiple of the sdhc read/write block size (512 bytes)!
	 */
	bool insert(std::string card_file_name);

	/* remove card (safe to call while in simulation) */
	void remove();
};

#endif /* RISCV_VP_SPI_SD_CARD_H */
