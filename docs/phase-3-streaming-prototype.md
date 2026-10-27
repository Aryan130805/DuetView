# Phase 3: Initial Streaming Prototype Guide

## 1. Overview

Phase 3 establishes an active, end-to-end screen capture and network frame-streaming pipeline between your Windows PC (host) and your Android device (client) over your local Wi-Fi network.

```
+-------------------------------------------------------------------+
|                        WINDOWS HOST PC                            |
|                                                                   |
|   +-------------------+      +-------------------------------+    |
|   | Desktop / Display | ---> | ScreenCapturer (mss + Pillow) |    |
|   +-------------------+      +---------------+---------------+    |
|                                              |                    |
|                                              v (JPEG Frame)       |
|                              +---------------+---------------+    |
|                              | Protocol Encoder (SS01 Header)|    |
|                              +---------------+---------------+    |
|                                              |                    |
|                                              v (TCP Packets)      |
|                              +---------------+---------------+    |
|                              | StreamingServer (Port 52025)  |    |
|                              +---------------+---------------+    |
+----------------------------------------------|--------------------+
                                               | Local Wi-Fi (TCP)
+----------------------------------------------|--------------------+
|                              +---------------+---------------+    |
|                              | Socket Stream Receiver        |    |
|                              +---------------+---------------+    |
|                                              |                    |
|                                              v (Chunk stream)     |
|                              +---------------+---------------+    |
|                              | FrameAccumulator (Reassembly) |    |
|                              +---------------+---------------+    |
|                                              |                    |
|                                              v (Complete JPEG)    |
|                              +---------------+---------------+    |
|                              | Flutter UI: Image.memory()    |    |
|                              | (Aspect Ratio & Live Metrics) |    |
|                              +-------------------------------+    |
|                                                                   |
|                       ANDROID CLIENT DEVICE                       |
+-------------------------------------------------------------------+
```

---

## 2. Windows Host Setup

### Prerequisites
- Python 3.10+ (Verified on Python 3.12.10)
- Python packages installed from `apps/windows_host/requirements.txt`

### Step 1: Install Dependencies
Open PowerShell or Command Prompt in the repository root:
```powershell
python -m pip install -r apps/windows_host/requirements.txt
```

### Step 2: Launch the Streaming Server
```powershell
python apps/windows_host/server.py
```

Optional command-line arguments:
```powershell
python apps/windows_host/server.py --port 52025 --fps 15 --width 1280 --height 720 --quality 65
```

The server will print its listening port and all active IPv4 network addresses, for example:
```
============================================================
  SecondScreen Windows Streaming Host (Phase 3 Prototype)
============================================================
Listening on port: 52025
Reachable Local IPv4 Addresses for Android connection:
  --> 192.168.1.100:52025
Configuration: 1280x720 @ 15 FPS (JPEG Quality: 65)
Press Ctrl+C to stop the server.
============================================================

[INFO] Waiting for Android device to connect...
```

---

## 3. Windows Firewall Configuration

For the Android device to connect over local Wi-Fi, Windows Defender Firewall must allow inbound TCP connections on port `52025`.

### Check or Add Firewall Rule (PowerShell as Administrator)
If your connection times out, you can allow inbound TCP on port 52025 with:
```powershell
New-NetFirewallRule -DisplayName "SecondScreen Streaming Host" -Direction Inbound -Protocol TCP -LocalPort 52025 -Action Allow
```

> [!NOTE]
> This rule only accepts connections on your local network (LAN). It does not expose your PC to the public internet unless you configure external router port forwarding.

---

## 4. Android Client Setup

### Option A: Install Built APK via ADB (Fastest)
Plug your Android phone/tablet in with USB Debugging enabled, then run:
```powershell
D:\Android\Sdk\platform-tools\adb.exe install -r "D:\Second Display\apps\android_app\build\app\outputs\flutter-apk\app-debug.apk"
```

### Option B: Run Directly from Flutter
```powershell
cd "D:\Second Display\apps\android_app"
D:\flutter\bin\flutter.bat run
```

---

## 5. How to Connect & Verify Real Frame Delivery

1. **Ensure both devices are on the same Wi-Fi network** (5 GHz Wi-Fi recommended).
2. **Launch the Windows Server**:
   ```powershell
   python apps/windows_host/server.py
   ```
   Note the IP address displayed (e.g. `192.168.1.100`).
3. **Open SecondScreen on your Android phone/tablet**.
4. Tap **Connect to PC** or **Manual IP**.
5. Enter the PC's IPv4 address and port `52025`.
6. Tap **Connect to PC**.
7. **Verify Live Streaming**:
   - The status transitions to **Streaming Live**.
   - Your Android screen immediately displays your live Windows desktop updating in real time.
   - The top HUD overlay displays:
     - Connected PC address
     - Active received frame rate (e.g. `15 FPS`)
     - Frame size (e.g. `120 KB/frame`)
     - Connection transit latency
8. Tap anywhere on the screen to toggle the controls HUD, or tap **Disconnect** to cleanly drop the session.

---

## 6. Diagnostics & Performance Metrics

| Metric | Typical Prototype Measurement | Note |
|---|---|---|
| **Capture + Resize Time** | ~12–25 ms | `mss` GDI capture + bilinear resize |
| **JPEG Encoding Time** | ~15–30 ms | Pillow standard JPEG encoder |
| **Bandwidth (15 FPS @ 720p)** | ~1.2–2.5 Mbps | Variable depending on desktop content complexity |
| **Android Receive FPS** | 12–15 FPS | Matches server pacing on stable local Wi-Fi |

---

## 7. Known Limitations & Architectural Notes

1. **Screen Mirroring vs. Virtual Extended Display**:
   - In this Phase 3 prototype, the host captures your primary Windows desktop using screen capture.
   - True extended desktop (where Windows treats the Android device as a distinct second monitor with its own workspace) is the subject of **Phase 5 (IddCx Virtual Display Driver)**.
2. **JPEG vs. Hardware H.264/H.265**:
   - JPEG streaming compresses every frame individually (all I-frames). This consumes higher bandwidth and CPU than inter-frame video codecs.
   - **Phase 4 (Production Video Pipeline)** will introduce Direct3D 11 DXGI Desktop Duplication and hardware H.264 encoding (`MediaCodec` on Android) to reach 60 FPS with minimal CPU usage.
3. **TCP vs. UDP/WebRTC**:
   - TCP guarantees ordered delivery, which means a lost packet temporarily halts subsequent frames (head-of-line blocking). On clean local Wi-Fi, this prototype performs reliably.
