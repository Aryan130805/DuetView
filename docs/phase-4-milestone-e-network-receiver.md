# SecondScreen — Phase 4, Milestone E: SS02 Network Receiver Integration

## 1. Overview & Objective

Milestone E connects the **SS02 TCP streaming protocol**, the **Windows native H.264 capture host**, and the **Android native `SecondScreenDecoderBridge`** into a resilient, end-to-end streaming pipeline.

Prior milestones established:
- **Milestone B:** Low-latency DXGI desktop capture.
- **Milestone C:** Windows Media Foundation H.264 encoder and SS02 packet serialization.
- **Milestone D:** Native Android MediaCodec decoder and native `SurfaceView` platform view.

Milestone E fulfills the networking and synchronization layer:
1. Continuous, bounded byte stream ingestion off the Android UI thread.
2. Robust SS02/SS01 frame accumulation handling arbitrary TCP segmentation (split headers, multi-packet buffers, partial payloads).
3. Packet dispatch to the Android decoder bridge with strict readiness ordering (`CONFIG` &rarr; `KEYFRAME` &rarr; `DELTA`).
4. Low-latency upstream keyframe requests (`PACKET_TYPE_KEYFRAME_REQ`, `0x0006`) with 200 ms rate-limiting.
5. Automatic fallback to Phase 3 SS01 JPEG streaming when H.264 hardware/software decoding is unsupported on the client device.
6. Multithreaded Windows native streaming server (`TcpTransportServer`) with live client tracking and keyframe request processing.

---

## 2. Architecture & Data Flow

```
+----------------------------------------------------------------------------------------------------+
|                                    SecondScreen Streaming Architecture                             |
+----------------------------------------------------------------------------------------------------+
|                                                                                                    |
|  [Windows Native Host: secondscreen_engine.exe]                                                    |
|  +-------------------------+      +-------------------------+      +----------------------------+  |
|  |     DXGIManager         | ---> |    H264Encoder (MFT)    | ---> |     TcpTransportServer     |  |
|  | (Desktop Duplication)   |      | (Annex B SPS/PPS/IDR/P) |      | (Winsock2, Multithreaded)  |  |
|  +-------------------------+      +------------^------------+      +-------------+--------------+  |
|                                                |                                 |                 |
|                                                | ForceKeyframe()                 | SS02 Packets    |
|                                                |                                 | (Port 5000)     |
|                                                +----------------------------+    |                 |
|                                                                             |    |                 |
|  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ TCP Network Stream ~~~~~~~~~~~~~~~~~~~|~~~~|~~~~~~~~~~~~~~~~ |
|                                                                             |    |                 |
|  [Android Flutter / Native Client]                                          |    |                 |
|  +-------------------------+      +-------------------------+               |    v                 |
|  |  SecondScreenNativeBridge<---+ | DefaultConnectionService|<--------------+----+                 |
|  |  (MediaCodec / Surface) |      | - Bounded parsing       |  Upstream KEYFRAME_REQ (0x0006)       |
|  +------------^------------+      | - Readiness gatekeeper  |  (Rate-limited: 200 ms throttle)     |
|               |                   | - Automatic fallback    |                                      |
|               |                   +------------^------------+                                      |
|               |                                |                                                   |
|  +------------+------------+      +------------+------------+                                      |
|  | VideoSurfacePlatformView|      |   SS02 FrameAccumulator |                                      |
|  | (Direct GPU Render)     |      |  - 2-stage state machine|                                      |
|  +-------------------------+      |  - Buffer slicing       |                                      |
|                                   +-------------------------+                                      |
|                                                                                                    |
+----------------------------------------------------------------------------------------------------+
```

---

## 3. Protocol Framing & Accumulator Implementation

### 3.1 SS02 Wire Format
The SS02 protocol header is 24 bytes, big-endian (`shared/protocol/README.md`):

| Offset | Size (bytes) | Type | Field Name | Description |
|---|---|---|---|---|
| `0` | 4 | ASCII | `magic` | Magic identifier: `"SS02"` (`0x53 0x53 0x30 0x32`) |
| `4` | 2 | `uint16_t` | `version` | Protocol version (`0x0002`) |
| `6` | 2 | `uint16_t` | `packet_type` | `0x0001` CONFIG, `0x0002` KEYFRAME, `0x0003` DELTA, `0x0006` KEYFRAME_REQ, `0x0007` SHUTDOWN |
| `8` | 4 | `uint32_t` | `sequence_number` | Monotonically increasing sequence number |
| `12` | 8 | `uint64_t` | `timestamp_us` | Microsecond timestamp (capture/send time) |
| `20` | 4 | `uint32_t` | `payload_length` | Total payload length in bytes following header |

### 3.2 Dual SS02 / SS01 Frame Accumulator
Implemented in [`apps/android_app/lib/services/frame_accumulator.dart`](file:///d:/Second%20Display/apps/android_app/lib/services/frame_accumulator.dart):
- **Two-Stage State Machine:**
  - `_AccumulatorState.readingHeader`: Collects bytes until header size (24 bytes for SS02, 20 bytes for SS01) is met.
  - `_AccumulatorState.readingPayload`: Reads exactly `payloadLength` bytes into memory before dispatching `FramePacket`.
- **Validation Bounds:**
  - Maximum payload size is capped at 4 MB (`maxPacketSize = 4 * 1024 * 1024`) to eliminate memory exhaustion from malformed headers.
  - Enforces `payloadLength >= 4` for video packets containing a 4-byte `video_length` field.
  - Zero-copy windowing using `Uint8List.sublistView()` minimizes allocation overhead.
- **Dual-Protocol Detection:**
  - Detects `SS02` (`version == 2`) and transparently falls back to `SS01` (`version == 1`) when encountering legacy JPEG streams.

---

## 4. Packet Dispatch & Decoder Readiness State Machine

The client receiver in [`DefaultConnectionService`](file:///d:/Second%20Display/apps/android_app/lib/services/connection_service.dart) implements a deterministic readiness gatekeeper:

```
[Disconnected]
      |
      | connect(ip, port)
      v
[Awaiting Codec Config] <---------------------------------------------+
      |                                                                |
      | PACKET_TYPE_CONFIG (0x0001)                                    |
      v                                                                | Decoder Reset /
[Config Accepted, Awaiting Keyframe] <----+                            | Discontinuity /
      |                                   | Drop Delta &               | onKeyframeRequested
      | PACKET_TYPE_KEYFRAME (0x0002)     | Send KEYFRAME_REQ (0x0006) |
      v                                   | (Throttle: 200 ms)         |
[Streaming / Decoding Deltas] ------------+----------------------------+
      |
      | PACKET_TYPE_DELTA (0x0003) -> feedAccessUnit(isKeyframe: false)
```

### Dispatch Rules
1. **`PACKET_TYPE_CONFIG` (`0x0001`):**
   - Transmits Annex B SPS/PPS configuration bytes to `decoderBridge.feedConfigPacket()`.
   - Sets `_hasCodecConfig = true`.
2. **`PACKET_TYPE_KEYFRAME` (`0x0002`):**
   - Extracts encoded NAL access unit (IDR slice).
   - If `_hasCodecConfig == false`, requests keyframe and buffers/discards safely.
   - Calls `decoderBridge.feedAccessUnit(..., isKeyframe: true)`.
   - Sets `_hasReceivedKeyframe = true`.
3. **`PACKET_TYPE_DELTA` (`0x0003`):**
   - If `!_hasCodecConfig || !_hasReceivedKeyframe`: Drops delta packet immediately and sends `PACKET_TYPE_KEYFRAME_REQ` upstream to prevent corrupt reference decoding artifacts.
   - If ready: Calls `decoderBridge.feedAccessUnit(..., isKeyframe: false)`.
4. **`PACKET_TYPE_SHUTDOWN` (`0x0007`):**
   - Triggers clean client disconnection.

---

## 5. Upstream Keyframe Request Flow & Rate-Limiting

### 5.1 Protocol Serialization
Upstream keyframe requests use standard SS02 header serialization:
- Magic: `"SS02"`
- Version: `2`
- Packet Type: `0x0006` (`PACKET_TYPE_KEYFRAME_REQ`)
- Sequence: Monotonic client sequence
- Timestamp: System microseconds
- Payload Length: `0`

### 5.2 Trigger Paths & Rate Limiting
Requests are generated under two conditions:
1. Native decoder error or discontinuity notified via `decoderBridge.onKeyframeRequested`.
2. Arrival of delta frames prior to initial keyframe or following reconnection.

To avoid overwhelming the host and saturating network bandwidth, `DefaultConnectionService` enforces a strict 200 ms cooldown (`_minKeyframeIntervalMs = 200`). Subsequent triggers within the window are safely throttled.

### 5.3 Host Handling
`TcpTransportServer` runs a persistent background worker thread (`ClientWorkerThread`) per connection. Upon reading `PACKET_TYPE_KEYFRAME_REQ`, it invokes the registered callback, which signals `H264Encoder::ForceKeyframe()`. The encoder marks the next captured DXGI desktop texture as an IDR frame and transmits `PACKET_TYPE_KEYFRAME`.

---

## 6. Automatic Fallback to SS01 JPEG Streaming

### 6.1 Capability Probing
Prior to establishing connection, `DefaultConnectionService` queries `decoderBridge.probeCapabilities()`:
- Checks if a compatible H.264 decoder is available (`isSupported`).
- Checks maximum supported dimensions against target stream resolution.

### 6.2 Fallback Triggers & Isolation
- If `probe.isSupported == false`:
  - Automatically switches protocol to `StreamingProtocol.ss01Jpeg`.
  - Sets `fallbackReason = "Device lacks compatible H.264 decoder"`.
  - Routes frames to legacy JPEG memory buffer rendering.
- **Error Isolation:**
  - Network timeouts, TCP socket resets, or packet framing errors **never** trigger JPEG fallback. They are surfaced as connection transport errors to allow retry.

---

## 7. Windows Host Streaming Server (`TcpTransportServer`)

Implemented in [`native/windows_engine/src/transport/tcp_transport_server.cpp`](file:///d:/Second%20Display/native/windows_engine/src/transport/tcp_transport_server.cpp):
- Non-blocking Winsock2 TCP server supporting client connection tracking.
- Thread-safe `SendPacket()` protected by mutex.
- Continuous background client read loop that parses upstream packets (such as `PACKET_TYPE_KEYFRAME_REQ`) and notifies the streaming coordinator.
- Thread-safe shutdown and resource release.

Integrated into [`native/windows_engine/src/main.cpp`](file:///d:/Second%20Display/native/windows_engine/src/main.cpp) as a live server streaming captured desktop frames at target frame rates.

---

## 8. Verification & Test Results

### 8.1 Flutter Tests
Executed: `flutter test`
Result: **41 tests passed, 0 failed**.
Covered suites:
- `frame_accumulator_test.dart`: Complete packet reassembly, fragmented reads, oversized payload rejection, zero video length rejection, SS01/SS02 version detection, keyframe request generation.
- `connection_service_test.dart`: Codec config dispatch, keyframe dispatch, delta suppression before keyframe, upstream keyframe request rate-limiting, unsupported decoder fallback to SS01 JPEG, network failure isolation.
- `decoder_bridge_test.dart`: Bridge lifecycle, capability probing, format configuration, access unit feeding.
- `h264_surface_view_test.dart`: Platform view embedding, aspect ratio calculation, widget rendering.
- `connection_screen_test.dart` & `streaming_screen_test.dart`: Navigation, UI controls, metrics.

### 8.2 Flutter Static Analysis
Executed: `flutter analyze`
Result: **No issues found!** (0 errors, 0 warnings, 0 hints).

### 8.3 Native C++ Tests
Executed: `secondscreen_protocol_tests.exe`
Result: **11 tests passed, 0 failed**.
Covered test suites:
- `TestKeyframeRequestSerializationAndParsing`: Byte-accurate validation of `0x0006` packet structure.
- `TestTcpTransportServerLoopback`: Multithreaded TCP loopback socket verifying connection, transmission of keyframe request upstream, and callback dispatch.
- Protocol tests: Config packet parsing, Access Unit parsing, frame serialization.

### 8.4 Android Native Unit Tests
Executed: `./gradlew.bat :app:testDebugUnitTest`
Result: **10 tests passed, 0 failed** (`H264ConfigParserTest`).

### 8.5 Android APK Build
Executed: `flutter build apk --debug`
Result: **Built `app-debug.apk` successfully** (58.9 MB).

### 8.6 Physical Hardware Test Status
- **Unit and Loopback Tests:** 100% passed across all layers.
- **Physical Silicon Rendering:** **Unverified** in current test environment due to zero attached physical Android hardware devices (`adb devices` returned 0 devices). End-to-end physical screen presentation remains to be visually inspected upon connecting a physical device.

---

## 9. Deliverables Summary

| Component | File Path | Status |
|---|---|---|
| Dual Frame Accumulator | [`apps/android_app/lib/services/frame_accumulator.dart`](file:///d:/Second%20Display/apps/android_app/lib/services/frame_accumulator.dart) | Completed & Tested |
| Connection Service | [`apps/android_app/lib/services/connection_service.dart`](file:///d:/Second%20Display/apps/android_app/lib/services/connection_service.dart) | Completed & Tested |
| Connection Models | [`apps/android_app/lib/models/connection_state.dart`](file:///d:/Second%20Display/apps/android_app/lib/models/connection_state.dart) | Completed & Tested |
| Windows TCP Transport Server | [`native/windows_engine/src/transport/tcp_transport_server.cpp`](file:///d:/Second%20Display/native/windows_engine/src/transport/tcp_transport_server.cpp) | Completed & Tested |
| Windows Live Streaming Host | [`native/windows_engine/src/main.cpp`](file:///d:/Second%20Display/native/windows_engine/src/main.cpp) | Completed & Tested |
| Protocol & Transport Tests | [`native/windows_engine/tests/test_protocol.cpp`](file:///d:/Second%20Display/native/windows_engine/tests/test_protocol.cpp) | 11/11 Passed |
| Dart Unit Tests | `apps/android_app/test/frame_accumulator_test.dart` & `connection_service_test.dart` | 41/41 Passed |

