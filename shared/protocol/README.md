# SecondScreen TCP Streaming Protocol (v1)

## 1. Overview

The SecondScreen TCP streaming protocol provides a lightweight, framed transport for transferring desktop screen frames from a Windows host to an Android client over a local network.

Because TCP is a continuous byte stream without inherent packet boundaries, this protocol specifies a fixed-length header preceding every frame payload. This guarantees that clients can safely reconstruct complete frames from fragmented network reads.

---

## 2. Packet Framing Specification

Every packet consists of a **20-byte fixed header** followed immediately by the **payload bytes**.

All integer fields are encoded in **Network Byte Order (Big-Endian)**.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic ('SS01')                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Type (0x01)  |                   Reserved                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                      Timestamp (ms, 64-bit)                   +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                        Payload Bytes                          |
|                     (JPEG Image Data)                         |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field Definitions

| Field | Size | Type | Description |
|---|---|---|---|
| **Magic** | 4 bytes | ASCII (`SS01`) | Identifies SecondScreen protocol packets (`0x53, 0x53, 0x30, 0x31`). Packets without this magic header are rejected immediately. |
| **Type** | 1 byte | uint8 | Packet type:<br>• `0x01`: Video Frame (JPEG)<br>• `0x02`: Ping / Keep-alive<br>• `0x03`: Pong / Heartbeat ack |
| **Reserved** | 3 bytes | Bytes | Reserved for future flags (compression, intra-frame indicator). Must be `0x00 0x00 0x00`. |
| **Payload Length** | 4 bytes | uint32 (BE) | Length of following image payload in bytes. Maximum allowed size is 10 MB (`10,485,760` bytes). |
| **Timestamp** | 8 bytes | uint64 (BE) | Host capture epoch time in milliseconds for calculating latency and frame age. |
| **Payload** | Variable | Bytes | Raw JPEG binary payload (begins with SOI `0xFF 0xD8` and ends with EOI `0xFF 0xD9`). |

---

## 3. Streaming Rules & Flow Control

1. **Unbounded Memory Protection**:
   - The client parses the 20-byte header first.
   - If `payload_length > 10 * 1024 * 1024` or `payload_length == 0`, the client aborts and closes the connection to prevent memory exhaustion.
2. **TCP Chunk Reassembly**:
   - Network socket reads arrive in arbitrary chunk sizes (e.g. 1460 bytes MTU, 8 KB, 64 KB).
   - Receiver maintains an internal accumulator buffer. It accumulates incoming bytes until at least 20 bytes exist for the header, then accumulates until `20 + payload_length` bytes exist, extracts the full frame, and shifts the remaining buffer.
3. **Queue Dropping & Freshness**:
   - On the Windows host: If the TCP send buffer cannot absorb a new frame immediately (e.g., client network is slow), the server drops the frame rather than buffering stale frames indefinitely.
   - On the Android client: Only the latest decoded frame is held for rendering. If a new frame arrives before the previous frame is finished rendering, the UI updates directly to the newest frame.

---

## 4. Default Connection Settings

- **Default Port**: `52025` (TCP)
- **Default Resolution**: 1280x720 (conservative scaling for Wi-Fi prototype)
- **Target Frame Rate**: 10–30 FPS (configurable)
- **JPEG Quality**: 65 (optimal balance between quality and bandwidth)
