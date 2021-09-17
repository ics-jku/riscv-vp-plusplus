#include <stdio.h>
#include "platform.h"
#include "uart.h"
#include "util.h"
#include "crc.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

unsigned long *led_peripheral = (unsigned long *)0x81000000;

typedef enum {
	IDLE,
	LISTENING,
	HEADER_START,
	PAYLOAD_START,
	CHECKSUM_START,
	PROCESSING
} wifi_bridge_fsm_state;

typedef union {
  struct __attribute__((packed)){
    unsigned char packet_identifier : 4;
	unsigned char versioning : 2;
    unsigned char __padding__ : 2;
  } meta_bitfield;
  unsigned char meta;
} header_meta_t;

typedef struct __attribute__((packed)){
	unsigned char WE : 1;
	unsigned char addr : 5;
	unsigned char v : 8;
	unsigned char __padding__ : 2;
} rw_packet_t;

typedef struct __attribute__((packed)){
	unsigned char led_manual_mode : 1;
	unsigned char program_addr : 5;
	unsigned char leds : 8;
	unsigned char __padding__ : 2;
} program_leds_packet_t;

#define PAYLOAD_SIZE 2
#define CHECKSUM_SIZE 1
#define VIRTUAL_MEM_SIZE 32
#define MAGIC_NUMBER_BYTE_REPEATS (4)

#define PACKET_SIZE (MAGIC_NUMBER_BYTE_REPEATS + sizeof(header_meta_t) + PAYLOAD_SIZE + sizeof(char))

typedef struct __attribute__((packed)){
  union {
    unsigned char magic[MAGIC_NUMBER_BYTE_REPEATS]; // 4
    header_meta_t header;                           // 1
    unsigned char payload[PAYLOAD_SIZE];            // 2
    unsigned char checksum[CHECKSUM_SIZE];          // 1
  };
  unsigned char __bytes__[PACKET_SIZE];
} magic_prefixed_packet_t;

const unsigned char MAGIC_NUMBER_BYTE = 0x5A;

unsigned char magic_number_bytes_read = 0, payload_bytes_read = 0, checksum_bytes_read = 0;
unsigned char packet_identifier, versioning;
unsigned char payload_buffer[PAYLOAD_SIZE], checksum_buffer[CHECKSUM_SIZE];

int virtual_memory[VIRTUAL_MEM_SIZE];

magic_prefixed_packet_t magic_prefixed_packet;
rw_packet_t rw_packet;
program_leds_packet_t program_leds_packet;

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

void rw_packet_processor(const char *buf){
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

void program_leds_packet_processor(const char *buf){
	memcpy(&program_leds_packet, buf, sizeof(program_leds_packet_t));
	if (program_leds_packet.led_manual_mode){
		*led_peripheral = program_leds_packet.leds;
	} else {
		unsigned char program_addr = program_leds_packet.program_addr;
		if(program_addr < 1) (*programs[program_addr])();
	}
}

void (*packet_processors[2])(const char *) = {
	rw_packet_processor,
	program_leds_packet_processor
};

void process_packet(const char *buf, const unsigned char id){
	if (id < 2) (*packet_processors[id])(buf);
}

wifi_bridge_fsm_state fsm(wifi_bridge_fsm_state s, char c){
	wifi_bridge_fsm_state state = s;
	switch(state){
		case IDLE:
			{
				putChr('I');
				magic_number_bytes_read = 0;
				payload_bytes_read = 0;
				checksum_bytes_read = 0;
				if (c == MAGIC_NUMBER_BYTE){
					magic_number_bytes_read++;
					state = LISTENING;
				}
			}
			break;
		case LISTENING: 
			{
				putChr('L');
				if (c != MAGIC_NUMBER_BYTE) state = IDLE;
				if (c == MAGIC_NUMBER_BYTE && ++magic_number_bytes_read == MAGIC_NUMBER_BYTE_REPEATS) state = HEADER_START;
			}
			break;
		case HEADER_START:
			{
				putChr('H');
				header_meta_t header;
				header.meta = c;
							
				packet_identifier = header.meta_bitfield.packet_identifier;
				versioning = header.meta_bitfield.versioning;

				magic_prefixed_packet.header = header;

				if (packet_identifier == NULL || packet_identifier > 2) state = IDLE;
				state = PAYLOAD_START;
			}
			break;
		case PAYLOAD_START:
			{
				payload_buffer[payload_bytes_read] = c;
				if (versioning == 1 && ++payload_bytes_read == PAYLOAD_SIZE){
					memcpy(payload_buffer, &magic_prefixed_packet.payload, payload_bytes_read);
					state = CHECKSUM_START;
				}
			}
			break;
		case CHECKSUM_START:
			{
				checksum_buffer[checksum_bytes_read] = c;
				if (versioning == 1 && ++checksum_bytes_read == CHECKSUM_SIZE){
					memcpy(checksum_buffer, &magic_prefixed_packet.checksum, checksum_bytes_read);
					state = PROCESSING;
				}
			}
			break;
		case PROCESSING:
			{
				unsigned char crc = crc8(payload_buffer, PAYLOAD_SIZE);

				if (versioning == 1 && magic_prefixed_packet.checksum[0] != crc){
					state = IDLE;
					break;
				}

				process_packet(payload_buffer, packet_identifier);
				
				state = IDLE;
			}
			break;
		default:
			break;
	}
	return state;
}

int main(int argc, char **argv) {
	wifi_bridge_fsm_state state = IDLE;
	while(1){
		char c;
		if (!UART_is_RX_empty()) {
			c = *UART_RX_DATA_ADDR;
			putChr(c);
			state = fsm(state, c);
		} 
	}
	return 0;
}
