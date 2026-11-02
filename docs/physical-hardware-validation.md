# SecondScreen — Physical Hardware Validation Guide & Status Report

## 1. Purpose & Implementation Audit

This document defines the physical hardware test setup, verification procedures, and performance profiling methodology for **SecondScreen**, an open-source Windows-to-Android secondary display streaming solution.

### 1.1 Implementation Status of Architectural Components

Before executing physical hardware validation, the software codebase was audited to distinguish implemented and unit-tested components from physical runtime behavior:

| Architectural Component | Implementation Location | Verified Status |
|---|---|---|
| **Windows Desktop Capture** | `native/windows_engine/src/capture/dxgi_capturer.cpp` | **Implemented & Integration-test-verified** via DXGI Desktop Duplication API (`IDXGIOutputDuplication`). |
| **Windows H.264 Video Encoder** | `native/windows_engine/src/encoder/mf_h264_encoder.cpp` | **Implemented & Integration-test-verified** via Media Foundation MFT (`CLSID_MSH264Enc`) with dynamic bitrate control via `ICodecAPI`. |
| **SS02 Binary TCP Transport** | `native/windows_engine/src/transport/tcp_transport_server.cpp` | **Implemented & Integration-test-verified** with non-blocking multi-threaded Winsock2 server and 24-byte big-endian framing. |
| **Android Ingestion & Framing** | `apps/android_app/lib/services/frame_accumulator.dart` | **Implemented & Unit-test-verified** with two-phase state machine handling arbitrary TCP chunk splits and packet concatenation. |
| **Android MediaCodec Decoder** | `apps/android_app/android/app/src/main/kotlin/.../H264Decoder.kt` | **Implemented & Unit-test-verified** in low-latency mode (`KEY_LOW_LATENCY = 1`) extracting Annex B SPS/PPS into `csd-0`/`csd-1`. |
| **Android SurfaceView Platform View** | `apps/android_app/android/app/src/main/kotlin/.../VideoSurfacePlatformView.kt` | **Implemented & Build-verified** using Flutter `AndroidView` for zero-copy GPU presentation. |
| **Upstream Keyframe Requests** | `apps/android_app/lib/services/connection_service.dart` | **Implemented & Integration-test-verified** using `PACKET_TYPE_KEYFRAME_REQ` (`0x0006`) with 200 ms rate-limiting. |
| **Upstream Telemetry Feedback** | `apps/android_app/lib/services/connection_service.dart` | **Implemented & Integration-test-verified** using `PACKET_TYPE_RECEIVER_REPORT` (`0x0008`) sending 20-byte reports at 1 Hz. |
| **AIMD Adaptive Rate Controller** | `native/windows_engine/src/adaptive/adaptive_controller.cpp` | **Implemented & Integration-test-verified** with multiplicative bitrate decrease (-25%) and adaptive target FPS step-down (60 &rarr; 15). |
| **Phase 3 SS01 JPEG Fallback** | `apps/android_app/lib/services/connection_service.dart` | **Implemented & Unit-test-verified** triggering when H.264 decoder is unsupported (`!isSupported`). |
| **Physical Display Rendering** | Physical Android silicon and display panel | **NOT RUN — HARDWARE UNAVAILABLE** (0 physical devices connected to test host). |

---

## 2. Environment Prerequisites

To conduct physical device validation, the environment must satisfy the following prerequisites:

### 2.1 Mandatory Prerequisites
1. **Windows Host Machine:**
   - Windows 10 (Version 1809+) or Windows 11 64-bit (Tested on Windows 11 Build `10.0.26200.9711`).
   - Direct3D 11 runtime support with DXGI 1.2 Desktop Duplication capability.
   - Microsoft In-box H.264 Encoder MFT or hardware MFT (Intel Quick Sync Video, NVIDIA NVENC, AMD AMF).
   - Local toolchain for building the engine: CMake 3.20+, Ninja, and MinGW-w64 (or MSVC).
2. **Android Development Tools:**
   - Flutter SDK 3.x (Tested with Dart 3.x).
   - Android SDK Platform-Tools containing `adb.exe` (v34+).
   - Java Development Kit (JDK 17).
3. **Physical Target Android Hardware:**
   - An Android phone or tablet running **Android 10+ (API level 29+)**.
   - USB cable for ADB installation, diagnostic logcat streaming, and baseline metrics collection.
   - **Developer Options** and **USB Debugging** enabled in device settings.
4. **Local Wi-Fi Network:**
   - Both the Windows PC and the Android device must be connected to the **same Wi-Fi subnet** (802.11ac / 802.11ax 5 GHz Wi-Fi strongly recommended).
   - Network isolation (AP Isolation / Client Isolation) must be disabled on the Wi-Fi router so devices can communicate directly peer-to-peer.
5. **Windows Firewall Rule:**
   - Narrowly scoped inbound TCP rule allowing port `52025` on the Private network profile:
     ```powershell
     netsh advfirewall firewall add rule name="SecondScreen Stream Port 52025" dir=in action=allow protocol=TCP localport=52025 profile=private
     ```
   - *Security Requirement:* **Never disable Windows Firewall globally** as a workaround.

### 2.2 Optional Profiling Tools
- **High-Speed Camera:** Capable of capturing **240 FPS** (or higher) with manual exposure control for optical glass-to-glass latency recording.
- **Network Impairment Tool:** A traffic shaper (such as *Clumsy* on Windows or *tc/netem* on a router) to inject controlled latency (e.g. +50 ms) and packet drop (e.g. 5–25%) for congestion profiling.
- **Android Diagnostic Utilities:** `adb shell dumpsys gfxinfo` and `adb shell top` for GPU rendering metrics and CPU utilization.

---

## 3. Physical Device Connection & Step-by-Step Execution

Follow these step-by-step instructions to establish and verify live streaming:

```
+-------------------------------------------------------------------------------+
|                       Physical Test Deployment Topology                       |
+-------------------------------------------------------------------------------+
|                                                                               |
|  [Windows PC] 192.168.1.105:52025             [Android Phone / Tablet]        |
|  +------------------------------+             +-----------------------------+ |
|  |  secondscreen_engine.exe     |             |  SecondScreen Client App    | |
|  |  - DXGI Desktop Capture      |             |  - SS02 TCP Ingestion       | |
|  |  - Media Foundation H.264    |             |  - MediaCodec Decoder       | |
|  +--------------+---------------+             +--------------+--------------+ |
|                 |                                            |                |
|                 | ~~~~~~~~ Local Wi-Fi (Port 52025) ~~~~~~~~ |                |
|                 +--------------------------------------------+                |
|                 | (Live H.264 Video Stream + 1 Hz Telemetry)                  |
|                 |                                                             |
|                 | ======== USB Cable (Debugging Only) ====== |                |
|                 +--------------------------------------------+                |
|                   (ADB Install, Real-Time Logcat & Metrics)                   |
|                                                                               |
+-------------------------------------------------------------------------------+
```

### Step 1: Connect Hardware & Verify ADB Detection
1. Connect the Android device to the PC using a USB cable.
2. Verify detection:
   ```powershell
   adb devices -l
   ```
   *Expected Output:*
   ```text
   List of devices attached
   RFCW10ABCDE   device product:gts8wifi model:SM_X700 device:gts8wifi
   ```
   *(If prompted on the Android screen, check "Always allow from this computer" and tap OK).*

### Step 2: Build & Install Android Application
In the project workspace, build the debug APK and install it to the connected device:
```powershell
cd apps/android_app
flutter build apk --debug
adb install -r build/app/outputs/flutter-apk/app-debug.apk
```

### Step 3: Launch Android Logcat Diagnostic Stream
In a separate terminal window, monitor live decoding and networking logs:
```powershell
adb logcat -s SecondScreenH264Decoder:V flutter:V
```

### Step 4: Identify Windows PC LAN IPv4 Address
In PowerShell on the Windows PC:
```powershell
Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias "Wi-Fi*" | Select-Object IPAddress
```
*Note the local IP (e.g. `192.168.1.105`).*

### Step 5: Start Windows Streaming Engine
From the repository root:
```powershell
.\native\windows_engine\build\secondscreen_engine.exe --port 52025 --fps 30 --bitrate 4000
```
*Expected Terminal Output:*
```text
========================================================
  SecondScreen Native Windows Engine (Phase 5 Adaptive) 
========================================================
Listening on TCP port: 52025
Target FPS: 30 | Bitrate: 4000 Kbps
Adaptive Streaming: ENABLED
[INFO] Source Display Resolution: 1920x1080
[INFO] Active Encoder: Microsoft H264 Video Encoder MFT (Hardware: NO)
[INFO] Server running. Press Ctrl+C to stop.
```

### Step 6: Connect from Android Client
1. Launch the **SecondScreen** app on the Android device.
2. Tap the **Connect** tab.
3. Enter the PC's IPv4 address (`192.168.1.105`) and port `52025`.
4. Tap **Connect**.
5. Observe the connection state progression: `Connecting` &rarr; `Connected` &rarr; `Streaming`.
6. Confirm that the Windows desktop appears smoothly on the Android `SurfaceView`.

### Step 7: Verify Interruption & Recovery
1. Toggle Wi-Fi off and back on via the Android Quick Settings panel.
2. Verify that the Android app detects socket closure, displays a retry button or reconnects automatically, and that the Windows server cleanly returns to the listening state without crashing.

---

### 3.1 Comprehensive Troubleshooting Guide

| Issue Encountered | Probable Root Cause | Resolution Procedure |
|---|---|---|
| **ADB reports `unauthorized` or `no devices`** | USB debugging not enabled or RSA key not accepted. | Disconnect USB cable, toggle USB Debugging off/on in Developer Options, reconnect cable, and tap **Allow** on the Android prompt. |
| **Connection times out after 4 seconds** | Incorrect IP address or Windows Firewall blocking TCP port. | Re-verify `ipconfig` IPv4 address. Ensure the Windows network profile is set to **Private** (not Public) and verify the firewall rule via `netsh advfirewall firewall show rule name="SecondScreen Stream Port 52025"`. |
| **App connects, but screen stays black** | Missing SPS/PPS codec configuration or keyframe drop. | Inspect `adb logcat`. If the log reports `Awaiting keyframe`, verify that `PACKET_TYPE_KEYFRAME_REQ` was transmitted. The host capture engine will force an immediate IDR keyframe. |
| **Stream is distorted, pixelated, or exhibits macroblocks** | Wi-Fi packet drop causing missing reference slices in P-frames. | Trigger a keyframe recovery. The Phase 5 adaptive controller will detect frame backlog and throttle the bitrate down automatically. |
| **Colors appear crushed or washed out** | RGB to NV12 color range mismatch. | The encoder converts BGRA to NV12 using standard BT.601 limited/full range. Verify that the native `MediaCodec` surface uses standard hardware colorimetry. |
| **App reports "H.264 decoder not supported"** | Device hardware lacks `video/avc` support. | The app automatically engages the **Phase 3 SS01 JPEG fallback** to preserve streaming functionality. |
| **High latency or growing lag over time** | Wi-Fi queue bufferbloat over TCP. | Enable adaptive bitrate (`--adaptive` flag on host, enabled by default). Ensure device is on 5 GHz Wi-Fi rather than 2.4 GHz. |
| **Windows engine fails with `DXGI_ERROR_ACCESS_LOST`** | Screen resolution change, monitor sleep, or UAC desktop switch. | `DxgiCapturer` automatically detects access loss, releases duplicated interfaces, re-acquires the desktop duplication token, and forces an IDR keyframe to re-sync the decoder. |

---

## 4. Optical Glass-to-Glass Latency Measurement Methodology

Direct subtraction of timestamps across Windows and Android devices is **invalid** because independent operating systems have different monotonic clock epochs and Wi-Fi NTP synchronization jitter (5–50 ms) exceeds the target streaming latency.

To quantify glass-to-glass display latency accurately, the optical high-speed video camera method is used:

```
+-------------------------------------------------------------------------------+
|                       Optical Latency Test Configuration                      |
+-------------------------------------------------------------------------------+
|                                                                               |
|       +------------------------+             +------------------------+       |
|       |   Windows PC Display   |             |     Android Screen     |       |
|       |  [ Millisecond Timer ] |             | [ Streamed Video Feed] |       |
|       |     00:12.345          |             |       00:12.310        |       |
|       +-----------+------------+             +-----------+------------+       |
|                   \                                     /                     |
|                    \                                   /                      |
|                     v                                 v                       |
|                   +-------------------------------------+                     |
|                   |       High-Speed Video Camera       |                     |
|                   |  (240 FPS, 4.167 ms frame exposure) |                     |
|                   +-------------------------------------+                     |
|                                                                               |
+-------------------------------------------------------------------------------+
```

### 4.1 Optical Test Procedure
1. **Render Test Pattern:** Launch a full-screen, high-contrast digital millisecond rolling counter (`MM:SS.mmm`) on the Windows primary monitor updating on every display VSYNC.
2. **Camera Placement:** Place the Android tablet directly adjacent to the PC monitor. Frame both displays within the camera lens so that both digital timers are legible in a single recording frame.
3. **High-Speed Recording:** Record a 60-second video clip at **240 frames per second** (or higher).
   - Frame interval: $\tau = \frac{1000}{240} \approx 4.167\text{ ms}$.
   - Temporal resolution limit: $\pm 4.167\text{ ms}$.
4. **Transition Extraction:** Step through the recorded footage frame-by-frame:
   - Identify the camera frame where the PC timer advances to time $T_1$.
   - Scan forward to locate the exact camera frame where the Android screen first reflects that same transition value $T_1$.
5. **Calculation:**
   $$\text{Observed Glass-to-Glass Latency} = T_{\text{Camera\_Android\_Visible}} - T_{\text{Camera\_PC\_Visible}}$$
6. **Statistical Characterization:** Extract at least **100 independent transitions** across the 60-second clip and compute:
   - Median Latency ($P_{50}$)
   - 95th Percentile Latency ($P_{95}$)
   - Minimum and Maximum Observed Latency
   - Standard Deviation ($\sigma$)

*Note:* Optical glass-to-glass latency encompasses all stages: DXGI capture + NV12 conversion + H.264 encode + TCP send queue + Wi-Fi transmission + Android socket read + frame accumulator + MediaCodec decode + Android display scanout.

---

## 5. Performance Metrics & Results Template

### 5.1 Pipeline Telemetry Metrics

| Stage | Metric Name | Measurement Tool / API |
|---|---|---|
| **Capture** | Acquisition Duration ($t_{\text{acq}}$) | Windows `MetricsTracker::RecordFrameAcquisitionDuration()` |
| **Encoder** | Encode Duration ($t_{\text{enc}}$) | Windows `MetricsTracker::RecordFrameEncoded()` |
| **Transport** | Send Delay ($t_{\text{send}}$) | Windows `TcpTransportServer::SendPacket()` duration |
| **Network** | Throughput & Bitrate | Windows `MetricsTracker` / Android `ConnectionStateModel` |
| **Client Ingestion**| Framing & Assembly Delay | Dart `FrameAccumulator` timestamp delta |
| **Decoder** | Decode Processing Time | Android `H264Decoder` `queueInputBuffer` to `dequeueOutputBuffer` |
| **Display** | Rendered FPS & Drops | Android `ConnectionStateModel.receivedFps` / dropped counter |
| **System** | CPU / GPU / RAM Usage | Windows Task Manager / Android `adb shell top` |

### 5.2 Physical Validation Results Table

> [!IMPORTANT]
> The table below records physical hardware test results. Because a physical Android device was unavailable during this verification session, physical tests are explicitly marked as pending.

| Test Scenario | Resolution | Target FPS | Measured FPS | Median Latency ($P_{50}$) | 95th % Latency ($P_{95}$) | Dropped Frames | Status / Notes |
|---|---|---:|---:|---:|---:|---:|---|
| **Stable Wi-Fi (5 GHz)** | 1920x1080 | 30 | — | — | — | — | **Pending physical test** (Host loopback baseline: 10.8 ms host latency) |
| **Extended Duration (15 min)** | 1920x1080 | 30 | — | — | — | — | **Pending physical test** (Evaluate thermal throttling & memory leak) |
| **Controlled Congestion (+50ms, 10% drop)** | 1920x1080 | 30 | — | — | — | — | **Pending physical test** (Evaluate AIMD bitrate step-down to 2,250 Kbps) |
| **Temporary Interruption (3s drop)** | 1920x1080 | 30 | — | — | — | — | **Pending physical test** (Evaluate socket recovery and IDR keyframe resync) |
| **SS01 JPEG Fallback** | 1280x720 | 30 | — | — | — | — | **Pending physical test** (Verified in unit test mock) |

---

## 6. Controlled Test Scenarios

When a physical Android device is connected, execute the following test scenarios:

### Scenario 1: Baseline Stable Wi-Fi (15-Minute Run)
- **Goal:** Verify baseline stability, average frame rate, and memory consumption.
- **Procedure:** Connect over 5 GHz Wi-Fi at 1080p @ 30 FPS. Stream continuous desktop video for 15 minutes. Record CPU, memory, and rendered FPS every 60 seconds.

### Scenario 2: Network Latency & Bandwidth Impairment
- **Goal:** Verify Phase 5 AIMD rate controller response.
- **Procedure:** Introduce 50 ms latency and 10% packet drop using a network shaping tool.
- **Expected Behavior:** Host logs `[ADAPTIVE] Bitrate decreased` (4,000 &rarr; 3,000 &rarr; 2,250 Kbps). Visual stream drops bitrate smoothly without stalling the video decoder.

### Scenario 3: Physical Disconnect & Reconnect Recovery
- **Goal:** Verify socket lifecycle robustness.
- **Procedure:** Disconnect Wi-Fi on Android during active streaming; wait 5 seconds; re-enable Wi-Fi and reconnect.
- **Expected Behavior:** Android resets decoder state; host handles client disconnect; re-connection sends fresh `PACKET_TYPE_CONFIG` and forces an initial IDR keyframe.

### Scenario 4: Hardware Fallback (SS01 JPEG)
- **Goal:** Verify that devices lacking H.264 support automatically stream JPEG frames.
- **Procedure:** Force `probeCapabilities()` to report `isSupported = false` or test on an unsupported device.
- **Expected Behavior:** Connection state transitions to `StreamingProtocol.ss01Jpeg`, rendering JPEG frames via `latestFrame` ValueNotifier.

---

## 7. Hardware Verification Status

The current status of all SecondScreen components is recorded below:

| Verification Category | Actual Result / Status | Details |
|---|---|---|
| **Software Build (Windows)** | **PASS** | Built `secondscreen_engine.exe` and test executables via CMake/Ninja. |
| **Software Build (Android)** | **PASS** | Built `app-debug.apk` (58.9 MB) via Flutter SDK. |
| **Automated C++ Tests** | **PASS** | 14 of 14 protocol, transport, and adaptive controller tests passed. |
| **Automated Dart Tests** | **PASS** | 43 of 43 unit, widget, and platform channel tests passed. |
| **Automated Android Native Tests** | **PASS** | 10 of 10 `H264ConfigParserTest` unit tests passed. |
| **Physical Android Connection** | **NOT RUN — HARDWARE UNAVAILABLE** | 0 physical devices detected via `adb devices -l`. |
| **Live H.264 Physical Streaming** | **NOT RUN — HARDWARE UNAVAILABLE** | Requires physical device display verification. |
| **Optical Latency Measurement** | **NOT RUN — HARDWARE UNAVAILABLE** | Requires physical camera recording of real display. |
| **Physical Congestion Testing** | **NOT RUN — HARDWARE UNAVAILABLE** | Requires physical Wi-Fi connection. |
| **Physical Recovery Testing** | **NOT RUN — HARDWARE UNAVAILABLE** | Requires physical Wi-Fi connection. |
