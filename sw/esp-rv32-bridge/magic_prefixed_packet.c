#include "magic_prefixed_packet.h"

magic_prefixed_packet_t *init_magic_prefixed_packet(unsigned char packet_id,
                                                    unsigned char *payload) {
  magic_prefixed_packet_t *p = (magic_prefixed_packet_t *)malloc(sizeof(*p));
  if (!p) {
    perror("malloc failed");
    exit(1);
  }
  header_meta_t header;
  header.meta_bitfield.packet_identifier = packet_id;
  header.meta_bitfield.versioning = 1;
  for (uint8_t i = 0; i < MAGIC_NUMBER_BYTE_REPEATS || i < PAYLOAD_SIZE; i++) {
    if (i < MAGIC_NUMBER_BYTE_REPEATS) p->magic[i] = MAGIC_NUMBER_BYTE;
    if (i < PAYLOAD_SIZE) p->payload[i] = payload[i];
  }
  p->header = header;
  p->checksum = sae_j1850_crc8(payload, PAYLOAD_SIZE);
  return p;
}
