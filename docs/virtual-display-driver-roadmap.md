# SecondScreen — Phase 6: Windows Indirect Display Driver (IDD) Architecture & Roadmap

## 1. Purpose & Core Objective

The core objective of **Phase 6** is to integrate a native Windows **Indirect Display Driver (IDD)** into SecondScreen, enabling Windows to recognize an independent, virtual secondary monitor whose video output is captured, encoded, and streamed to an Android tablet or phone over local Wi-Fi.

### 1.1 The Fundamental Limitation of Desktop Duplication
In Phase 4 and Phase 5, SecondScreen established high-performance screen capture using the DXGI Desktop Duplication API (`IDXGIOutputDuplication`). However:
> [!IMPORTANT]
> **Capturing the primary display does not, by itself, create an extended monitor.**
> Desktop Duplication can only capture displays that already physically exist in the Windows display topology. It duplicates pixels that are already being rendered for an existing physical screen.

### 1.2 The True Extended Desktop Experience
To transform an Android tablet into a genuine secondary monitor, SecondScreen must integrate with the Windows display driver architecture so that:
1. Windows Plug and Play (PnP) detects a separate, virtual monitor (`\\.\DISPLAY2`).
2. The user can open **Windows Display Settings** (`ms-settings:display`) and choose **"Extend these displays"**.
3. Windows Desktop Window Manager (DWM) allocates an independent desktop canvas with its own coordinate space, resolution, and refresh rate.
4. Windows applications (e.g. browser, code editor, terminal) can be dragged across the screen boundary onto the virtual monitor.
5. SecondScreen acquires the virtual monitor's output, encodes it with Media Foundation H.264, and streams it via SS02 to Android.
6. The Android screen displays the distinct extended desktop while the PC's primary monitor remains independently usable.

---

## 2. Windows Driver Architecture & Verification

The architecture is designed according to Microsoft's official driver specifications:

### 2.1 Display Driver Model & IddCx
- **Driver Architecture:** **User-Mode Driver Framework (UMDF 2.x)** using the **Indirect Display Driver Class Extension (IddCx)** (`IddCx.dll`).
- **Execution Context:** The driver executes entirely in user space within the Windows Driver Foundation host process (`WUDFHost.exe`, Session 0).
- **Crash Isolation:** Because the driver runs in user mode, any fatal driver exception or memory fault is caught by Windows. The OS cleanly drops the virtual monitor without causing a **Kernel Blue Screen of Death (BSOD)**. The primary monitor remains active and undisturbed.
- **IddCx Framework Versions:**
  - *IddCx 1.4:* Supported on Windows 10 (Version 1903+) and Windows 11.
  - *IddCx 1.9:* Supported on Windows 11 (Version 22H2+).
  - *IddCx 1.10:* Supported on Windows 11 (Version 24H2+).
  - *Target Framework:* **IddCx 1.4** is selected as the baseline to ensure compatibility across all modern Windows 10 (1903+) and Windows 11 releases.

### 2.2 Toolchain & Development Environment Prerequisites
Compiling the driver binary (`SecondScreenIdd.dll`) requires Microsoft's official driver toolchain:
- **Build Engine:** Microsoft Visual Studio 2022 (Community, Professional, or Enterprise) with MSBuild.
- **C++ Workload:** *Desktop development with C++* with MSVC v143 x64 build tools.
- **SDK:** Windows 11 SDK (Build 10.0.22621.0 or 10.0.26100.0).
- **WDK:** Windows Driver Kit (WDK) 11 matching the installed SDK version.
- **Required Proprietary Headers & Libs:** `iddcx.h`, `wdf.h`, `wudfwdm.h`, and `IddCx.lib`.

#### Audit of Current Host Environment:
- **OS Version:** Windows 11 Insider Preview (Build `10.0.26200.9711`) — **Verified Compatible**.
- **IddCx Runtime:** `C:\Windows\System32\IddCx.dll` — **Verified Present in OS**.
- **PnP Utility:** `C:\Windows\System32\pnputil.exe` — **Verified Present**.
- **MSVC & WDK:** **Not Installed** on the current machine (only MinGW GCC, CMake, and Ninja are present).
  *Conclusion:* Compiling the driver binary requires provisioning Visual Studio 2022 + WDK 11 on the development machine.

### 2.3 Official Microsoft Documentation & Sample References
- **Microsoft IddSample Repository:**  
  [`https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay`](https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay)  
  *The canonical Microsoft sample demonstrating UMDF 2.x, `IddCxDeviceInit`, `IddCxAdapterInitAsync`, synthetic EDID injection, and mode enumeration.*
- **Microsoft Learn — Indirect Display Driver Overview:**  
  [`https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-overview`](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-overview)
- **Microsoft Learn — IddCx Architecture and Model:**  
  [`https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iddcx-architecture`](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iddcx-architecture)
- **Microsoft Learn — Windows Driver Kit (WDK) Installation:**  
  [`https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk`](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)

---

## 3. Proposed Component Architecture & Boundary

SecondScreen enforces a strict separation between the Windows driver and the user-mode streaming application:

```
+---------------------------------------------------------------------------------------------------+
|                               SecondScreen Phase 6 System Architecture                            |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|  [Windows Display Subsystem]                                                                      |
|  +-------------------------+      +-------------------------+      +---------------------------+  |
|  | Windows Display Settings| ---> |  Desktop Window Manager | ---> |   Extended Desktop Space  |  |
|  | (ms-settings:display)   |      |  (Composes Primary + IDD|      |   (Primary: 1920x1080)    |  |
|  +------------+------------+      +------------+------------+      |   (Virtual: 1920x1080)    |  |
|               |                                |                   +-------------+-------------+  |
|               v                                v                                 |                |
|  [User-Mode Driver Framework: WUDFHost.exe (Session 0)]                          |                |
|  +----------------------------------------------------------------------------+  |                |
|  |                   SecondScreenIdd.dll (UMDF 2.x Driver)                    |  |                |
|  |  - IddCxDeviceInit, IddCxAdapterInitAsync                                  |  |                |
|  |  - IddCxMonitorCreate, IddCxMonitorArrival                                 |  |                |
|  |  - Injects Virtual EDID (Manufacturer: "SCS", Model: "SecondScreen")       |  |                |
|  |  - Exposes Modes: 1920x1080 @ 60Hz, 1280x720 @ 60Hz                        |  |                |
|  +-------------------------------------+--------------------------------------+  |                |
|                                        | Creates Virtual Output                  |                |
|                                        v (\\.\DISPLAY2)                          v                |
|  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |
|                                                                                                   |
|  [SecondScreen Windows Streaming Host: secondscreen_engine.exe (User Mode)]                       |
|  +---------------------------------------------------------------------------------------------+  |
|  |  1. DxgiCapturer (Configured for monitorIndex = 1, targeting \\.\DISPLAY2)                  |  |
|  |  2. MfH264Encoder (Media Foundation H.264 MFT, Low Latency, Dynamic Bitrate)                |  |
|  |  3. AdaptiveStreamingController (AIMD rate & frame-rate control)                           |  |
|  |  4. TcpTransportServer (SS02 binary framing, port 52025)                                   |  |
|  +---------------------------------------------+-----------------------------------------------+  |
|                                                |                                                  |
|  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |
|                                                | SS02 TCP Protocol Stream                         |
|                                                v                                                  |
|  [Android Client: apps/android_app]                                                               |
|  +---------------------------------------------------------------------------------------------+  |
|  |  1. DefaultConnectionService (SS02 reassembly & 1 Hz telemetry feedback)                    |  |
|  |  2. SecondScreenDecoderBridge (Platform Channel)                                           |  |
|  |  3. H264Decoder (Native Android MediaCodec low-latency video/avc)                            |  |
|  |  4. VideoSurfacePlatformView (Native GPU SurfaceView rendering extended desktop)            |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

### 3.1 Responsibilities Breakdown

#### 1. Windows Virtual Display Driver (`SecondScreenIdd.dll`)
- **Scope:** Runs in `WUDFHost.exe` (Session 0).
- **Core Duties:**
  - Instantiates a virtual software display adapter via `IddCxAdapterInitAsync`.
  - Exposes synthetic monitor arrival via `IddCxMonitorArrival`.
  - Injects a valid CEA-861 compliant EDID block declaring 16:9 and 16:10 desktop resolutions.
  - Reports preferred modes: `1920x1080 @ 60 Hz` and `1280x720 @ 60 Hz`.
  - Responds to DWM swapchain assignments and mode change notifications.
- **Strict Boundary:** The driver contains **zero networking, zero video compression, zero audio processing, and zero UI logic**.

#### 2. Windows User-Mode Streaming Engine (`secondscreen_engine.exe`)
- **Scope:** Standard Windows user-mode application running in the user session.
- **Core Duties:**
  - Queries `IDXGIFactory1::EnumAdapters1` and `IDXGIAdapter1::EnumOutputs`.
  - Locates the active virtual monitor output (`\\.\DISPLAY2`) attached to the desktop.
  - Captures virtual monitor frames at native GPU speeds via `IDXGIOutput1::DuplicateOutput()`.
  - Stages textures in VRAM and encodes them into H.264 access units using `MfH264Encoder`.
  - Applies Phase 5 AIMD adaptive bitrate and frame-rate control.
  - Transmits packets over TCP port 52025 to the Android client.

#### 3. Android Client (`apps/android_app`)
- **Scope:** Flutter application with native Kotlin `MediaCodec` integration.
- **Core Duties:**
  - Reassembles SS02 network packets.
  - Extracts SPS/PPS codec configuration and feeds access units into hardware decoder.
  - Presents rendered video directly on a hardware-accelerated native `SurfaceView`.
  - Dispatches 1 Hz telemetry feedback (`PACKET_TYPE_RECEIVER_REPORT`) and handles keyframe requests.

---

## 4. Frame Acquisition Strategy & Trade-Off Analysis

How the user-mode streaming host acquires rendered pixels from the virtual display is a critical design decision. Two architectural approaches were investigated:

```
Option 1: DXGI Desktop Duplication on Virtual Output (Selected)
=============================================================================
[DWM Composition] ---> [Virtual Adapter IDXGIOutput] ---> [DxgiCapturer] ---> [MfH264Encoder]
                                                           (User Mode)          (User Mode)

Option 2: In-Driver Swapchain Buffer Extraction (Rejected)
=============================================================================
[DWM Composition] ---> [IddCx Driver Swapchain] =====Shared Memory=====> [Streaming Engine]
                       (WUDFHost.exe Session 0)   (IPC / Handle Passing)  (User Mode)
```

### 4.1 Evaluation of Alternatives

| Evaluation Criteria | Option 1: DXGI Desktop Duplication on Virtual Output | Option 2: In-Driver Swapchain Extraction |
|---|---|---|
| **Driver Complexity** | **Minimal:** Driver only manages monitor PnP and EDID. | **High:** Driver must manage swapchain buffers, shared DirectX handles, and IPC. |
| **Driver Stability** | **Maximum:** No inter-process communication in driver; driver cannot crash from client disconnects. | **Risky:** IPC pipe stalls or buffer backpressure can lock the driver worker thread. |
| **Zero-Copy GPU Path**| **Yes:** Direct3D 11 GPU-to-GPU `CopyResource` inside user-mode process. | **Complex:** Requires opening shared resource handle across Session 0 / Session 1 boundary. |
| **Code Reuse** | **100%:** Reuses existing `DxgiCapturer` implementation by passing `--monitor 1`. | **0%:** Requires writing a custom shared-memory IPC layer. |
| **Microsoft Support** | Fully supported in modern Windows 10/11 DWM for active IddCx monitors. | Supported via `IddCxSwapChainReleaseAndAcquireBuffer`. |

### 4.2 Selected Architecture & Technical Details
**Option 1 is selected.** In Windows 10 (1903+) and Windows 11, when an IddCx virtual monitor is set to "Extend desktop to this display", DWM treats it as an active desktop target.
- The virtual monitor is assigned an `IDXGIOutput` interface on the display adapter.
- The existing [`DxgiCapturer`](file:///d:/Second%20Display/native/windows_engine/src/capture/dxgi_capturer.cpp) already enumerates all attached outputs via `curAdapter->EnumOutputs(oIdx, &curOutput)`.
- By setting `config.monitorIndex = 1`, `DxgiCapturer` acquires the virtual monitor's `IDXGIOutput1` interface and calls `DuplicateOutput()`.
- Desktop frames are acquired with GPU-resident latency (~0.18 ms), preserving full pipeline compatibility.

---

## 5. Driver Safety & Recovery Plan

Because display drivers interact directly with the Windows Desktop Window Manager, the development process requires robust failure isolation and recovery mechanisms:

### 5.1 Development Safeguards
1. **User-Mode Fault Isolation:** Running under UMDF 2.x ensures driver crashes never generate a kernel bug check (BSOD). Windows logs the crash in Event Viewer (`Application` log, Source: `DriverFrameworks-UserMode`) and restarts the host process.
2. **Dedicated Test-Signing:**
   - The driver is signed using a local test certificate:
     ```powershell
     bcdedit /set testsigning on
     ```
   - *Security Rule:* **Never disable driver signature verification or Windows security features globally.** Test-signing mode restricts execution strictly to explicitly trusted test certificates.
3. **No Unbounded Memory Buffers:** The driver does not maintain internal video queues.

### 5.2 Clean Installation & Recovery Commands
```powershell
# 1. Install driver package into Windows Driver Store and attach virtual device
pnputil /add-driver SecondScreenIdd.inf /install

# 2. Check device status in PnP Manager
Get-PnpDevice -FriendlyName "*SecondScreen*"

# 3. Disable virtual monitor cleanly
pnputil /disable-device "Root\SecondScreenIdd"

# 4. Force-uninstall driver package from Driver Store
pnputil /delete-driver oem*.inf /uninstall /force

# 5. Disable test-signing after verification
bcdedit /set testsigning off
```

### 5.3 Emergency Recovery
If a development driver causes display instability:
- Boot Windows into **Safe Mode** (Shift + Restart &rarr; Startup Settings &rarr; Safe Mode).
- UMDF drivers are prevented from loading in Safe Mode.
- Run `pnputil /delete-driver oem*.inf /uninstall /force` to remove the development package, then reboot normally.

---

## 6. Phase 6 Implementation Milestones

```
+-------------------------------------------------------------------------------+
|                       Phase 6 Milestone Progression                           |
+-------------------------------------------------------------------------------+
|                                                                               |
|  [Milestone A: Environment & Official Sample Build]                           |
|  - Provision Visual Studio 2022 + WDK 11 on development machine               |
|  - Clone and build canonical Microsoft IddSample                              |
|  - Exit Criterion: IddSample builds cleanly with 0 errors.                    |
|                                                                               |
|  [Milestone B: Minimal Virtual Monitor Implementation]                        |
|  - Implement Driver.cpp, Device.cpp, Monitor.cpp in native/virtual_display_... |
|  - Generate test certificate & sign package                                   |
|  - Install via pnputil on test Windows machine                                |
|  - Exit Criterion: Windows enumerates \\.\DISPLAY2; Extended mode selectable. |
|                                                                               |
|  [Milestone C: Virtual Desktop Frame Acquisition]                             |
|  - Validate DxgiCapturer on \\.\DISPLAY2 (monitorIndex = 1)                   |
|  - Verify 1920x1080 @ 60 FPS texture capture with zero CPU readback           |
|  - Exit Criterion: secondscreen_engine acquires live virtual desktop frames.   |
|                                                                               |
|  [Milestone D: Existing Encoder & Network Integration]                        |
|  - Route virtual frames through MfH264Encoder & TcpTransportServer            |
|  - Retain Phase 5 AIMD adaptive bitrate and framerate control                 |
|  - Exit Criterion: Windows host streams virtual display over SS02 TCP.        |
|                                                                               |
|  [Milestone E: Android Extended Desktop Display]                              |
|  - Connect Android client to virtual monitor stream                           |
|  - Drag a Windows application window onto the virtual monitor                 |
|  - Exit Criterion: Android displays independent window; PC display distinct.  |
|                                                                               |
|  [Milestone F: Stability, Reconnect & Performance Hardening]                  |
|  - Test 15-minute extended run, repeated disconnect/reconnect cycles          |
|  - Measure latency, resource usage, and clean driver removal                  |
|  - Exit Criterion: Zero memory leaks, clean uninstallation verified.          |
|                                                                               |
+-------------------------------------------------------------------------------+
```

---

## 7. Testing & Validation Matrix

| Test Case | Expected Result | Verification Method | Current Status |
|---|---|---|---|
| **1. Driver Package Build** | `SecondScreenIdd.dll` and `.cat` compile with 0 errors | MSBuild / Visual Studio log | **Pending (WDK Toolchain Pending)** |
| **2. Test-Signing & Installation** | Driver installs cleanly into Windows Driver Store | `pnputil /add-driver ... /install` | **Pending** |
| **3. Virtual Monitor Enumeration** | Windows detects `SecondScreen Virtual Display` | Device Manager & `Get-PnpDevice` | **Pending** |
| **4. Extended Mode Selection** | "Extend these displays" enabled in display settings | `ms-settings:display` inspection | **Pending** |
| **5. DXGI Output Duplication** | `DuplicateOutput()` succeeds on secondary monitor | `DxgiCapturer` log output | **Implemented in User-Mode** |
| **6. Encoder Integration** | Virtual desktop frames encoded as H.264 access units | `secondscreen_engine.exe` log | **Implemented in User-Mode** |
| **7. SS02 Protocol Streaming** | Frames transmitted over port 52025 with SS02 headers | `secondscreen_protocol_tests` | **Integration-test-verified** |
| **8. Android Hardware Decode** | Android `MediaCodec` decodes virtual desktop stream | `flutter test` / `app-debug.apk` | **Unit-test-verified** |
| **9. Window Dragging Test** | Window dragged to virtual screen appears on Android | Visual validation on Android | **Pending (Awaiting Driver)** |
| **10. Safe Driver Uninstallation** | Driver uninstalled; primary display remains stable | `pnputil /delete-driver ... /force` | **Pending** |

---

## 8. Performance Considerations

1. **Resolution vs. Bandwidth Trade-Off:**
   - At `1920x1080 @ 60 FPS`, raw uncompressed BGRA video requires ~3.98 Gbps.
   - H.264 compression reduces this to **4,000–6,000 Kbps**, achieving a **~800:1 compression ratio** suitable for standard 5 GHz Wi-Fi.
2. **GPU Memory Sharing:**
   - DWM renders the virtual monitor surface directly in GPU VRAM.
   - `DxgiCapturer` copies this surface using `ID3D11DeviceContext::CopyResource`, keeping pixels 100% inside GPU memory until NV12 encoding.
3. **Backpressure & Latency Control:**
   - The Phase 5 AIMD adaptive controller monitors send queue delay ($t_{\text{send}}$) and receiver telemetry. If Wi-Fi bandwidth drops, the host throttles bitrate dynamically via `ICodecAPI`, preventing TCP bufferbloat without stalling DWM.

---

## 9. Compatibility & Distribution Strategy

- **Development Testing:**
  - Deployed locally with test-signing enabled (`bcdedit /set testsigning on`) and a self-signed root certificate.
- **Production Distribution Requirements:**
  - Commercial distribution requires submitting the driver package to the **Microsoft Windows Hardware Developer Center Dashboard** for **WHQL certification** or **Attestation Signing**.
  - Attestation signing produces an official Microsoft signature that installs seamlessly on retail Windows 10/11 machines without requiring test-signing mode.
- **Supported Operating Systems:**
  - Windows 10 (Version 1903 / Build 18362 and newer).
  - Windows 11 (All builds, including 21H2, 22H2, 23H2, and 24H2).

---

## 10. Sources & Official Microsoft References

1. **Microsoft Windows Driver Samples — Indirect Display Driver (IddSample):**  
   [`https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay`](https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay)
2. **Microsoft Learn — Indirect Display Driver Overview:**  
   [`https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-overview`](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-overview)
3. **Microsoft Learn — IddCx Architecture and Objects:**  
   [`https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iddcx-architecture`](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iddcx-architecture)
4. **Microsoft Learn — Windows Driver Kit (WDK) Setup:**  
   [`https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk`](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
5. **Microsoft Learn — Driver Signing Requirements:**  
   [`https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing`](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing)

---

## 11. Current Status & Immediate Next Actions

### 11.1 Component Status Summary
- **Windows User-Mode Streaming Engine (`secondscreen_engine.exe`):** **Implemented & Integration-test-verified** (DXGI capture, MFT H.264 encode, Winsock2 transport, AIMD rate control).
- **Android Display Client (`apps/android_app`):** **Implemented & Unit-test-verified** (SS02 packet reassembly, native `MediaCodec` low-latency decoding, `SurfaceView` platform view).
- **Windows IDD Driver (`SecondScreenIdd.dll`):** **Planned & Architected** (Awaiting Visual Studio + WDK toolchain).

### 11.2 Immediate Next Actions
1. Install **Visual Studio 2022** and **Windows Driver Kit (WDK) 11** on the development workstation.
2. Build and verify the canonical Microsoft `IddSample` project.
3. Implement `native/virtual_display_driver/SecondScreenIdd` and install the test-signed driver.
4. Verify `\\.\DISPLAY2` enumeration and test dragging windows onto the Android secondary display.
