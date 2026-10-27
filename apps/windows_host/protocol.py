"""
SecondScreen TCP Streaming Protocol Implementation (v1)
Handles serialization, framing, and validation for frame transmissions.
"""

import struct
import time
from typing import Tuple

MAGIC = b"SS01"
HEADER_SIZE = 20
MAX_PAYLOAD_SIZE = 10 * 1024 * 1024  # 10 MB maximum safe frame size

TYPE_FRAME = 0x01
TYPE_PING = 0x02
TYPE_PONG = 0x03


class ProtocolError(Exception):
    """Raised when framing or packet data is corrupted or invalid."""
    pass


def encode_frame_packet(jpeg_bytes: bytes, timestamp_ms: int = None) -> bytes:
    """
    Packs JPEG image bytes into a SecondScreen v1 protocol frame.

    Header structure (20 bytes, big-endian):
      - 4 bytes: Magic (b'SS01')
      - 1 byte: Packet Type (0x01)
      - 3 bytes: Reserved (b'\x00\x00\x00')
      - 4 bytes: Payload length (uint32)
      - 8 bytes: Timestamp in ms (uint64)
    """
    payload_len = len(jpeg_bytes)
    if payload_len == 0:
        raise ProtocolError("Cannot encode empty frame payload.")
    if payload_len > MAX_PAYLOAD_SIZE:
        raise ProtocolError(
            f"Frame payload size {payload_len} exceeds maximum allowed {MAX_PAYLOAD_SIZE} bytes."
        )

    if timestamp_ms is None:
        timestamp_ms = int(time.time() * 1000)

    # Format: 4s (magic) + B (type) + 3s (reserved) + I (payload_len) + Q (timestamp_ms)
    header = struct.pack(
        ">4sB3sIQ",
        MAGIC,
        TYPE_FRAME,
        b"\x00\x00\x00",
        payload_len,
        timestamp_ms,
    )
    return header + jpeg_bytes


def decode_header(header_bytes: bytes) -> Tuple[int, int, int]:
    """
    Decodes the 20-byte header.

    Returns:
        (packet_type, payload_length, timestamp_ms)

    Raises:
        ProtocolError: If header size, magic, or payload length is invalid.
    """
    if len(header_bytes) != HEADER_SIZE:
        raise ProtocolError(
            f"Invalid header size: expected {HEADER_SIZE} bytes, got {len(header_bytes)}."
        )

    magic, packet_type, _, payload_len, timestamp_ms = struct.unpack(
        ">4sB3sIQ", header_bytes
    )

    if magic != MAGIC:
        raise ProtocolError(f"Invalid protocol magic: {magic!r}, expected {MAGIC!r}.")

    if payload_len == 0 or payload_len > MAX_PAYLOAD_SIZE:
        raise ProtocolError(
            f"Invalid payload length: {payload_len} (limit is {MAX_PAYLOAD_SIZE} bytes)."
        )

    return packet_type, payload_len, timestamp_ms
