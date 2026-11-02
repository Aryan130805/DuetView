# Phase 4, Milestone B: DXGI Desktop Duplication Capture Engine

## 1. Overview

Milestone B implements a native, hardware-accelerated Windows desktop capture engine using **Direct3D 11** and the **DXGI Desktop Duplication API** (`IDXGIOutputDuplication`).

This component replaces the experimental GDI / `mss` screen capture pipeline from Phase 3 with a high-performance GPU-resident pipeline:
- **Zero CPU copies**: Acquired desktop textures are retained in Direct3D 11 VRAM via `CopyResource`.
- **Immediate frame release**: The DXGI frame lock is released immediately after the GPU copy, ensuring the Desktop Duplication pipeline is never blocked by downstream processing.
- **Ultra-low acquisition latency**: Measured average frame acquisition time is **~0.11 ms** on Intel HD Graphics 520 hardware.
- **Session recovery**: Recovers automatically from `DXGI_ERROR_ACCESS_LOST` (display resolution changes, desktop switches) and device resets.

---

## 2. Architecture & GPU Texture Lifetime

```
+--------------------------------------------------------------------------------+
|                         WINDOWS DIRECT3D 11 / DXGI                             |
|                                                                                |
|   +-----------------------+        +---------------------------------------+   |
|   | IDXGIOutput1          | -----> | IDXGIOutputDuplication                |   |
|   | (Attached Display)    |        | (AcquireNextFrame, timeoutMs)         |   |
|   +-----------------------+        +-------------------+-------------------+   |
|                                                        |                       |
|                                                        v (Acquired Texture)    |
|   +-----------------------+        +-------------------+-------------------+   |
|   | ID3D11DeviceContext   | -----> | CopyResource (GPU-to-GPU, ~0.1 ms)    |   |
|   +-----------------------+        +-------------------+-------------------+   |
|                                                        |                       |
|                                                        v                       |
|                                    +-------------------+-------------------+   |
|                                    | ReleaseFrame() (DXGI unlocked)        |   |
|                                    +-------------------+-------------------+   |
|                                                        |                       |
|                                                        v (Retained Texture)    |
|   +----------------------------------------------------+-------------------+   |
|   | CapturedFrame (VRAM ID3D11Texture2D*, format 87, 100% GPU-resident)    |   |
|   +----------------------------------------------------+-------------------+   |
|                                                        |                       |
|                                                        v                       |
|                                      (To Milestone C: H.264 MFT Encoder)       |
+--------------------------------------------------------------------------------+
```

### Resource Ownership Rules
1. `DxgiCapturer` owns the Direct3D 11 device, context, and a persistent `retainedTexture_` (`ID3D11Texture2D`).
2. When `AcquireFrame(timeoutMs, outFrame)` executes:
   - `duplication->AcquireNextFrame` locks the latest desktop image.
   - `context->CopyResource(retainedTexture, acquiredTexture)` copies image data purely inside GPU VRAM without reading back to host system RAM.
   - `duplication->ReleaseFrame()` releases DXGI's exclusive lock immediately.
   - `outFrame.d3d11Texture` points to `retainedTexture`. It remains valid and safe to read until the next `AcquireFrame()` call or `Shutdown()`.
3. When `Shutdown()` is invoked, all COM resources (`ID3D11Device`, `ID3D11DeviceContext`, `IDXGIOutputDuplication`, `ID3D11Texture2D`) are safely released via Microsoft WRL `ComPtr`.

---

## 3. Toolchain & Build Prerequisites

### Minimum Requirements
- **Operating System**: Windows 10 (Version 1607+) or Windows 11 64-bit.
- **Graphics Hardware**: Direct3D 11 feature level `10_0` or higher (verified on Intel HD Graphics 520, Skylake WDDM 3.1).
- **C++ Compiler**: GCC 16.2.0 (UCRT MinGW-w64 via WinLibs) or Visual Studio 2022 C++ Build Tools (`cl.exe`).
- **Build System**: CMake 3.22+ and Ninja.

### Build Commands
Run from PowerShell in the repository root:

```powershell
$toolchainBin = "C:\Users\Aryan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
$env:PATH = "$toolchainBin;$env:PATH"

# 1. Configure CMake
cmake -B "native/windows_engine/build" -S "native/windows_engine" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# 2. Build library, executable, tests, and smoke test
cmake --build "native/windows_engine/build"
```

---

## 4. Hardware Capture Smoke Test

A dedicated hardware smoke test executable (`secondscreen_smoke_capture.exe`) validates real desktop frame capture against the active display.

### Command
```powershell
$toolchainBin = "C:\Users\Aryan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
$env:PATH = "$toolchainBin;$env:PATH"

& ".\native\windows_engine\build\secondscreen_smoke_capture.exe" --frames 15 --timeout 150
```

### Actual Verified Output
```
========================================================
  SecondScreen DXGI Desktop Capture Smoke Test          
========================================================
Configuration:
  Target Frames: 15
  Monitor Index: 0
  Acquisition Timeout: 150 ms
--------------------------------------------------------
[INFO] Initializing Direct3D 11 and DXGI Desktop Duplication...
[INFO] Attached Device: \\.\DISPLAY1
[INFO] Desktop Resolution: 1366x768 (DXGI Format: 87)
[INFO] Commencing frame acquisition loop...

  [FRAME #  1] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.493 ms
  [FRAME #  2] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.081 ms
  [FRAME #  3] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.087 ms
  [FRAME #  4] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.083 ms
  [FRAME #  5] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.08 ms
  [FRAME # 10] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.075 ms
  [FRAME # 15] 1366x768 | GPU Texture: 0x23b19e9b320 | Acq Time: 0.089 ms

--------------------------------------------------------
Capture Smoke Test Results:
  Frames Acquired:      15 / 15
  Timeouts Encountered: 0
  Total Duration:       0.41 s
  Effective Rate:       36.55 FPS
  Avg Acquisition Time: 0.11 ms
--------------------------------------------------------
[INFO] Shutting down and releasing DXGI / D3D11 resources...
[PASS] Smoke test completed successfully with clean resource shutdown.
```

---

## 5. Metrics & Telemetry

The capture engine updates `MetricsTracker` atomically for every frame event:
- **`captureFps`**: Rate of frames acquired per second.
- **`avgAcquisitionDurationMs`**: Mean time spent executing `AcquireNextFrame` and `CopyResource`.
- **`totalTimeouts`**: Number of times `AcquireNextFrame` timed out because the desktop was static.
- **`totalCaptureErrors`**: Number of unexpected DXGI errors encountered.
- **`totalRecoveries`**: Number of successful automatic session recreations.

---

## 6. Error Handling & Recovery

| DXGI Return Code | Meaning | Handling Strategy |
|---|---|---|
| `S_OK` | Frame successfully acquired. | GPU copy performed, frame lock released, metrics updated. |
| `DXGI_ERROR_WAIT_TIMEOUT` | No display updates occurred during `timeoutMs`. | Non-fatal; returns `false`, increments timeout counter, loop continues. |
| `DXGI_ERROR_ACCESS_LOST` | Desktop mode or resolution changed, or UAC switch occurred. | Invalidates session, triggers `RecoverSession()` with exponential backoff (up to 5 retries). |
| `DXGI_ERROR_DEVICE_REMOVED` / `RESET` | GPU driver restarted or device lost. | Tears down D3D11 device and context, re-enumerates adapters, and recreates pipeline. |

---

## 7. Next Steps for Milestone C: Hardware H.264 Encoding

With native desktop capture delivering GPU-resident `ID3D11Texture2D` surfaces in **~0.11 ms**:
1. Implement `native/windows_engine/src/encoder/mf_h264_encoder.cpp` implementing `IEncoder`.
2. Configure Windows Media Foundation to instantiate the **Intel® Quick Sync Video H.264 Encoder MFT** (`{4BE8D3C0-0515-4A37-AD55-E4BAE19AF471}`).
3. Share the D3D11 device via `IMFDXGIDeviceManager` so the encoder consumes `retainedTexture_` directly in GPU VRAM without CPU staging.
4. Extract SPS/PPS parameters for `PACKET_TYPE_CONFIG` and package Annex B access units for `PACKET_TYPE_KEYFRAME` and `PACKET_TYPE_DELTA`.
