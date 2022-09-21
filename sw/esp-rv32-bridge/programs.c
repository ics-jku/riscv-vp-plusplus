#include "programs.h"

void knight_rider(){
	unsigned int state = 0;
	bool reverse = false;
	unsigned char index = 0;

	for(size_t i=0; i<16; i++){
		if (reverse){
			index--;
			if (index == 0) reverse = false;
		} else{
			index++;
			if (index >= 7) reverse = true;
		}
		state |= 0 << index;
		*led_peripheral = state;
		state = 0;
		//usleep(100); TODO: sleep
	}
	*led_peripheral = 0;
}

void (*programs[])() = {knight_rider};
