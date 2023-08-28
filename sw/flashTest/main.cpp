#include <inttypes.h>
#include <string.h>

#include <iostream>

// From flash.h
static constexpr unsigned int BLOCKSIZE = 512;
static constexpr unsigned int FLASH_ADDR_REG = 0;
static constexpr unsigned int FLASH_SIZE_REG = sizeof(uint64_t);
static constexpr unsigned int DATA_ADDR = FLASH_SIZE_REG + sizeof(uint64_t);
// static constexpr unsigned int ADDR_SPACE = DATA_ADDR + BLOCKSIZE;

static uint8_t* volatile const FLASH_CONTROLLER = (uint8_t* volatile const)(0x71000000);

using namespace std;

void setTargetBlock(uint64_t addr);
void readFlash(char* dst, uint64_t addr, size_t len);
void writeFlash(const char* src, uint64_t addr, size_t len);

int main() {
	unsigned long counter = 0;
	uint64_t flashNumOfBlocks = 0;
	memcpy(&flashNumOfBlocks, FLASH_CONTROLLER + FLASH_SIZE_REG, sizeof(uint64_t));

	cout << "Flash size: " << flashNumOfBlocks * BLOCKSIZE << " (" << flashNumOfBlocks << " Blocks)" << endl;

	readFlash(reinterpret_cast<char*>(&counter), 0, sizeof(unsigned long));

	cout << " Counter before: " << counter << endl;

	writeFlash(reinterpret_cast<char*>(&++counter), 0, sizeof(unsigned long));

	cout << " Counter after: " << counter << endl;
}

void setTargetBlock(uint64_t addr) {
	uint64_t targetBlock = addr % BLOCKSIZE;
	memcpy(FLASH_CONTROLLER + FLASH_ADDR_REG, &targetBlock, sizeof(uint64_t));
}

void readFlash(char* dst, uint64_t addr, size_t len) {
	if (addr / BLOCKSIZE != (addr + len) / BLOCKSIZE) {
		cerr << "Unaligned read, currently not supported" << endl;
		return;
	}
	setTargetBlock(addr);
	memcpy(dst, FLASH_CONTROLLER + DATA_ADDR, len);
}

void writeFlash(const char* src, uint64_t addr, size_t len) {
	if (addr / BLOCKSIZE != (addr + len) / BLOCKSIZE) {
		cerr << "Unaligned read, currently not supported" << endl;
		return;
	}
	setTargetBlock(addr);
	memcpy(FLASH_CONTROLLER + DATA_ADDR, src, len);
}
