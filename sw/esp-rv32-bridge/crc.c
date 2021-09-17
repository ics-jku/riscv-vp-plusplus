#include "crc.h"

// 8-bit SAE J1850 CRC
unsigned char crc8(unsigned char *data ,int length) {
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
