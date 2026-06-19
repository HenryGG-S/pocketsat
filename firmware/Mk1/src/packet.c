#include "packet.h"

#include <string.h>

uint16_t packet_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;

    for (size_t i = 0u; i < length; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);

        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

size_t packet_put_u8(uint8_t *buffer, size_t offset, uint8_t value)
{
    buffer[offset] = value;
    return offset + 1u;
}

size_t packet_put_u16_le(uint8_t *buffer, size_t offset, uint16_t value)
{
    buffer[offset] = (uint8_t)(value & 0xFFu);
    buffer[offset + 1u] = (uint8_t)((value >> 8) & 0xFFu);
    return offset + 2u;
}

size_t packet_put_u32_le(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset] = (uint8_t)(value & 0xFFu);
    buffer[offset + 1u] = (uint8_t)((value >> 8) & 0xFFu);
    buffer[offset + 2u] = (uint8_t)((value >> 16) & 0xFFu);
    buffer[offset + 3u] = (uint8_t)((value >> 24) & 0xFFu);
    return offset + 4u;
}

size_t packet_put_i16_le(uint8_t *buffer, size_t offset, int16_t value)
{
    return packet_put_u16_le(buffer, offset, (uint16_t)value);
}

size_t packet_encode(uint8_t *out,
                     size_t out_capacity,
                     PacketType type,
                     uint16_t sequence,
                     const uint8_t *payload,
                     uint8_t payload_length)
{
    size_t frame_length =
        PACKET_HEADER_SIZE + (size_t)payload_length + PACKET_CRC_SIZE;
    uint16_t crc = 0u;
    size_t offset = 0u;

    if (out == NULL)
    {
        return 0u;
    }

    if (payload_length > PACKET_MAX_PAYLOAD_SIZE)
    {
        return 0u;
    }

    if (out_capacity < frame_length)
    {
        return 0u;
    }

    if (payload_length != 0u && payload == NULL)
    {
        return 0u;
    }

    offset = packet_put_u8(out, offset, PACKET_SYNC_0);
    offset = packet_put_u8(out, offset, PACKET_SYNC_1);
    offset = packet_put_u8(out, offset, PACKET_VERSION);
    offset = packet_put_u8(out, offset, (uint8_t)type);
    offset = packet_put_u16_le(out, offset, sequence);
    offset = packet_put_u8(out, offset, payload_length);

    if (payload_length != 0u)
    {
        memcpy(&out[offset], payload, payload_length);
        offset += payload_length;
    }

    /* CRC covers VERSION through PAYLOAD, excluding sync bytes and CRC field. */
    crc = packet_crc16_ccitt_false(&out[2], 5u + (size_t)payload_length);

    offset = packet_put_u16_le(out, offset, crc);

    return offset;
}
