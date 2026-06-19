#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>

#define PACKET_SYNC_0 (0x50u) /* 'P' */
#define PACKET_SYNC_1 (0x53u) /* 'S' */
#define PACKET_VERSION (0x01u)
#define PACKET_MAX_PAYLOAD_SIZE (64u)
#define PACKET_HEADER_SIZE (7u)
#define PACKET_CRC_SIZE (2u)
#define PACKET_MAX_FRAME_SIZE \
    (PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD_SIZE + PACKET_CRC_SIZE)

typedef enum
{
    PACKET_TYPE_HEALTH = 0x01u,
    PACKET_TYPE_STATUS = 0x02u,
    PACKET_TYPE_IMU = 0x03u,
    PACKET_TYPE_SENSOR = 0x04u,
    PACKET_TYPE_EVENT = 0x05u,
    PACKET_TYPE_ACK = 0x06u,
    PACKET_TYPE_REJECT = 0x07u,
    PACKET_TYPE_DENY = 0x08u
} PacketType;

uint16_t packet_crc16_ccitt_false(const uint8_t *data, size_t length);

size_t packet_encode(uint8_t *out,
                     size_t out_capacity,
                     PacketType type,
                     uint16_t sequence,
                     const uint8_t *payload,
                     uint8_t payload_length);

size_t packet_put_u8(uint8_t *buffer, size_t offset, uint8_t value);
size_t packet_put_u16_le(uint8_t *buffer, size_t offset, uint16_t value);
size_t packet_put_u32_le(uint8_t *buffer, size_t offset, uint32_t value);
size_t packet_put_i16_le(uint8_t *buffer, size_t offset, int16_t value);

#endif
