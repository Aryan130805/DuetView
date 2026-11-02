# SecondScreen Streaming Protocols

This directory contains the protocol specifications for the SecondScreen Windows-to-Android display pipeline.

- **Protocol v1 (`SS01`)**: Experimental JPEG-over-TCP prototype (Phase 3 fallback).
- **Protocol v2 (`SS02`)**: Production H.264 video streaming protocol (Phase 4).

---

## 1. Protocol v2 (`SS02`) H.264 Streaming Protocol Specification

### 1.1 Overview

The `SS02` protocol transfers encoded H.264 video bitstreams, codec parameters, and control messages over a continuous network byte stream (initially TCP on port `52025`).

Unlike JPEG images, H.264 is a continuous, stateful video bitstream consisting of Network Abstraction Layer (NAL) units. Video decoders require:
1. **Out-of-band or in-band codec configuration**: Sequence Parameter Sets (SPS) and Picture Parameter Sets (PPS) before decoding starts.
2. **Access Unit boundaries**: Clear delimitation between Instantaneous Decoder Refresh (IDR) keyframes and predicted delta frames (P-frames).
3. **Resilience to packet fragmentation**: A robust framing envelope ensuring TCP read fragmentation never corrupts NAL units or causes unaligned bitstream feeding into hardware decoders like Android `MediaCodec`.

---

### 1.2 Binary Header Specification

Every packet begins with a fixed-size **24-byte header**, followed immediately by `payload_length` bytes of payload.

All integer fields are strictly serialized in **Network Byte Order (Big-Endian)**.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic ('SS02')                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Version (0x0002)        |          Packet Type          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Frame Sequence Number                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                 Timestamp (Microseconds, 64-bit)              +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                        Payload Bytes                          |
|                 (Length = 'Payload Length')                   |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 1.3 Field Definitions & Offsets

| Offset (Bytes) | Field Name | Data Type | Value / Description |
|---|---|---|---|
| `0 .. 3` | `magic` | 4 × uint8 | ASCII `'S', 'S', '0', '2'` (`0x53, 0x53, 0x30, 0x32`). Packets lacking this magic sequence are invalid. |
| `4 .. 5` | `version` | uint16 (BE) | Protocol major version. Must equal `2` (`0x0002`). |
| `6 .. 7` | `packet_type` | uint16 (BE) | Message type discriminator (see Section 1.4). |
| `8 .. 11` | `sequence_number` | uint32 (BE) | Monotonically increasing sequence number per sender stream. Starts at `0`. |
| `12 .. 15` | `payload_length` | uint32 (BE) | Length of following payload in bytes. Maximum permitted size: `16,777,216` bytes (16 MB). |
| `16 .. 23` | `timestamp_us` | uint64 (BE) | Capture / presentation epoch timestamp in **microseconds** (µs) using a monotonic clock. |
| `24 .. 24+L` | `payload` | `uint8[L]` | Binary payload bytes whose interpretation depends on `packet_type`. |

---

### 1.4 Packet Types

| Type ID | Symbol | Direction | Description | Payload Requirements |
|---|---|---|---|---|
| `0x0001` | `PACKET_TYPE_CONFIG` | Host → Client | Codec configuration containing H.264 SPS and PPS NAL units. | Mandatory `payload_length > 0`. Annex B format. |
| `0x0002` | `PACKET_TYPE_KEYFRAME` | Host → Client | H.264 IDR (Instantaneous Decoder Refresh) access unit. | Mandatory `payload_length > 0`. Begins with IDR slice (NAL unit type 5). |
| `0x0003` | `PACKET_TYPE_DELTA` | Host → Client | Predicted frame (P-slice / non-IDR). | Mandatory `payload_length > 0`. NAL unit type 1. |
| `0x0004` | `PACKET_TYPE_HEARTBEAT` | Either | Keepalive / roundtrip latency probe. | Optional payload (0 bytes or echo token). |
| `0x0005` | `PACKET_TYPE_HEARTBEAT_ACK` | Either | Keepalive response echoing the probe timestamp. | Matches received heartbeat payload. |
| `0x0006` | `PACKET_TYPE_KEYFRAME_REQ` | Client → Host | Client requests the host encoder to force an immediate IDR keyframe (e.g. after decoder reset). | Typically 0 bytes payload. |
| `0x0007` | `PACKET_TYPE_SHUTDOWN` | Either | Graceful stream termination notice. | 0 bytes payload. |
| `0x0008` | `PACKET_TYPE_RECEIVER_REPORT` | Client → Host | Telemetry feedback (received FPS, decoded FPS, drops, delay). | Fixed 20-byte payload. |
| `0x0009` | `PACKET_TYPE_PAIR_REQ` | Client → Host | Authenticated pairing request containing short-lived token and client metadata. | JSON payload string. |
| `0x000A` | `PACKET_TYPE_PAIR_RESP` | Host → Client | Pairing response with status code (`0` = OK, `1` = Invalid, `2` = Expired). | JSON payload string. |
| `0x000B` | `PACKET_TYPE_POINTER_EVENT` | Client → Host | Upstream pointer movement, button down/up, double-click, and drag. | Fixed 16-byte payload. |
| `0x000C` | `PACKET_TYPE_KEYBOARD_EVENT` | Client → Host | Upstream key down/up and direct Unicode character insertion. | Fixed 12-byte payload. |
| `0x000D` | `PACKET_TYPE_SCROLL_EVENT` | Client → Host | Upstream vertical and horizontal scroll wheel deltas. | Fixed 16-byte payload. |

---

### 1.5 Payload Format Details

#### Codec Configuration (`PACKET_TYPE_CONFIG`)
Sent immediately upon client connection or after stream format/resolution changes.
- Contains the H.264 **Sequence Parameter Set (SPS)** and **Picture Parameter Set (PPS)** formatted in standard Annex B notation:
  ```
  [00 00 00 01] [SPS NAL Unit (type 7)] [00 00 00 01] [PPS NAL Unit (type 8)]
  ```
- Android `MediaCodec` extracts these into `csd-0` (SPS) and `csd-1` (PPS) buffers before transitioning to the `INFO_OUTPUT_FORMAT_CHANGED` state.

#### Video Access Units (`KEYFRAME` and `DELTA`)
- Each packet contains a complete H.264 access unit in Annex B framing:
  ```
  [00 00 00 01] [NAL Unit Header] [RBSP data...]
  ```
- One video packet corresponds to one discrete presentation frame timestamp (`timestamp_us`).

#### Receiver Telemetry Report (`PACKET_TYPE_RECEIVER_REPORT`)
Sent periodically (e.g. 1 Hz) from the client to the host during active streaming to enable adaptive bitrate and frame-rate control:
- Fixed 20-byte payload formatted in big-endian network byte order:
  - `0 .. 3`: `uint32` - Measured received FPS at receiver.
  - `4 .. 7`: `uint32` - Measured decoded/rendered FPS.
  - `8 .. 11`: `uint32` - Cumulative count of dropped or rejected frames.
  - `12 .. 15`: `uint32` - Estimated receiver buffer queue delay in milliseconds.
  - `16 .. 19`: `uint32` - Reserved flags / future expansion.

---

#### Pairing Request & Response (`PACKET_TYPE_PAIR_REQ` & `PACKET_TYPE_PAIR_RESP`)
Used during initial QR code pairing and device registration:
- `PACKET_TYPE_PAIR_REQ` (`0x0009`): Client sends JSON payload `{"token":"<hex>","client":"<ClientName>"}`.
- `PACKET_TYPE_PAIR_RESP` (`0x000A`): Host replies with JSON payload `{"status":0,"message":"Paired successfully"}` where status `0` indicates success, `1` indicates invalid token, and `2` indicates expired token.

#### Pointer Event (`PACKET_TYPE_POINTER_EVENT` 0x000B)
Transmits touch and mouse interaction from client to host:
- Fixed 16-byte payload (big-endian):
  - `0 .. 1`: `uint16` - Event Type: `1` = Move, `2` = ButtonDown, `3` = ButtonUp, `4` = DoubleClick.
  - `2`: `uint8` - Button ID: `0` = None, `1` = Left, `2` = Right, `3` = Middle.
  - `3`: `uint8` - Modifiers bitmask: `0x01` = Shift, `0x02` = Ctrl, `0x04` = Alt, `0x08` = Meta/Win.
  - `4 .. 7`: `float32` (IEEE 754 BE) - Normalized X coordinate in $[0.0, 1.0]$.
  - `8 .. 11`: `float32` (IEEE 754 BE) - Normalized Y coordinate in $[0.0, 1.0]$.
  - `12 .. 15`: `uint32` - Flags / Pointer ID.

#### Keyboard Event (`PACKET_TYPE_KEYBOARD_EVENT` 0x000C)
Transmits keystrokes and Unicode text input:
- Fixed 12-byte payload (big-endian):
  - `0 .. 1`: `uint16` - Event Type: `1` = KeyDown, `2` = KeyUp, `3` = UnicodeChar.
  - `2`: `uint8` - Modifiers bitmask.
  - `3`: `uint8` - Reserved (0).
  - `4 .. 7`: `uint32` - Virtual Key code (Win32 VK_* or USB scancode).
  - `8 .. 11`: `uint32` - Unicode code point (UTF-32) for direct text typing.

#### Scroll Event (`PACKET_TYPE_SCROLL_EVENT` 0x000D)
Transmits two-finger or wheel scroll deltas:
- Fixed 16-byte payload (big-endian):
  - `0 .. 3`: `float32` - Normalized X coordinate in $[0.0, 1.0]$.
  - `4 .. 7`: `float32` - Normalized Y coordinate in $[0.0, 1.0]$.
  - `8 .. 11`: `int32` - Horizontal scroll delta (negative = left, positive = right).
  - `12 .. 15`: `int32` - Vertical scroll delta (standard WHEEL_DELTA increments).

#### Heartbeat & Heartbeat ACK (`PACKET_TYPE_HEARTBEAT` & `PACKET_TYPE_HEARTBEAT_ACK`)
Used for lightweight online/offline host presence detection without initiating video capture or encoding:
- `PACKET_TYPE_HEARTBEAT` (`0x0004`): Sent by client with 0-byte payload or 8-byte timestamp.
- `PACKET_TYPE_HEARTBEAT_ACK` (`0x0005`): Host responds immediately echoing the sequence and timestamp without starting desktop capture.

### 1.6 Network Framing & Validation Rules

Because TCP deliver bytes in arbitrary chunks, the receiver must strictly enforce:

1. **Two-Stage Frame Accumulation**:
   - Accumulate incoming socket bytes until at least **24 bytes** are present.
   - Parse and validate the 24-byte header fields **before allocating memory** for the payload.
   - If header validation fails, immediately terminate the connection and close the socket.
   - Once validated, accumulate exactly `payload_length` payload bytes.
   - Dispatch the complete packet to the decoder/event handler and remove the consumed `24 + payload_length` bytes from the accumulator.

2. **Strict Validation Invariants**:
   - `magic == 0x53533032`: Disconnect immediately on mismatch.
   - `version == 2`: Disconnect immediately if incompatible.
   - `payload_length <= 16,777,216`: Reject oversized payloads immediately to protect against memory exhaustion attacks.
   - Video types (`0x0001`, `0x0002`, `0x0003`) MUST have `payload_length > 0`.
   - Control types (`0x0004` .. `0x0007`) may have `payload_length == 0`.

3. **Reconnection & Loss Recovery**:
   - If the network drops or the Android decoder encounters an unrecoverable error, the client sends `PACKET_TYPE_KEYFRAME_REQ` (or reconnects).
   - Upon receiving `PACKET_TYPE_KEYFRAME_REQ`, the Windows host encoder invokes `RequestKeyframe()`, ensuring the next video packet transmitted is `PACKET_TYPE_KEYFRAME` (IDR).

---

## 2. Protocol v1 (`SS01`) JPEG Streaming Protocol (Phase 3 Fallback)

Retained for backward compatibility and fallback diagnostic mode:

- **Magic**: `'SS01'` (`0x53, 0x53, 0x30, 0x31`)
- **Header Size**: 20 bytes
- **Payload**: Raw JPEG frame (`0xFF 0xD8 ... 0xFF 0xD9`)
- **Max Payload**: 10 MB (`10,485,760` bytes)
- **Port**: `52025` (TCP)
