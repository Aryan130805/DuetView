# Virtual Display Driver Build & Environment Audit

## 1. Architectural & Implementation Audit

### 1.1 Driver Model & Framework
The DuetView virtual display driver is implemented as a **Windows Indirect Display Driver (IddCx)** built on the **User-Mode Driver Framework (UMDF 2.25)**. 

Unlike legacy kernel-mode display drivers (KMD/WDDM miniports), an IddCx driver runs in user space inside the Windows User-Mode Driver Framework Host process (`WUDFHost.exe`) under the `NT AUTHORITY\LocalService` account:
- **Zero Kernel BSOD Risk**: A crash or unhandled exception within the driver only terminates `WUDFHost.exe`, which Windows automatically restarts without blue-screening or destabilizing the operating system.
- **DWM Integration**: The driver creates an indirect display adapter. Desktop Window Manager (DWM) recognizes this adapter as a legitimate physical display (`\\.\DISPLAY2` or higher) and renders the virtual extended desktop directly to it via DirectX 11 swapchains.
- **Decoupled Architecture**: Frame capture is handled by the streaming host engine (`secondscreen_engine.exe`) using standard DXGI Desktop Duplication (`IDXGIOutputDuplication`). This cleanly decouples display surface creation from network transport and video compression.

---

### 1.2 Driver Source Files & Roles
The driver codebase is located in [`native/virtual_display_driver/`](file:///d:/Second%20Display/native/virtual_display_driver/):

| File | Type | Functional Role |
|------|------|-----------------|
| [`Driver.h`](file:///d:/Second%20Display/native/virtual_display_driver/Driver.h) / [`Driver.cpp`](file:///d:/Second%20Display/native/virtual_display_driver/Driver.cpp) | C++ Source | UMDF 2.x driver lifecycle. Defines `DriverEntry` and `EvtDriverDeviceAdd`. Initializes `IDD_CX_CLIENT_CONFIG` and configures WDF device context callbacks. |
| [`Device.h`](file:///d:/Second%20Display/native/virtual_display_driver/Device.h) / [`Device.cpp`](file:///d:/Second%20Display/native/virtual_display_driver/Device.cpp) | C++ Source | Adapter lifecycle and power management. Implements `EvtDeviceD0Entry` and `EvtDeviceD0Exit`. Calls `IddCxAdapterInitAsync` and triggers virtual monitor arrival. |
| [`Monitor.h`](file:///d:/Second%20Display/native/virtual_display_driver/Monitor.h) / [`Monitor.cpp`](file:///d:/Second%20Display/native/virtual_display_driver/Monitor.cpp) | C++ Source | Synthetic monitor provisioning. Injects a CEA-861 compliant EDID identifying the virtual monitor (`"SCS"`), defines preferred display modes (1920×1080 @ 60Hz and 1280×720 @ 60Hz), calls `IddCxMonitorCreate`, and manages the frame swapchain worker thread. |
| [`Trace.h`](file:///d:/Second%20Display/native/virtual_display_driver/Trace.h) | C++ Header | Windows Software Trace Preprocessor (WPP) macros for low-overhead logging during driver development and diagnostics. |
| [`SecondScreenIdd.inf`](file:///d:/Second%20Display/native/virtual_display_driver/SecondScreenIdd.inf) | PnP INF | Windows Setup Information file. Classifies device under `Class = Display` (`{4d36e968-e325-11ce-bfc1-08002be10318}`), binds the `WUDFRd.sys` reflector, copies `SecondScreenIdd.dll` to `%12%\UMDF`, and assigns hardware ID `Root\SecondScreenIdd`. |
| [`SecondScreenIdd.vcxproj`](file:///d:/Second%20Display/native/virtual_display_driver/SecondScreenIdd.vcxproj) | MSBuild Project | Visual Studio 2022 project targeting `WindowsUserModeDriver10.0` platform toolset, linking `IddCx.lib` and configured for x64 architecture. |
| [`SecondScreenIdd.sln`](file:///d:/Second%20Display/native/virtual_display_driver/SecondScreenIdd.sln) | VS Solution | Visual Studio solution wrapper for automated command-line builds. |
| [`scripts/install_driver.bat`](file:///d:/Second%20Display/native/virtual_display_driver/scripts/install_driver.bat) | Batch Script | Deployment utility requesting administrative privileges and executing `pnputil /add-driver SecondScreenIdd.inf /install`. |
| [`scripts/uninstall_driver.bat`](file:///d:/Second%20Display/native/virtual_display_driver/scripts/uninstall_driver.bat) | Batch Script | Teardown utility removing the virtual display device node and deleting the driver package from the Windows Driver Store. |

---

## 2. Environment Audit

### 2.1 Audit Results
A systematic audit of the host compilation environment was conducted:

| Tool / Dependency | Expected Path / Command | Detected Status | Notes |
|-------------------|-------------------------|-----------------|-------|
| **Visual Studio 2022** | `C:\Program Files\Microsoft Visual Studio\2022\*` | **NOT INSTALLED** | No VS2022 installation found on C:, D:, or E: |
| **MSBuild** | `msbuild.exe` on PATH | **NOT INSTALLED** | Command unavailable |
| **MSVC Compiler** | `cl.exe` (v143 toolset) | **NOT INSTALLED** | Command unavailable |
| **Windows SDK** | `C:\Program Files (x86)\Windows Kits\10` | **NOT INSTALLED** | Directory absent |
| **Windows Driver Kit (WDK 11)** | WDK Visual Studio extension & headers | **NOT INSTALLED** | Directory absent |
| **MinGW-w64 GCC** | `gcc.exe` (v15.2.0) | **PRESENT** | Used for user-mode desktop application & engine |
| **CMake & Ninja** | `cmake.exe`, `ninja.exe` | **PRESENT** | Active build system for Windows engine and unit tests |

### 2.2 Why MinGW-w64 Cannot Compile IddCx Drivers
While the DuetView PC user-mode desktop application (`SecondScreenApp.exe`) and native streaming engine (`secondscreen_engine.exe`) build seamlessly using MinGW-w64 and GCC, the virtual display driver binary `SecondScreenIdd.dll` **cannot** be compiled with MinGW-w64:
1. **Proprietary Microsoft WDK Headers**: The IddCx API headers (`iddcx.h`, `wdf.h`, `wdfdevice.h`, `wdfdriver.h`) are proprietary to the Microsoft Windows Driver Kit. They rely heavily on MSVC-specific annotations (`_IRQL_requires_max_`, `_Function_class_`), compiler intrinsics, and SAL (Source Code Annotation Language).
2. **Proprietary Class Extension Libraries**: Linking against `IddCx.lib` requires Microsoft's COFF-format driver import library supplied exclusively with the WDK.
3. **Platform Toolset Requirement**: Visual Studio uses the specialized `WindowsUserModeDriver10.0` build rules and property sheets (`$(VCTargetsPath)\Microsoft.Cpp.targets`) to inject UMDF runtime stubs and metadata into the resulting dynamic link library.

### 2.3 Prerequisites to Compile Driver on a Developer Machine
To compile `SecondScreenIdd.dll` on Windows:
1. Install **Visual Studio 2022** (Community, Professional, or Enterprise) with the **"Desktop development with C++"** workload.
2. Install the **Windows 11 SDK** (Version 10.0.22621.0 or higher).
3. Install the **Windows Driver Kit (WDK) 11** matching the installed Windows SDK version.
4. Execute:
   ```cmd
   cd /d "native\virtual_display_driver"
   msbuild SecondScreenIdd.sln /p:Configuration=Release /p:Platform=x64
   ```

---

## 3. Packaging & Driver Staging Architecture

### 3.1 Relocatable Driver Packaging
In a production deployment, the desktop application should not rely on hardcoded developer repository paths (`d:\Second Display\native\...`).

The project now implements an automated driver staging mechanism:
- A dedicated `driver/` staging subdirectory is generated at `${CMAKE_BINARY_DIR}/driver/` alongside `SecondScreenApp.exe`.
- CMake automatically copies `SecondScreenIdd.inf`, `install_driver.bat`, and `uninstall_driver.bat` into this directory during every build via the `stage_driver_package` target.
- When `SecondScreenIdd.dll` is compiled, placing it in `<AppDir>\driver\` creates a fully self-contained, redistributable driver package.

### 3.2 Dynamic Path Resolution Hierarchy
`DriverManager::FindDriverInfPath()` resolves the driver package using the following precedence:
1. `<ExecutableDirectory>\driver\SecondScreenIdd.inf` (Packaged distribution directory)
2. `<ExecutableDirectory>\SecondScreenIdd.inf` (Flat portable distribution)
3. `<ExecutableDirectory>\..\driver\SecondScreenIdd.inf` (Staged relative build path)
4. `<ExecutableDirectory>\..\..\native\virtual_display_driver\SecondScreenIdd.inf` (Developer build out of `build/`)
5. `<ExecutableDirectory>\native\virtual_display_driver\SecondScreenIdd.inf` (Child developer path)
6. Workspace root fallback (`d:\Second Display\native\virtual_display_driver\SecondScreenIdd.inf`)

### 3.3 Safe Space-in-Path Handling
Because paths such as `D:\Second Display\` contain spaces, all driver invocation strings passed to `PnPUtil` are explicitly quoted:
- Installation: `pnputil.exe /add-driver "<infPath>" /install`
- Uninstallation: `pnputil.exe /delete-driver "<infFilename>" /uninstall /force`
- Working Directory: Explicitly passed as `driverDir` in `SHELLEXECUTEINFOW::lpDirectory`.

---

## 4. Operational Fallback: Desktop Mirroring
The DuetView PC architecture strictly separates **Virtual Extended Display** (which requires the IddCx driver) from **Desktop Mirroring** (which does not):
- Desktop Mirroring uses DXGI Desktop Duplication on the primary display (`\\.\DISPLAY1`).
- Mirroring, hardware H.264 encoding, network transport, QR pairing, and Android touch/trackpad input are **100% operational** regardless of driver installation status.
- When the driver binary is absent, DuetView PC informs the user clearly and guides them to use Desktop Mirroring immediately.

