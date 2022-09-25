#ifndef MAGIC_PREFIXED_PACKET_H
#define MAGIC_PREFIXED_PACKET_H

#include <cstdio>
#include <cstdlib>

#include "sae_j1850_crc.h"
#include "types.h"

typedef struct __attribute__((packed)) {
  union {
    unsigned char magic[MAGIC_NUMBER_BYTE_REPEATS];  // 4
    header_meta_t header;                            // 1
    unsigned char payload[PAYLOAD_SIZE];             // 2
    unsigned char checksum;                          // 1
  };
  unsigned char __bytes__[PACKET_SIZE];
} magic_prefixed_packet_t;  // 4+1+2+1 = 8 bytes

magic_prefixed_packet_t *init_magic_prefixed_packet(unsigned char,
                                                    unsigned char *);

#endif
