import pytest
from protocol import (
    MAGIC,
    HEADER_SIZE,
    TYPE_FRAME,
    MAX_PAYLOAD_SIZE,
    encode_frame_packet,
    decode_header,
    ProtocolError,
)


def test_encode_and_decode_valid_frame():
    fake_jpeg = b"\xff\xd8\xff\xe0" + b"\x00" * 1024 + b"\xff\xd9"
    packet = encode_frame_packet(fake_jpeg, timestamp_ms=1700000000000)

    assert len(packet) == HEADER_SIZE + len(fake_jpeg)
    assert packet.startswith(MAGIC)

    header_bytes = packet[:HEADER_SIZE]
    pkt_type, payload_len, timestamp_ms = decode_header(header_bytes)

    assert pkt_type == TYPE_FRAME
    assert payload_len == len(fake_jpeg)
    assert timestamp_ms == 1700000000000
    assert packet[HEADER_SIZE:] == fake_jpeg


def test_reject_empty_payload():
    with pytest.raises(ProtocolError, match="Cannot encode empty frame"):
        encode_frame_packet(b"")


def test_reject_oversized_payload():
    oversized = b"X" * (MAX_PAYLOAD_SIZE + 1)
    with pytest.raises(ProtocolError, match="exceeds maximum allowed"):
        encode_frame_packet(oversized)


def test_decode_invalid_magic():
    bad_header = b"BAD!" + b"\x00" * 16
    with pytest.raises(ProtocolError, match="Invalid protocol magic"):
        decode_header(bad_header)


def test_decode_short_header():
    short_header = b"SS01" + b"\x00" * 10
    with pytest.raises(ProtocolError, match="Invalid header size"):
        decode_header(short_header)
