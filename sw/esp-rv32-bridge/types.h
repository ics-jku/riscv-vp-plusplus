#ifndef TYPES_H
#define TYPES_H

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

#endif
