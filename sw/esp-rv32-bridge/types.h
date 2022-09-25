#ifndef TYPES_H
#define TYPES_H

#define PAYLOAD_SIZE 2
#define CHECKSUM_SIZE 1
#define VIRTUAL_MEM_SIZE 32
#define MAGIC_NUMBER_BYTE_REPEATS (4)

#define PACKET_SIZE (MAGIC_NUMBER_BYTE_REPEATS + sizeof(header_meta_t) + PAYLOAD_SIZE + sizeof(char))

typedef enum {
	IDLE,
	LISTENING,
	HEADER,
	PAYLOAD,
	CHECKSUM,
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

typedef struct __attribute__((packed)){
  union {
    unsigned char magic[MAGIC_NUMBER_BYTE_REPEATS]; // 4
    header_meta_t header;                           // 1
    unsigned char payload[PAYLOAD_SIZE];            // 2
    unsigned char checksum[CHECKSUM_SIZE];          // 1
  };
  unsigned char __bytes__[PACKET_SIZE];
} magic_prefixed_packet_t;

#endif
