# SecondScreen Technical Architecture

## 1. Overview

SecondScreen creates a virtual secondary monitor on a Windows host machine and streams the desktop frames with minimal latency to an Android device (phone or tablet) over Wi-Fi or USB.

```
+-------------------------------------------------------------+
|                        WINDOWS HOST                         |
|                                                             |
|  +--------------------+        +-------------------------+  |
|  | Windows OS Display | <----+ | Indirect Display Driver |  |
|  | Subsystem (DWM)    |        | (IddCx Virtual Monitor) |  |
|  +---------+----------+        +-------------------------+  |
|            |                                                |
|            v (DXGI Desktop Duplication API)                 |
|  +---------+----------+                                     |
|  | C++ Capture Engine |                                     |
|  +---------+----------+                                     |
|            | (Direct3D 11 NV12 Texture)                     |
|            v                                                |
|  +---------+----------+                                     |
|  | Hardware H.264     |                                     |
|  | Encoder (MF/NVENC) |                                     |
|  +---------+----------+                                     |
|            | (H.264 NAL Units)                              |
|            v                                                |
|  +---------+----------+        +-------------------------+  |
|  | Transport Engine   | <----> | Windows Flutter Control |  |
|  | (TCP / UDP)        |        | App (Settings & Status) |  |
|  +---------+----------+        +-------------------------+  |
+------------|------------------------------------------------+
             | Local Network (Wi-Fi 5/6 GHz or USB ADB Reverse)
+------------|------------------------------------------------+
|            v                                                |
|  +---------+----------+                                     |
|  | Network Receiver   |                                     |
|  +---------+----------+                                     |
|            |                                                |
|            v                                                |
|  +---------+----------+        +-------------------------+  |
|  | Android MediaCodec | -----> | Flutter Texture /       |  |
|  | Hardware Decoder   |        | SurfaceView Display     |  |
|  +---------+----------+        +-------------------------+  |
|                                                             |
|                       ANDROID CLIENT                        |
+-------------------------------------------------------------+
```

## 2. Key Subsystems

### A. Android Client Application (`apps/android_app`)
- **Technology**: Flutter (Dart) + Kotlin platform channel
- **Responsibilities**:
  - UI for discovery, pairing, and settings.
  - Video receiver client and frame rendering via hardware-accelerated `MediaCodec`.
  - Capturing touch and gesture input to forward back to Windows.

### B. Windows Control Application (`apps/windows_app`)
- **Technology**: Flutter for Windows / Native tray interface
- **Responsibilities**:
  - Managing host server state (Start/Stop streaming).
  - Virtual display resolution and refresh rate controls.
  - Paired client list and security authorization.

### C. Native Windows Streaming Engine (`native/windows_engine`)
- **Technology**: C++20, DirectX 11, Media Foundation
- **Responsibilities**:
  - Capturing frames from the specific virtual display adapter at 60 FPS using DXGI.
  - Low-latency hardware encoding into H.264 (intra-refresh / low latency profile).
  - Packetizing and transmitting frame stream.

### D. Virtual Display Driver (`native/virtual_display_driver`)
- **Technology**: Windows Indirect Display Driver (IddCx v1.4+)
- **Responsibilities**:
  - Reporting an extended monitor to Windows DWM.
  - Exposing custom EDID with resolutions matching client device aspect ratios (e.g., 1080p, 1440p, 16:10, 3:2).

### E. Shared Protocol (`shared/protocol`)
- **Structure**: Handshake, resolution negotiation, frame packets, ping/pong latency measurement, and touch/mouse input events.
