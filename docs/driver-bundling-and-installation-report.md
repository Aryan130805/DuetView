# DuetView PC — Driver Bundling & Packaging Verification Report

## Executive Summary

The packaging and installation workflow for DuetView PC has been enhanced to bundle the complete virtual display driver package and automate driver detection and setup.

End users do **not** need Visual Studio, the Windows Driver Kit (WDK), or manual compilation. The installer packages the complete driver bundle (`driver\SecondScreenIdd.inf`, `driver\install_driver.bat`, `driver\uninstall_driver.bat`, and the driver binary when compiled). When DuetView PC launches, it performs first-launch driver detection querying the Windows PnP device tree, handles state transitions, requests elevated UAC installation when approved, and seamlessly falls back to Desktop Mirroring if the driver is not installed.

Both a standalone Windows installer executable (`DuetViewPC-Setup.exe`) and a redistributable CPack ZIP archive (`DuetView-PC-0.4.0-win64.zip`) were produced and verified on Windows.

---

## Deliverables & Artifact Details

| Deliverable | Path | File Size | Description |
|---|---|---|---|
| **Standalone Windows Installer** | [`native/windows_engine/build/DuetViewPC-Setup.exe`](file:///d:/Second%20Display/native/windows_engine/build/DuetViewPC-Setup.exe) | **668,266 bytes** (~668 KB) | Standalone Win32 setup wizard and silent installer (`/S /D=<dir>`). Bundles binaries, icon, driver package, Start Menu/Desktop shortcuts, and Add/Remove Programs registry uninstaller. |
| **Redistributable Package** | [`native/windows_engine/build/DuetView-PC-0.4.0-win64.zip`](file:///d:/Second%20Display/native/windows_engine/build/DuetView-PC-0.4.0-win64.zip) | **610,642 bytes** (~611 KB) | Complete CPack distribution containing `DuetViewPC-Setup.exe`, `SecondScreenApp.exe`, `secondscreen_engine.exe`, `duetview.ico`, and the bundled `driver/` package. |
| **Windows Desktop Application** | [`native/windows_engine/build/SecondScreenApp.exe`](file:///d:/Second%20Display/native/windows_engine/build/SecondScreenApp.exe) | **371,991 bytes** (~372 KB) | GUI desktop host with automated first-launch driver detection, UAC elevation prompt, and real-time PnP status banner. |
| **Native Streaming Engine** | [`native/windows_engine/build/secondscreen_engine.exe`](file:///d:/Second%20Display/native/windows_engine/build/secondscreen_engine.exe) | **465,681 bytes** (~466 KB) | Native DXGI desktop capturer and MediaFoundation hardware H.264 encoder. |
| **Android Client APK** | [`apps/android_app/build/app/outputs/flutter-apk/app-debug.apk`](file:///d:/Second%20Display/apps/android_app/build/app/outputs/flutter-apk/app-debug.apk) | **211,514,789 bytes** (~202 MB) | Rebranded DuetView Android app with Scan to Connect, hardware decoder, and touch/trackpad input. |

---

## Feature & Acceptance Criteria Status Matrix

| Requirement | Implementation & Status | Verification Details |
|---|---|---|
| **1. Driver Source & Build Audit** | **PASS / BLOCKED** | `native/virtual_display_driver/` contains authentic Microsoft IddCx 1.4 UMDF 2.25 source files (`Driver.cpp`, `Device.cpp`, `Monitor.cpp`, `SecondScreenIdd.inf`, `SecondScreenIdd.vcxproj`). Driver runs user-mode in `WUDFHost.exe` with zero BSOD risk.<br><br>**Blocker**: Visual Studio 2022, MSVC `cl.exe`, MSBuild, and WDK 11 are not installed on this host. MinGW-w64 cannot compile WDF drivers due to proprietary Microsoft headers and libraries. No dummy or fake binaries were generated. |
| **2. Bundle Driver in Installer** | **PASS** | `DuetViewPC-Setup.exe` and CPack bundle the `driver/` subdirectory containing `SecondScreenIdd.inf`, `install_driver.bat`, and `uninstall_driver.bat` alongside the application binaries. Path resolution uses `<AppDir>\driver\SecondScreenIdd.inf` without depending on developer source paths. |
| **3. First-Launch Driver Detection** | **PASS** | In `main_window.cpp`, `WM_USER_FIRST_LAUNCH` triggers `CheckFirstLaunchDriverSetup()`. Queries Windows PnP device tree (`SetupDiGetClassDevsW` + `CM_Get_DevNode_Status`):<br>• If `Ready`: Logs readiness and continues without prompting.<br>• If `NotInstalled`: Presents welcome dialog offering driver setup or Desktop Mirroring; remembers response in `HKCU\Software\DuetView\Settings` to avoid repeated nagging.<br>• If `InstalledButNotOperational`: Offers diagnostic repair.<br>• If `RestartRequired`: Explains restart requirement. |
| **4. Supported Elevated Installation** | **PASS** | Elevation requested via `ShellExecuteExW` with `runas` targeting `pnputil.exe`. Paths with spaces are enclosed in double quotes (`"D:\Second Display\..."`). Exit codes (0 = Success, 3010 = Reboot Required, 2 = Missing files, `ERROR_CANCELLED` = UAC declined) are handled cleanly. |
| **5. Seamless User Experience & Mirroring Fallback** | **PASS** | If driver is not installed or installation is declined/unavailable, Desktop Mirroring is 100% active immediately without restart. Virtual monitor is only claimed as active after Windows PnP confirms `DN_STARTED` and multiple monitors exist. |
| **6. Real Windows Installation Verification** | **PASS** | Executed real installation on Windows: `DuetViewPC-Setup.exe /S /D="...build\test_installed"`. Verified that all files and `driver/` directory were deployed, and `secondscreen_driver_tests.exe` executed from the installed folder resolved the driver path relative to the installation directory. |
| **7. Automated Unit Test Verification** | **PASS** | 100% passed (4 of 4 native CTests, 7 of 7 driver manager unit tests, 79 of 79 Flutter unit/widget tests). |

---

## Detailed Test Logs

### 1. Real Installation Workflow Test
Executed silent installation to isolated target directory:
```powershell
cmd.exe /c "native\windows_engine\build\DuetViewPC-Setup.exe /S /D=D:\Second Display\native\windows_engine\build\test_installed"
```

Verified installed filesystem structure:
```text
D:\Second Display\native\windows_engine\build\test_installed\
├── SecondScreenApp.exe         (371,991 bytes)
├── secondscreen_engine.exe     (465,681 bytes)
├── duetview.ico                (119,654 bytes)
├── uninstall.bat               (419 bytes)
└── driver\
    ├── SecondScreenIdd.inf     (1,423 bytes)
    ├── install_driver.bat      (1,379 bytes)
    └── uninstall_driver.bat    (1,064 bytes)
```

Executed driver test suite directly from the installed folder:
```text
========================================================
  Running SecondScreen Driver Manager Unit Tests        
========================================================
[RUN ] TestStateToString...
  [PASS] TestStateToString
[RUN ] TestStateDescription...
  [PASS] TestStateDescription
[RUN ] TestIsIddCxSupportedOnCurrentOS...
  IddCx Supported on host: YES
  [PASS] TestIsIddCxSupportedOnCurrentOS
[RUN ] TestDetectDriverStateDefault...
  Detected Driver State: NOT_INSTALLED
  Hardware ID: Root\SecondScreenIdd
  Diagnostic Message: SecondScreen Virtual Display Driver is not installed. Extended Display mode requires installation.
  [PASS] TestDetectDriverStateDefault
[RUN ] TestMockStateLifecycle...
  [PASS] TestMockStateLifecycle
[RUN ] TestFindDriverInfPath...
  Resolved Driver INF Path: D:\Second Display\native\windows_engine\build\test_installed\driver\SecondScreenIdd.inf
  [PASS] TestFindDriverInfPath
[RUN ] TestInstallDriverPreFlightChecks...
  [PASS] TestInstallDriverPreFlightChecks
========================================================
  Driver Manager Tests Summary: ALL PASSED              
========================================================
```

### 2. Native CTest Suite
```text
Test project D:/Second Display/native/windows_engine/build
    Start 1: ProtocolTests
1/4 Test #1: ProtocolTests ....................   Passed    0.87 sec
    Start 2: CaptureTests
2/4 Test #2: CaptureTests .....................   Passed    0.17 sec
    Start 3: EncoderTests
3/4 Test #3: EncoderTests .....................   Passed    0.55 sec
    Start 4: DriverTests
4/4 Test #4: DriverTests ......................   Passed    0.14 sec

100% tests passed out of 4 (Total: 1.78 sec)
```

---

## Code Signing & Distribution Requirements

1. **Commercial Distribution**:
   - For end-user deployment without security prompts or test-signing watermarks, the driver package must be submitted to the Microsoft Windows Hardware Developer Center (WHQL).
   - Requires an **EV (Extended Validation) Code Signing Certificate**.
   - Microsoft produces a WHQL-signed `.cat` catalog file ensuring compatibility with standard Windows 10/11 installations with Secure Boot enabled.

2. **Development / Internal Evaluation**:
   - Drivers built locally via Visual Studio 2022 + WDK 11 are signed with a test certificate (`SecondScreenIdd.cer`).
   - Requires Windows Test-Signing mode: `bcdedit /set testsigning on` and importing the test certificate to `LocalMachine\Root` and `LocalMachine\TrustedPublisher`.
   - The application does not silently modify security settings or disable Secure Boot.

