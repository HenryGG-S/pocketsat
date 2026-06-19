#!/usr/bin/env python3
"""Decode PocketSat Mk1 binary telemetry frames.

Usage examples:
    python3 scripts/decode_packets.py capture.bin
    cat capture.bin | python3 scripts/decode_packets.py
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from typing import BinaryIO

SYNC = b"PS"
VERSION = 0x01
MAX_PAYLOAD = 64

PACKET_TYPES = {
    0x01: "HEALTH",
    0x02: "STATUS",
    0x03: "IMU",
    0x04: "SENSOR",
    0x05: "EVENT",
    0x06: "ACK",
    0x07: "REJECT",
    0x08: "DENY",
}

MODES = {
    0: "BOOT",
    1: "SAFE",
    2: "NOMINAL",
    3: "PAYLOAD",
}

FAULTS = {
    0: "NONE",
    1: "SENSOR_MISSING",
    2: "OVERTEMP",
    3: "COMMAND_STORM",
    4: "APP_STALL",
}


@dataclass(frozen=True)
class Frame:
    version: int
    packet_type: int
    sequence: int
    payload: bytes
    crc_expected: int
    crc_actual: int


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def u16_le(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def u32_le(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "little")


def parse_frames(blob: bytes) -> list[Frame]:
    frames: list[Frame] = []
    i = 0

    while i <= len(blob) - 7:
        if blob[i : i + 2] != SYNC:
            i += 1
            continue

        version = blob[i + 2]
        packet_type = blob[i + 3]
        sequence = u16_le(blob, i + 4)
        payload_len = blob[i + 6]

        if version != VERSION or payload_len > MAX_PAYLOAD:
            i += 1
            continue

        frame_len = 7 + payload_len + 2
        if i + frame_len > len(blob):
            break

        payload_start = i + 7
        payload_end = payload_start + payload_len
        payload = blob[payload_start:payload_end]
        expected = u16_le(blob, payload_end)
        actual = crc16_ccitt_false(blob[i + 2 : payload_end])

        if actual == expected:
            frames.append(
                Frame(
                    version=version,
                    packet_type=packet_type,
                    sequence=sequence,
                    payload=payload,
                    crc_expected=expected,
                    crc_actual=actual,
                )
            )
            i += frame_len
        else:
            i += 1

    return frames


def decode_health(frame: Frame) -> dict[str, object]:
    payload = frame.payload
    if len(payload) != 28:
        return {
            "packet": "HEALTH",
            "sequence": frame.sequence,
            "error": f"bad_payload_length:{len(payload)}",
        }

    mode = payload[12]
    fault = payload[13]

    return {
        "packet": "HEALTH",
        "sequence": frame.sequence,
        "tick_ms": u32_le(payload, 0),
        "telemetry_seq": u32_le(payload, 4),
        "health_counter": u32_le(payload, 8),
        "mode": MODES.get(mode, f"UNKNOWN({mode})"),
        "mode_raw": mode,
        "fault": FAULTS.get(fault, f"UNKNOWN({fault})"),
        "fault_raw": fault,
        "app_age_ms": u32_le(payload, 14),
        "app_tick_age_ms": u32_le(payload, 18),
        "uart_rx_used": u16_le(payload, 22),
        "uart_rx_overflows": u32_le(payload, 24),
        "crc": f"0x{frame.crc_actual:04X}",
    }


def decode_frame(frame: Frame) -> dict[str, object]:
    if frame.packet_type == 0x01:
        return decode_health(frame)

    return {
        "packet": PACKET_TYPES.get(frame.packet_type, f"UNKNOWN({frame.packet_type})"),
        "sequence": frame.sequence,
        "payload_hex": frame.payload.hex(" ").upper(),
        "crc": f"0x{frame.crc_actual:04X}",
    }


def read_input(path: str | None) -> bytes:
    if path is None or path == "-":
        stream: BinaryIO = sys.stdin.buffer
        return stream.read()

    with open(path, "rb") as f:
        return f.read()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", help="binary capture file, or stdin if omitted")
    args = parser.parse_args()

    blob = read_input(args.path)
    frames = parse_frames(blob)

    for frame in frames:
        print(json.dumps(decode_frame(frame), separators=(",", ":")))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
