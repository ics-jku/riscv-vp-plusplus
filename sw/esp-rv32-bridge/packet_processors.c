#include "packet_processors.h"

void rw_packet_processor(const char *buf, rw_packet_t *rw_packet, int *virtual_memory){
	memcpy(&rw_packet, buf, sizeof(rw_packet_t));

	// no limit check necessary, bit size automatically restricts

	const unsigned char addr = rw_packet.addr;

	if (rw_packet.WE){
		virtual_memory[addr] = rw_packet.v;
	} else{
		// read and send back
		const int v = virtual_memory[addr];
	}
}

void program_leds_packet_processor(const char *buf, program_leds_packet_t *program_leds_packet, int *led_peripheral){
	memcpy(&program_leds_packet, buf, sizeof(program_leds_packet_t));
	if (program_leds_packet.led_manual_mode){
		*led_peripheral = program_leds_packet.leds;
	} else {
		unsigned char program_addr = program_leds_packet.program_addr;
		if(program_addr < 1) (*programs[program_addr])();
	}
}
