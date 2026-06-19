# PocketSat Mk1 Binary Telemetry Protocol

Status: experimental Mk1.2 protocol baseline  
Transport: USART2/ST-LINK VCOM at 115200 baud  
Byte order: little-endian for all multi-byte integer fields

## 1. Design intent

The binary packet layer is a machine-readable telemetry channel for compact, checksummed records. It does not replace the ASCII operator channel. Commands, acknowledgements, rejects, denies, events, and most debug telemetry remain ASCII unless explicitly migrated later.

Mk1.2 introduces binary emission for periodic `HEALTH` telemetry only. This keeps the board operable from a normal serial terminal while proving the packet framing, CRC, host decoder, and telemetry format switching path.

## 2. Frame format

```text
+--------+--------+---------+------+--------+-----+---------+--------+
| SYNC_0 | SYNC_1 | VERSION | TYPE | SEQ    | LEN | PAYLOAD | CRC16  |
| 1 byte | 1 byte | 1 byte  | 1 B  | uint16 | u8  | LEN B   | uint16 |
+--------+--------+---------+------+--------+-----+---------+--------+
```

Constants:

```text
SYNC_0  = 0x50  ('P')
SYNC_1  = 0x53  ('S')
VERSION = 0x01
```

`SEQ` is a packet sequence counter, separate from the ASCII telemetry sequence counter. It wraps naturally at `uint16_t` overflow.

`LEN` is the number of payload bytes. The maximum Mk1 payload size is 64 bytes.

## 3. CRC

CRC is CRC-16/CCITT-FALSE:

```text
Polynomial: 0x1021
Initial:    0xFFFF
RefIn:      false
RefOut:     false
XorOut:     0x0000
```

CRC coverage starts at `VERSION` and ends at the final payload byte. The two sync bytes and the CRC field itself are excluded.

The CRC is appended little-endian: low byte first, then high byte.

## 4. Packet types

```c
typedef enum
{
    PACKET_TYPE_HEALTH = 0x01,
    PACKET_TYPE_STATUS = 0x02,
    PACKET_TYPE_IMU    = 0x03,
    PACKET_TYPE_SENSOR = 0x04,
    PACKET_TYPE_EVENT  = 0x05,
    PACKET_TYPE_ACK    = 0x06,
    PACKET_TYPE_REJECT = 0x07,
    PACKET_TYPE_DENY   = 0x08
} PacketType;
```

Only `PACKET_TYPE_HEALTH` is emitted in Mk1.2. The remaining IDs are reserved so the protocol can evolve without renumbering early packet types.

## 5. HEALTH payload, type `0x01`

```text
Offset  Size  Field              Type
0       4     tick_ms            uint32
4       4     telemetry_seq      uint32
8       4     health_counter     uint32
12      1     mode               uint8
13      1     fault              uint8
14      4     app_age_ms         uint32
18      4     app_tick_age_ms    uint32
22      2     uart_rx_used       uint16
24      4     uart_rx_overflows  uint32
```

Total payload length: 28 bytes.

`mode` and `fault` use the firmware enum numeric values from `app_types.h`.

Current mode values:

```text
0 = BOOT
1 = SAFE
2 = NOMINAL
3 = PAYLOAD
```

Current fault values:

```text
0 = NONE
1 = SENSOR_MISSING
2 = OVERTEMP
3 = COMMAND_STORM
4 = APP_STALL
```

## 6. Operator commands

The ASCII command interface remains active in both telemetry formats.

```text
SET_TLM_FORMAT ASCII
SET_TLM_FORMAT BINARY
```

`SET_TLM_FORMAT BINARY` switches periodic health telemetry from ASCII `TLM,...type=HEALTH` records to binary `PACKET_TYPE_HEALTH` frames. Other operator responses remain ASCII.

Recommended test flow:

```text
GET_HEALTH
SET_TLM_FORMAT BINARY
# observe binary health frames with scripts/decode_packets.py
SET_TLM_FORMAT ASCII
GET_HEALTH
```

## 7. Test vector

Example decoded input values:

```text
tick_ms=1000
telemetry_seq=7
health_counter=3
mode=1
fault=0
app_age_ms=1000
app_tick_age_ms=0
uart_rx_used=0
uart_rx_overflows=0
packet_seq=1
```

Expected frame bytes:

```text
50 53 01 01 01 00 1C E8 03 00 00 07 00 00 00 03
00 00 00 01 00 E8 03 00 00 00 00 00 00 00 00 00
00 00 00 63 B1
```

Frame length: 37 bytes.  
CRC: `0xB163`, emitted as `63 B1`.

## 8. Decoder behaviour

A host decoder should:

1. Search for `0x50 0x53` sync bytes.
2. Read the fixed header.
3. Reject frames with unsupported version or excessive length.
4. Read payload and CRC.
5. Verify CRC before decoding fields.
6. Resynchronise after corrupt bytes by continuing the sync search.

The reference decoder is `scripts/decode_packets.py`.
