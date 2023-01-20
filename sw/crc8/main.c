#include <stdint.h>
#include <stdio.h>
#include "platform.h"
#include "uart.h"
#include "util.h"

// 8-bit SAE J1850 CRC
unsigned char crc8(unsigned char *data, int length) {
	/*
	 * 8-bit CRC calculation
	 * polynomial     : 0x1D
	 * initial value  : 0xFF
	 * reflect input  : no
	 * reflect result : no
	 * XOR value      : 0xFF
	 * check          : 0x4B
	 * magic check    : 0xC4
	 */
	unsigned long crc;
	int i,bit;
	crc = 0xFF;
	for(i=0; i<length; i++) {
		crc ^= data[i];
		for (bit=0; bit<8; bit++) {
			if((crc & 0x80)!=0) {
				crc <<= 1;
				crc ^= 0x1D;
			} else {
				crc <<= 1;
			}
		}
	}
	return (~crc)&0xFF; // xor value = 0xFF
}



int main(int argc, char **argv) {
	/*
	 * test cases from AUTOSAR specification
	 * 8-bit SAE J1850 CRC
	 * see : https://www.autosar.org/fileadmin/user_upload/standards/classic/4-3/AUTOSAR_SWS_CRCLibrary.pdf (page 22/50)
	 * 		7.2.1 8-bit CRC calculation
	 * 		7.2.1.1 8-bit SAE J1850 CRC Calculation
	*/
	// NOTE idea: calculate all 6 test cases in one main loop, see if it makes a good benchmark?
	// unsigned char crcData[] = {0x00,0xC0,0xDF,0xAC,0xAA,0x07,0x40}; // 0x6C
	// unsigned char crcData[] = {0x00,0x00,0x00,0x00}; // 0x59
	// unsigned char crcData[] = {0xF2,0x01,0x83}; // 0x37
	const unsigned char crcData[] = {0x33,0x22,0x55,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}; // 0xCB
	// unsigned char crcData[] = {0x92,0x6B,0x55}; // 0x8C
	//unsigned char crcData[] = {0xFF,0xFF,0xFF,0xFF}; // 0x74
	const unsigned char crcTrueResult = 0xCB;
	char buf[3];
	char buf2[3];
	char *txt1 = "crc8 of:\n";
	char *txt2 = "result:\n";
	sendString(txt1,10);
	for(int i=0; i<sizeof(crcData); i++) {
		itoa(crcData[i], buf, 16);
		sendString(buf, 3);
	}
	putChr('\n');
	const unsigned char crcResult = crc8((unsigned char*)crcData,sizeof(crcData));
	itoa(crcResult, buf2, 16);
	sendString(txt2,9);
	sendString(buf2, 3);
	putChr('\n');

	if(crcResult != crcTrueResult) {
		sendString("CRC wrong!!!!111!!", 19);
		return -1;
	} else {
		sendString("CRC correct\n", 13);
	}
	return 0;
}
