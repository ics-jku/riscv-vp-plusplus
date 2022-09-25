#include <stdio.h>
#include "platform.h"
#include "uart.h"
#include "util.h"
#include "crc.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "packet_processors.h"

unsigned long *led_peripheral = (unsigned long *)0x81000000;

const unsigned char MAGIC_NUMBER_BYTE = 0x5A;

unsigned char magic_number_bytes_read = 0, payload_bytes_read = 0, checksum_bytes_read = 0;
unsigned char packet_identifier = 0, versioning = 0;
unsigned char payload_buffer[PAYLOAD_SIZE], checksum_buffer[CHECKSUM_SIZE];

int virtual_memory[VIRTUAL_MEM_SIZE];

magic_prefixed_packet_t magic_prefixed_packet;
rw_packet_t rw_packet;
program_leds_packet_t program_leds_packet;

void process_packet(const char *buf, const unsigned char id){
    if (id == 0) rw_packet_processor(buf, &rw_packet, virtual_memory);
    if (id == 1) program_leds_packet_processor(buf, &program_leds_packet, led_peripheral);
}

wifi_bridge_fsm_state on_idle(char c){
    putChr('I');
	magic_number_bytes_read = 0;
	payload_bytes_read = 0;
	checksum_bytes_read = 0;
	if (c == MAGIC_NUMBER_BYTE){
	    magic_number_bytes_read++;
		return LISTENING;
	}
    return IDLE;
}

wifi_bridge_fsm_state on_listening(char c){
	putChr('L');
	if (c != MAGIC_NUMBER_BYTE) return IDLE;
	if (c == MAGIC_NUMBER_BYTE && ++magic_number_bytes_read == MAGIC_NUMBER_BYTE_REPEATS) return HEADER;
    return LISTENING;
}

wifi_bridge_fsm_state on_header(char c){
	putChr('H');
	header_meta_t header;
	header.meta = c;
							
	packet_identifier = header.meta_bitfield.packet_identifier;
	versioning = header.meta_bitfield.versioning;

	magic_prefixed_packet.header = header;

    if (packet_identifier == NULL || versioning == NULL || (versioning == 1 && packet_identifier > 2)){
        return IDLE;
    }

    return PAYLOAD;
}

wifi_bridge_fsm_state on_payload(char c){
    putChr('P');
	payload_buffer[payload_bytes_read] = c;
	if (versioning == 1 && ++payload_bytes_read == PAYLOAD_SIZE){
	    memcpy(payload_buffer, &magic_prefixed_packet.payload, payload_bytes_read);
		return CHECKSUM;
	}
    return PAYLOAD;
}

wifi_bridge_fsm_state on_checksum(char c){
    putChr('C');
	checksum_buffer[checksum_bytes_read] = c;
	if (versioning == 1 && ++checksum_bytes_read == CHECKSUM_SIZE){
	    memcpy(checksum_buffer, &magic_prefixed_packet.checksum, checksum_bytes_read);
		return PROCESSING;
	}
    return PAYLOAD;
}

wifi_bridge_fsm_state on_processing(char c){
    unsigned char crc = crc8(payload_buffer, PAYLOAD_SIZE);

	if (versioning == 1 && magic_prefixed_packet.checksum[0] != crc){
	    return IDLE;
	}

	process_packet(payload_buffer, packet_identifier);
				
	return IDLE;
}

wifi_bridge_fsm_state (*fsm_state_handlers[])(char) = {
    on_idle,
    on_listening,
    on_header,
    on_payload,
    on_checksum,
    on_processing
};


wifi_bridge_fsm_state fsm(wifi_bridge_fsm_state state, char c){
    return fsm_state_handlers[state](c);
}

int main(int argc, char **argv) {
	wifi_bridge_fsm_state state = IDLE;
    char c;
	while(1){
		if (!UART_is_RX_empty()) {
			c = *UART_RX_DATA_ADDR;
			putChr(c);
			state = fsm(state, c);
		} 
	}
	return 0;
}
