# DuetView — Comprehensive Stability, Integration, and Architecture Report

**Project**: DuetView (Windows-to-Android Secondary Display System)  
**Developer**: Aryan Verma (Role: Developer, DuetView)  
**Date**: November 2, 2026  
**Document Version**: 1.0.0 (Release Audit & Final Report)  

---

## Executive Summary

DuetView is a high-performance, low-latency, peer-to-peer secondary display system enabling an Android device to serve as an extended or mirrored monitor for a Windows 10/11 host.

This engineering audit investigated and resolved core reliability issues across:
1. **Host-Side Crash on Other PCs**: Missing MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`).
2. **Network Pairing Failures**: 7/10 failure rate during QR pairing caused by TCP RST resets and virtual adapter IP prioritization.
3. **Capture Stalling When Mouse is Stationary**: Direct3D/DXGI duplication pipeline idling when no dirty desktop rects were produced, preventing H.264 SPS/PPS generation and keeping Android MediaCodec stuck in an uninitialized state.
4. **Touch & Mouse Input Drag False-Positives**: Immediate `ButtonDown` on pointer contact turning quick taps into accidental drags, and lack of soft-keyboard backspace support.
5. **Frame-Rate Capabilities**: Expanding negotiable refresh rates to support modern 90 Hz and 120 Hz Android displays.
6. **Virtual Display Driver (IddCx)**: Auditing driver installation flow, signing status, and integration with `SetDisplayConfig(SDC_TOPOLOGY_EXTEND)` to provide true extended desktop displays.

---

## 1. Root Cause Analysis & Resolutions

### 1.1 Host-Side Crash on Remote PCs (Missing MinGW DLLs)
- **Root Cause**: The C++ engine binaries were linked dynamically with GCC/MinGW-w64 runtime libraries. On development workstations where MinGW was added to the system `PATH`, executables launched successfully. On standard consumer Windows PCs without MinGW installed, Windows failed immediately with error dialogs: `The code execution cannot proceed because libgcc_s_seh-1.dll was not found`.
- **Resolution**:
  - Configured GCC linker flags `-static -static-libgcc -static-libstdc++` in CMake.
  - Verified with `objdump -p` that `SecondScreenApp.exe`, `DuetView-PC.exe`, and `secondscreen_engine.exe` have zero MinGW DLL dependencies, linking exclusively to standard Windows system DLLs (`KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `SETUPAPI`, `SHELL32`, `WS2_32`).
  - As an extra precaution, bundled runtime DLLs in the redistributable zip package `dist/DuetView-PC-0.4.0-win64.zip`.

### 1.2 QR Code Pairing & Connection Reliability
- **Root Cause**:
  1. **TCP RST on Close**: The host accepted the Android pairing connection, sent the `SS02` pairing response JSON, and immediately invoked `closesocket(clientSock)`. On Windows Sockets, closing a connection before the client has finished reading all data or without a graceful `shutdown(SD_SEND)` causes the TCP stack to issue a `RST` (Reset) packet, aborting the connection on Android.
  2. **Adapter Priority (WSL/Hyper-V/VirtualBox)**: `GetLocalIPv4Endpoints()` enumerated network adapters without prioritizing physical Wi-Fi or Ethernet. Virtual adapters (e.g., WSL2 `172.x.x.x` or Hyper-V `192.168.128.x`) were frequently placed first in the QR code payload, making the Android client attempt connection to unreachable internal subnets.
- **Resolution**:
  - Implemented adapter sorting: physical Wi-Fi adapters (`IF_TYPE_IEEE80211`) and physical Ethernet (`IF_TYPE_ETHERNET_CSMACD`) are evaluated first. Link-local APIPA addresses (`169.254.*`) and virtual interfaces are deprioritized.
  - Implemented graceful socket shutdown: `shutdown(clientSock, SD_SEND)` followed by draining client acknowledgment before `closesocket()`.
  - Added connection backoff and timeout handling on Android client (`pc_health_service.dart`, `qr_pairing_service.dart`).

### 1.3 Desktop Capture Stalling (Stationary Mouse Issue)
- **Root Cause**: `DxgiCapturer::AcquireFrame` polled the DXGI Output Duplication interface. When the desktop content was static (e.g., mouse cursor not moving, video not playing), DXGI returned `DXGI_ERROR_WAIT_TIMEOUT`, or returned frames where `desktopResource == nullptr` (`AccumulatedFrames == 0`). In those cases, the capturer skipped encoding. Because no frames entered `MfH264Encoder`, SPS and PPS headers were never generated for new connections, and Android MediaCodec remained stuck in `WAITING_FOR_CONFIG` / `UNINITIALIZED`.
- **Resolution**:
  - Retained the latest captured Direct3D 11 texture (`retainedTexture`) in `DxgiCapturer`.
  - Pre-seeded `retainedTexture` on capturer initialization.
  - When DXGI reports timeout or mouse-only update, the capturer emits the retained texture as a keepalive refresh frame if 500 ms have elapsed or if an initial keyframe was requested.
  - Result: Android receives clean H.264 IDR frames with SPS/PPS immediately upon connecting, even when the PC desktop is completely still.

### 1.4 High Refresh Rate Support (90 FPS & 120 FPS)
- **Implementation**:
  - Updated `AdaptiveController::supportedFpsLevels` in C++ to `{120U, 90U, 60U, 45U, 30U, 20U, 15U}`.
  - Added 90 FPS and 120 FPS options to Windows UI combobox `g_hComboFps` in `main_window.cpp`.
  - Added `fps90(90, '90 FPS (Smooth 90Hz)')` and `fps120(120, '120 FPS (Ultra 120Hz)')` to `StreamFrameRate` enum in Flutter (`stream_settings.dart`).

### 1.5 Touch & Mouse Input Optimization
- **Root Cause**:
  1. `TouchInputOverlay` immediately dispatched `buttonDown(left)` on pointer down. Hand micro-tremors (>1px) converted taps into unintended mouse dragging on Windows.
  2. Software keyboards (IME) on Android do not emit `onChanged` events when Backspace is tapped on an empty text field.
- **Resolution**:
  - **Touch Slop Disambiguation**: Implemented an 8-pixel movement slop threshold. Pointer down records coordinates and starts a 500ms long-press timer (for right-click). If the finger moves less than 8px and lifts, a clean click sequence (`buttonDown` followed by `buttonUp`) is dispatched.
  - **Double-Tap Detection**: Consecutive taps within 320ms and 25px dispatch a Win32 double-click event.
  - **Dedicated Trackpad Mode**: Added relative mouse glide, on-screen pointer cursor, bottom control bar with dedicated [Left Click] and [Right Click] buttons, scroll support, and instant mode switching.
  - **Keyboard Backspace & Shortcuts**: Integrated a zero-width space sentinel (`\u200B`) and `Focus(onKeyEvent)` listener, enabling reliable soft-keyboard and hardware backspace deletion. Added dedicated one-tap shortcut buttons for `Ctrl+C`, `Ctrl+V`, `Ctrl+A`, `Ctrl+Z`, `Alt+Tab`, `Win+D`, and `Win+P`.

### 1.6 Virtual Display Driver & Windows Display Extension
- **IddCx Driver Audit**:
  - DuetView includes an Indirect Display Driver (`SecondScreenIdd.dll` and `SecondScreenIdd.inf`) using Microsoft's IddCx 1.4+ architecture.
  - In Windows, third-party kernel and indirect display drivers require WHQL or test-signing when Secure Boot is enabled. DuetView includes built-in detection (`DriverManager`) that safely evaluates driver installation state without bypassing Windows security policies.
  - Integrated programmatic display extension via `SetDisplayConfig(0, NULL, 0, NULL, SDC_TOPOLOGY_EXTEND | SDC_APPLY)` in the PC interface and shortcuts, immediately putting Windows into extended display mode.

---

## 2. Verification and Test Results

### 2.1 Native C++ Unit and Integration Tests
```
Test project D:/Second Display/native/windows_engine/build
    Start 1: ProtocolTests
1/4 Test #1: ProtocolTests ....................   Passed    1.07 sec
    Start 2: CaptureTests
2/4 Test #2: CaptureTests .....................   Passed    0.53 sec
    Start 3: EncoderTests
3/4 Test #3: EncoderTests .....................   Passed    1.37 sec
    Start 4: DriverTests
4/4 Test #4: DriverTests ......................   Passed   31.18 sec

100% tests passed out of 4 (Total time: 34.31 sec)
```

### 2.2 Flutter Unit and Widget Tests
Tests verify:
- SS02 packet serialization (40-byte pointer, 40-byte scroll, 36-byte keyboard packets).
- Normalized coordinate mapping accounting for aspect ratio and letterboxing.
- Touch slop discrimination preventing false drag operations.
- Keyboard chord generation (`Ctrl+C`, `Ctrl+V`, `Ctrl+A`, `Ctrl+Z`, `Alt+Tab`, `Win+D`, `Win+P`).
- QR code payload parsing, token validation, and IP selection.

---

## 3. Deliverables & Release Packages

The build artifacts are located in `dist/`:
- `dist/DuetView-PC-0.4.0-win64.zip`: Standalone redistributable package for 64-bit Windows PCs (contains `DuetView-PC.exe`, `SecondScreenApp.exe`, `secondscreen_engine.exe`, MinGW runtimes, and `driver/` package).
- `dist/DuetView-PC.exe` / `dist/SecondScreenApp.exe`: Windows host manager GUI.
- `dist/secondscreen_engine.exe`: Background capture and encoding streaming engine.
- `dist/DuetView-Android.apk`: Android client application with 90/120 FPS, trackpad mode, and developer details.
- `dist/driver/`: Indirect Display Driver package with `devcon.exe` and installation batch scripts.

