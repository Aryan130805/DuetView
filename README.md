# SecondScreen

> Free, open-source Windows-to-Android secondary display solution. Turn an Android phone or tablet into a genuine extended desktop monitor for your Windows PC over high-speed local Wi-Fi or USB.

---

## 1. Project Vision

SecondScreen provides a high-performance, private, zero-cost alternative to proprietary virtual monitor tools (like Duet Display or Spacedesk).

- **Genuine Extended Desktop**: Not just screen mirroring; integrates as a genuine virtual monitor in Windows Display Settings.
- **High Performance**: Smooth 60 FPS streaming with minimal end-to-end latency where hardware permits.
- **Hardware Acceleration**: GPU screen capture (DXGI Desktop Duplication), hardware H.264 encoding (NVENC / AMF / Intel QuickSync / Media Foundation), and Android hardware decoding (`MediaCodec`).
- **Private & Local**: Zero cloud dependencies, zero account requirements, zero subscription fees, peer-to-peer over local LAN or USB.
- **Cross-Platform Interface**: Built with Flutter for clean, responsive, Material 3 client and desktop controls.

---

## 2. Architecture & Repository Structure

```
SecondScreen/
├── apps/
│   ├── android_app/           # Flutter Android application (display receiver & client)
│   └── windows_app/           # Flutter Windows management & control application
├── native/
│   ├── windows_engine/        # High-performance C++ screen capture & H.264 streaming engine
│   ├── virtual_display_driver/# Windows Indirect Display Driver (IddCx) for virtual monitor
│   └── android_decoder/       # Native Android MediaCodec / Surface render integration
├── shared/
│   └── protocol/              # Framing, handshake, packets, and network protocol definitions
├── tests/                     # Unit, integration, and latency benchmark tests
├── docs/                      # Technical documentation, protocols, and driver guides
├── .gitignore
└── README.md
```

---

## 3. Technology Stack

| Layer | Component | Technology |
|---|---|---|
| **Android Client** | UI & State | Flutter (Dart 3, Material 3) |
| | Native Decoder | Kotlin + Android `MediaCodec` + `SurfaceView` |
| **Windows Controller** | Management UI | Flutter for Windows (or standalone native tray) |
| **Windows Streaming Engine** | Capture & Encode | C++20, DXGI Desktop Duplication API, Media Foundation / NVENC |
| | Network Transport | Low-latency TCP/UDP or WebRTC data transport |
| **Virtual Monitor** | Driver | Windows Indirect Display Driver (IddCx C++) |

---

## 4. Development Roadmap

- [x] **Phase 1: Environment Inspection & Project Scaffolding**
  - Toolchain audit (Flutter, Android SDK, JDK, MSVC build tools).
  - Repository structure & Git initialization.
  - Base Flutter app configuration and sanity testing.
- [ ] **Phase 2: Flutter MVP UI & Discovery**
  - Material 3 client interface (Home, Connect, Stream view, Settings).
  - Responsive layout for Android phones and tablets.
  - Connection status, state machine, and error handling.
- [ ] **Phase 3: Initial Streaming Prototype**
  - Fast baseline capture & streaming prototype (e.g., JPEG/raw over socket).
  - End-to-end packet validation and diagnostic frame counter.
  - Baseline latency measurement.
- [ ] **Phase 4: Production Video Pipeline**
  - Direct3D 11 / DXGI Desktop Duplication screen capture.
  - Hardware H.264 video encoder.
  - Android `MediaCodec` hardware decoding directly to `Surface`.
  - Latency and frame drop instrumentation.
- [ ] **Phase 5: Genuine Extended Display (IddCx Driver)**
  - Microsoft IddCx driver implementation.
  - Virtual monitor creation, EDID management, and resolution negotiation.
  - Windows extended display integration.
- [ ] **Phase 6: Input Forwarding & Hardening**
  - Touch, mouse, and keyboard input forwarding.
  - Secure local pairing and AES handshake.
  - Auto-reconnection and USB ADB tunnel support.

---

## 5. Development Prerequisites

- **Flutter SDK**: 3.47+ (Stable)
- **Dart SDK**: 3.13+
- **JDK**: Eclipse Temurin OpenJDK 17
- **Android SDK**: API 36 (build-tools 36.0.0, platform-tools)
- **Git**: Installed
- **Windows C++ Build Tools**: Visual Studio 2022 C++ Build Tools (Required for Windows app & native C++ engine)
