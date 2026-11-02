# DuetView

> **Turn a compatible Android device into a companion display for your Windows PC.**

[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Android-blue.svg)](https://github.com/Aryan130805/DuetView)
[![Release](https://img.shields.io/badge/Release-v0.4.0%20(Beta)-emerald.svg)](https://github.com/Aryan130805/DuetView/releases)
[![Build & Tests](https://img.shields.io/badge/Tests-81%20Android%20%7C%204%20C%2B%2B%20Passing-brightgreen.svg)](https://github.com/Aryan130805/DuetView/actions)
[![Language](https://img.shields.io/badge/Languages-Dart%20%7C%20C%2B%2B20%20%7C%20Kotlin-purple.svg)](https://github.com/Aryan130805/DuetView)

DuetView is a free, high-performance display streaming and companion screen solution that connects your Windows PC to an Android phone or tablet over local Wi-Fi or USB tethering.

---

## 📥 Download DuetView

Official pre-compiled binaries are published in the [GitHub Releases](https://github.com/Aryan130805/DuetView/releases) section.

| Platform | Package | Description | Release Asset Link |
|---|---|---|---|
| **Android** | `DuetView-Android.apk` | Companion display client for Android 8.0+ | [Download Android APK](https://github.com/Aryan130805/DuetView/releases/download/v0.4.0/DuetView-Android.apk) |
| **Windows PC** | `DuetView-PC-Setup.exe` | Standalone installer for Windows 10/11 (64-bit) | [Download Windows Installer](https://github.com/Aryan130805/DuetView/releases/download/v0.4.0/DuetView-PC-Setup.exe) |
| **Windows PC** | `DuetView-PC-0.4.0-win64.zip` | Portable standalone package (no installation required) | [Download Portable ZIP](https://github.com/Aryan130805/DuetView/releases/download/v0.4.0/DuetView-PC-0.4.0-win64.zip) |

> [!NOTE]
> Downloads become accessible upon publication of the `v0.4.0` release tag on GitHub. You can always find the latest builds directly on the [DuetView Releases Page](https://github.com/Aryan130805/DuetView/releases).

### Release Asset SHA-256 Checksums (v0.4.0)

```
05c5cf02642801b315080cbc8d0749f6a25fba2ee689f18d4064d62695c90b5a  DuetView-Android.apk
0fe3d3561893dc3257014c56b393dbe5a5c18a3904aab025783b52f8c295a2c6  DuetView-PC-Setup.exe
7e0b0589de23fc5349ec2c0dedc1f6207fb6f2c9bf76cdbdb9b19ab3b6072667  DuetView-PC-0.4.0-win64.zip
```

---

## ✨ Features

- **Driver-Free Desktop Mirroring**: Stream your primary Windows desktop to your Android device immediately without installing any drivers or restarting your PC.
- **Extended Display Mode**: Bundled with a Microsoft Indirect Display Driver (IddCx / UMDF 2.x) to provide a true secondary monitor for independent desktop workspaces (`Win + P` -> *Extend*).
- **GPU Screen Capture**: Direct3D 11 and DXGI Desktop Duplication API capture frames directly from your GPU with sub-millisecond acquisition times.
- **Hardware-Accelerated H.264 Pipeline**:
  - Windows: Hardware video encoding via Windows Media Foundation (`IMFTransform`) with dynamic bitrate scaling (2,000 to 12,000 Kbps).
  - Android: Zero-copy direct hardware decoding via Android `MediaCodec` rendering straight to `SurfaceView`.
- **High Refresh Rate Support**: Configurable target frame rates supporting 30 FPS, 60 FPS (default), 90 FPS, and 120 FPS.
- **Bi-Directional Input Forwarding**:
  - Multi-touch gesture mapping (tap, drag, pinch, scroll) normalized to Windows virtual desktop coordinates.
  - On-screen virtual keyboard with modifier key support (Ctrl, Alt, Shift, Win, Esc, Tab, arrow keys) injected via Windows `SendInput`.
- **Instant QR Pairing**: Secure, one-tap connection using cryptographically generated 5-minute QR pairing tokens.
- **Saved PC Management**: Remembers your computers with automatic endpoint discovery, DHCP migration recovery, and live background health pings.
- **Ultra-Low Latency Over USB**: Connect via standard USB cable with Android USB tethering for lightning-fast sub-5ms display latency.
- **100% Local & Private**: Direct peer-to-peer connection over your local LAN or USB cable. Zero cloud dependencies, zero account creation, zero subscription fees.

---

## 🏗️ Architecture

```
DuetView/
├── apps/
│   └── android_app/               # Flutter client & Kotlin MediaCodec SurfaceView bridge
│       ├── lib/                   # UI (Home, Streaming, Connection, Guide, Settings)
│       ├── android/               # Native Android video decoder & platform views
│       └── test/                  # Automated Flutter unit & widget tests (81 tests)
├── native/
│   ├── windows_engine/            # C++20 DXGI Desktop Duplication & MF H.264 streaming engine
│   │   ├── src/app/               # Desktop UI (main_window.cpp) with sidebar navigation
│   │   ├── src/capture/           # DXGI Desktop Duplication capturer & cursor manager
│   │   ├── src/encoder/           # Media Foundation H.264 hardware encoder & NAL parser
│   │   ├── src/pairing/           # QR code generator & pairing authentication server
│   │   ├── src/input/             # Windows SendInput touch, mouse, and keyboard injector
│   │   └── tests/                 # C++ ctest test suites (Protocol, Capture, Encoder, Driver)
│   ├── windows_installer/         # Standalone native installer (DuetView-PC-Setup.exe)
│   └── virtual_display_driver/    # Microsoft IddCx UMDF 2.x virtual display driver source
├── shared/
│   └── protocol/                  # SecondScreen SS02 binary packet framing & specifications
├── dist/                          # Packaged release binaries (excluded from git, hosted on Releases)
└── docs/                          # Architecture guides, diagnostics, and driver manuals
```

### Protocol & Networking Stack

- **Port 52025 (TCP)**: High-speed video stream carrying SecondScreen SS02 H.264 packets (SPS/PPS configuration, keyframes, delta frames) and upstream input packets (touch, scroll, keyboard, keyframe requests, receiver reports).
- **Port 52026 (UDP)**: Automatic local LAN beacon for zero-configuration discovery of active DuetView hosts.
- **Port 52027 (TCP)**: Cryptographic pairing authentication server validating QR tokens.

---

## 📋 Prerequisites

### For End Users
- **PC**: 64-bit Windows 10 or Windows 11.
- **Android Device**: Android 8.0 (Oreo / API 26) or higher.
- **Network**: A shared 5 GHz Wi-Fi router (recommended) or a USB data cable with USB Tethering enabled.

### For Developers
- **Flutter SDK**: 3.x (Dart 3.x)
- **Android SDK & NDK**: API 34+ with JDK 17
- **CMake**: 3.20 or newer
- **C++ Compiler**: GCC/MinGW-w64 (C++20) or Microsoft Visual Studio 2022 (MSVC)
- **Windows Driver Kit (WDK)**: Only required if modifying and re-compiling the virtual display driver (`SecondScreenIdd.dll`).

---

## 🛠️ Build Instructions

### 1. Build the Android Application (`DuetView-Android.apk`)

```bash
cd apps/android_app
flutter pub get
flutter test
flutter build apk --release
```
The compiled APK is output to `apps/android_app/build/app/outputs/flutter-apk/app-release.apk`.

### 2. Build the Windows PC Application (`DuetView-PC.exe`)

```bash
cmake -B native/windows_engine/build -S native/windows_engine -DCMAKE_BUILD_TYPE=Release
cmake --build native/windows_engine/build --config Release --target DuetView-PC
ctest --test-dir native/windows_engine/build --output-on-failure
```
The unified Windows desktop executable is output to `native/windows_engine/build/DuetView-PC.exe`.

### 3. Build the Windows Installer (`DuetView-PC-Setup.exe`)

```bash
cmake --build native/windows_engine/build --config Release --target DuetViewPC-Setup
```
The standalone installer executable is output to `native/windows_engine/build/DuetView-PC-Setup.exe`.

---

## 🚀 Installation & Usage

### 1. Android Installation
1. Download `DuetView-Android.apk` from the [Releases](https://github.com/Aryan130805/DuetView/releases) page.
2. Open the downloaded APK on your Android device and tap **Install** (allow installation from unknown sources if prompted).
3. Grant Camera permission when prompted (required for scanning the pairing QR code).

### 2. Windows PC Setup
- **Installer**: Run `DuetView-PC-Setup.exe` and follow the on-screen setup wizard.
- **Portable**: Extract `DuetView-PC-0.4.0-win64.zip` to any folder and double-click `DuetView-PC.exe`.

### 3. Pairing & Connecting

#### Over Wi-Fi:
1. Ensure both your PC and Android device are connected to the same Wi-Fi network.
2. Launch `DuetView-PC.exe` on your computer. The host service starts automatically and displays a pairing QR code framed with cyan brackets.
3. Open DuetView on your Android phone or tablet and tap **"Scan to Connect"**.
4. Point your camera at the QR code on your PC monitor. Video streaming starts instantly!

#### Over USB (Sub-5ms Latency):
1. Connect your Android device to your PC using a USB cable.
2. On your Android device, go to **Settings > Network & internet > Hotspot & tethering** and enable **USB tethering**.
3. Launch DuetView on both devices. Tap **"Discover PCs"** or **"Scan to Connect"** on Android to stream over the ultra-fast direct USB tunnel.

### Mirroring vs. Extended Display

- **Desktop Mirroring (Default)**: Ready immediately without installing any driver. Mirrors your main PC screen to your Android device with touch and keyboard interaction.
- **Extended Display Mode**: To create an independent virtual second screen, click **"Install Virtual Display Driver"** in DuetView PC (requires administrator privileges). Once installed, press `Win + P` and select **"Extend"** in Windows.

---

## 🔧 Troubleshooting

| Symptom | Cause | Solution |
|---|---|---|
| **QR Scan Fails** | Camera cannot focus or token expired | Tap **"Regenerate QR"** in DuetView PC for a fresh 5-minute token, or use **"Discover PCs"** / **"Manual IP"**. |
| **Connection Refused / Timeout** | Windows Firewall blocking inbound traffic | Open Windows Defender Firewall and ensure inbound TCP port 52025 and 52027 are allowed for `DuetView-PC.exe`. |
| **High Latency or Stuttering** | Congested 2.4 GHz Wi-Fi band or distance from router | Switch to a 5 GHz Wi-Fi channel, or use a USB cable with Android USB tethering enabled. |
| **Virtual Monitor Not Showing** | Driver installed but Extend mode not activated | Press `Win + P` and choose **"Extend"**, or open Windows Display Settings (`ms-settings:display`) to arrange screens. |
| **Client Isolation on Guest Wi-Fi** | Public/enterprise router prevents peer-to-peer traffic | Connect both devices to a private home network or use direct USB tethering. |

---

## 🧪 Automated Testing

Both the Android client and the Windows native engine include automated test suites:

- **Android (Flutter & Dart)**:
  ```bash
  cd apps/android_app
  flutter test
  ```
  *Result: 81 tests passing (connection lifecycle, decoder bridge, packet accumulator, touch input serialization, widget layouts).*

- **Windows Engine (C++20 & CTest)**:
  ```bash
  ctest --test-dir native/windows_engine/build --output-on-failure
  ```
  *Result: 4 test suites passing (ProtocolTests, CaptureTests, EncoderTests, DriverTests).*

---

## 📄 License & Contributions

### License
The open-source license for DuetView is currently being finalized by the repository owner (`Aryan130805`). Please see the repository updates for license declaration. All code is currently provided for evaluation and personal use.

### Contributing
Contributions, bug reports, and feature requests are welcome! Feel free to open an issue or submit a pull request on the [DuetView GitHub Repository](https://github.com/Aryan130805/DuetView).
