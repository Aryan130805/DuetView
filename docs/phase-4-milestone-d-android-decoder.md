# SecondScreen — Phase 4, Milestone D: Android MediaCodec Decoder & Native SurfaceView Rendering

## 1. Overview & Objective

Milestone D implements the native Android H.264 video decoder for **SecondScreen**, receiving `SS02` video packets and rendering decoded video frames directly to a hardware-accelerated `SurfaceView` embedded inside the existing Flutter client application.

This milestone establishes the decoding and rendering foundation. **Milestone E will integrate the live network receiver with the decoder.**

The primary components are:
- **Decoder Core:** [`H264Decoder.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/decoder/H264Decoder.kt)
- **Annex B Parameter Set Parser:** [`H264ConfigParser.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/decoder/H264ConfigParser.kt)
- **Platform View (SurfaceView):** [`VideoSurfacePlatformView.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/platform/VideoSurfacePlatformView.kt) / [`VideoSurfaceFactory.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/platform/VideoSurfaceFactory.kt)
- **Platform Channel Bridge:** [`SecondScreenNativeBridge.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/platform/SecondScreenNativeBridge.kt)
- **Dart Bridge Service:** [`decoder_bridge.dart`](file:///d:/Second%20Display/apps/android_app/lib/services/decoder_bridge.dart)
- **Flutter Widget:** [`h264_surface_view.dart`](file:///d:/Second%20Display/apps/android_app/lib/widgets/h264_surface_view.dart)
- **Automated Tests:**
  - Android Unit Tests: [`H264ConfigParserTest.kt`](file:///d:/Second%20Display/apps/android_app/android/app/src/test/kotlin/com/secondscreen/second_screen_android/decoder/H264ConfigParserTest.kt)
  - Flutter Tests: [`decoder_bridge_test.dart`](file:///d:/Second%20Display/apps/android_app/test/decoder_bridge_test.dart) & [`h264_surface_view_test.dart`](file:///d:/Second%20Display/apps/android_app/test/h264_surface_view_test.dart)

---

## 2. Architecture & Rendering Pipeline

```
+---------------------------------------------------------------------------------+
|                        SecondScreen Android Video Pipeline                      |
+---------------------------------------------------------------------------------+
|                                                                                 |
|   SS02 Network Stream (Milestone E integration point)                           |
|   [PACKET_TYPE_CONFIG 0x0001]     [PACKET_TYPE_KEYFRAME / DELTA 0x0002/0x0003]  |
|                 |                                       |                       |
|                 v                                       v                       |
|   +---------------------------+           +---------------------------+         |
|   |    H264ConfigParser       |           |     Access Unit Parser    |         |
|   |  - Scan start codes       |           |  - Verify AU framing      |         |
|   |  - Extract SPS (Type 7)   |           |  - IDR slice verification |         |
|   |  - Extract PPS (Type 8)   |           |  - Timestamp conversion   |         |
|   |  - Canonical 4-byte wrap  |           +-------------+-------------+         |
|   +-------------+-------------+                         |                       |
|                 | csd-0 (SPS) & csd-1 (PPS)             |                       |
|                 v                                       v                       |
|   +-------------------------------------------------------------------+         |
|   |                    Android MediaCodec (video/avc)                 |         |
|   |  - Hardware Decoder Discovery (prefer c2/OMX hardware codec)      |         |
|   |  - Low Latency Mode (KEY_LOW_LATENCY = 1, Realtime Priority = 0)  |         |
|   |  - Input Buffer Queue (queueInputBuffer with presentation time)   |         |
|   +---------------------------------+---------------------------------+         |
|                                     | Direct Buffer Rendering                   |
|                                     v (releaseOutputBuffer(..., render=true))   |
|   +-------------------------------------------------------------------+         |
|   |                     Native Android SurfaceView                    |         |
|   |  - Embedded via Flutter AndroidView Platform View                 |         |
|   |  - Direct GPU surface presentation without Dart buffer copying   |         |
|   |  - Aspect-ratio preservation, landscape/portrait auto-scaling    |         |
|   +-------------------------------------------------------------------+         |
|                                                                                 |
+---------------------------------------------------------------------------------+
```

---

## 3. Codec Discovery, Selection & Verification

Android devices present diverse codec implementations (Qualcomm Snapdragon `c2.qti.avc.decoder`, Samsung Exynos `c2.exynos.h264.decoder`, MediaTek `c2.mtk.avc.decoder`, or Google software fallbacks `c2.android.avc.decoder` / `OMX.google.h264.decoder`).

### 3.1 Selection Strategy
[`H264Decoder.probeCapabilities()`](file:///d:/Second%20Display/apps/android_app/android/app/src/main/kotlin/com/secondscreen/second_screen_android/decoder/H264Decoder.kt#L104-L156) inspects `MediaCodecList(REGULAR_CODECS)`:
1. Filters for decoder transforms (`!info.isEncoder`).
2. Matches MIME type `MediaFormat.MIMETYPE_VIDEO_AVC` (`video/avc`).
3. Classifies hardware acceleration:
   - On Android 10+ (API 29+): `info.isHardwareAccelerated` is checked directly.
   - On API < 29: Heuristic check ensures the codec name does not start with `OMX.google.` or `c2.android.`.
4. Prioritizes the first hardware-accelerated codec found, falling back to a software decoder if no hardware codec is present.
5. Queries `VideoCapabilities` for `supportedWidths` and `supportedHeights` upper bounds.

### 3.2 MediaFormat Configuration
The `MediaFormat` is configured with:
- MIME: `video/avc`
- Width & Height: Stream dimensions (e.g. `1280x720`, `1920x1080`)
- `csd-0`: `ByteBuffer` containing canonical 4-byte Annex B SPS (`00 00 00 01 [SPS]`)
- `csd-1`: `ByteBuffer` containing canonical 4-byte Annex B PPS (`00 00 00 01 [PPS]`)
- Low Latency: `KEY_LOW_LATENCY` set to `1` (API 30+)
- Priority: `KEY_PRIORITY` set to `0` (real-time streaming priority)

---

## 4. SS02 Codec Configuration Parsing

### 4.1 Specification Compliance
In accordance with `shared/protocol/README.md` and Milestone C:
- `PACKET_TYPE_CONFIG` (`0x0001`) contains SPS (NAL unit type 7) and PPS (NAL unit type 8).
- The Windows encoder emits both NAL units prefixed by Annex B start codes (`00 00 00 01` or `00 00 01`).
- `H264ConfigParser` scans the raw payload for 3-byte and 4-byte start codes without assuming fixed offsets.
- Extracted NAL units are normalized into canonical 4-byte Annex B buffers:
  - `csd-0`: `[00 00 00 01] [SPS byte sequence]`
  - `csd-1`: `[00 00 00 01] [PPS byte sequence]`
- Both SPS and PPS must be present; if either is missing or corrupted, the configuration is rejected safely with an error notification.

---

## 5. Safe Access Unit Processing & Presentation Timestamps

### 5.1 Submission Pipeline
Access units are submitted via `H264Decoder.feedAccessUnit()`:
1. **State Validation:** Checks decoder state. Discards frames if awaiting configuration (`WAITING_FOR_CONFIG`) or released.
2. **Keyframe Enforcement:**
   - If in `WAITING_FOR_KEYFRAME` state, non-IDR frames (P-slices) are discarded, and `onKeyframeRequested` is fired.
   - Once an IDR slice arrives (detected by `containsIdrSlice` or indicated by `isKeyframeHint`), the decoder transitions to `DECODING`.
3. **Buffer Acquisition:** Calls `MediaCodec.dequeueInputBuffer(timeoutUs = 10,000)`.
4. **Buffer Capacity Check:** Validates that the acquired buffer has sufficient capacity (`capacity() >= data.size`).
5. **Timestamp Transfer:** Passes `timestampUs` monotonically to `queueInputBuffer()`.
6. **Keyframe Flag:** Sets `BUFFER_FLAG_KEY_FRAME` when IDR slices are submitted.

---

## 6. Rendering Directly to SurfaceView

### 6.1 Flutter Platform View Integration
- Implemented via `PlatformView` / `PlatformViewFactory` registered in `MainActivity.kt`:
  - View Type: `com.secondscreen/video_surface`
  - Flutter Widget: [`H264SurfaceView`](file:///d:/Second%20Display/apps/android_app/lib/widgets/h264_surface_view.dart) wraps an `AndroidView`.
- The native `SurfaceHolder.Callback` receives:
  - `surfaceCreated`: Attaches the native `Surface` to `H264Decoder.setSurface(surface)`.
  - `surfaceChanged`: Updates the decoder with the modified surface.
  - `surfaceDestroyed`: Detaches the surface (`decoder.setSurface(null)`).
- When a valid surface is attached, the decoder output draining thread calls:
  ```kotlin
  codec.releaseOutputBuffer(outIndex, /* render = */ isSurfaceReady.get())
  ```
  This instructs the Android media subsystem to send decoded video buffers directly to the underlying Surface compositor without copying frames into Flutter Dart memory.

---

## 7. Threading & Lifecycle Architecture

### 7.1 Separation of Threads
- **Flutter UI Thread / Main Thread:** Only handles lightweight MethodChannel invocations and state callbacks dispatched via `Handler(Looper.getMainLooper())`.
- **Decoder Drain Thread:** Dedicated background daemon thread (`SecondScreen-DecoderDrain`) continuously drains output buffers using `dequeueOutputBuffer(10ms)` and updates metrics.
- **Surface Lifecycle:** Thread-safe transitions via `AtomicReference<State>` and synchronized lifecycle operations (`initialize`, `setSurface`, `flush`, `reset`, `release`).
- **Shutdown & Interruption:** `stopDrainLoop()` signals the worker thread via `AtomicBoolean`, interrupts blocking waits, and joins before codec release.

---

## 8. Platform Channel API

Channel: `com.secondscreen/decoder_control`

| Method / Call | Direction | Arguments | Return Value / Behavior |
|---|---|---|---|
| `getDecoderCapabilities` | Dart → Android | None | `{codecName, isHardwareAccelerated, maxWidth, maxHeight, isSupported}` |
| `initializeDecoder` | Dart → Android | `{"width": int, "height": int}` | `bool` |
| `feedConfigPacket` | Dart → Android | `{"data": Uint8List}` | `bool` |
| `feedAccessUnit` | Dart → Android | `{"data": Uint8List, "timestampUs": int, "isKeyframe": bool}` | `bool` |
| `resetDecoder` | Dart → Android | None | `bool` |
| `getMetrics` | Dart → Android | None | Real-time performance & error metrics map |
| `runSmokeTest` | Dart → Android | `{"width": int, "height": int, "frames": int}` | Smoke test execution results map |
| `onStateChanged` | Android → Dart | `{"state": String}` | Decoder state notification |
| `onKeyframeRequested`| Android → Dart | None | Signals client to request an IDR keyframe from host |
| `onDecoderError` | Android → Dart | `{"error": String, "isFatal": bool}` | Propagates decoder diagnostics |

---

## 9. Verification & Automated Test Results

### 9.1 Test Execution Summary

1. **Flutter Analysis:**
   ```powershell
   flutter analyze
   ```
   **Result:** `No issues found! (ran in 4.1s)`

2. **Flutter Test Suite:**
   ```powershell
   flutter test
   ```
   **Result:** `30 tests passed!` (including `decoder_bridge_test.dart`, `h264_surface_view_test.dart`, `frame_accumulator_test.dart`, `connection_service_test.dart`, `widget_test.dart`)

3. **Android Gradle Unit Tests:**
   ```powershell
   cd apps/android_app/android
   ./gradlew.bat :app:testDebugUnitTest
   ```
   **Result:** `BUILD SUCCESSFUL (10 tests passed, 0 failures, 0 skipped)`

4. **Android Debug APK Build:**
   ```powershell
   cd apps/android_app
   flutter build apk --debug
   ```
   **Result:** `√ Built build\app\outputs\flutter-apk\app-debug.apk`

---

## 10. Smoke Test & Hardware Decoding Status

### 10.1 Standalone Smoke Test
The native bridge contains an integrated standalone smoke test (`runSmokeTest`):
1. Initializes the decoder at configured dimensions (e.g. 1280x720).
2. Feeds a canonical SPS/PPS Annex B configuration payload.
3. Submits an IDR keyframe access unit.
4. Submits successive P-slice delta access units at ~33.3ms intervals.
5. Monitors output frame draining, timestamp pacing, and error counts.

### 10.2 Technical Honesty & Verification State
- **Android Device Attached:** None (`adb devices` reported 0 devices/emulators attached during this test session).
- **Unit and Mock Verification:** **PASSED**.
- **Real Hardware MediaCodec Decoding on Physical Silicon:** **UNVERIFIED on physical hardware** during this execution due to absence of an attached Android device/emulator. Real hardware rendering will be exercised once an Android runtime target is connected.

---

## 11. Milestone E: Live Network Integration Steps

Milestone E will connect the `SS02` TCP receiver to this decoder:
1. Parse the 24-byte `SS02` packet header in the network accumulator.
2. Route `PACKET_TYPE_CONFIG` (`0x0001`) packets to `feedConfigPacket()`.
3. Route `PACKET_TYPE_KEYFRAME` (`0x0002`) and `PACKET_TYPE_DELTA` (`0x0003`) packets to `feedAccessUnit()`.
4. Connect `onKeyframeRequested` events from `SecondScreenDecoderBridge` to send `PACKET_TYPE_KEYFRAME_REQ` (`0x0006`) packets to the Windows streaming host.
5. Provide automatic fallback to Phase 3 JPEG streaming (`SS01`) if the Android device reports incompatible or unsupported H.264 decoders.

