#include "driver/driver_manager.hpp"

#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace secondscreen::driver {

namespace {

// Display device class GUID: {4d36e968-e325-11ce-bfc1-08002be10318}
static const GUID GUID_DISPLAY_CLASS = {
    0x4d36e968, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 }
};

bool StringContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](wchar_t ch1, wchar_t ch2) {
            return std::tolower(ch1) == std::tolower(ch2);
        }
    );
    return (it != haystack.end());
}

} // namespace

DriverManager::DriverManager() = default;
DriverManager::~DriverManager() = default;

void DriverManager::SetMockState(std::optional<DriverState> mockState) {
    mockState_ = mockState;
}

std::optional<DriverState> DriverManager::GetMockState() const noexcept {
    return mockState_;
}

std::string DriverManager::StateToString(DriverState state) {
    switch (state) {
    case DriverState::NotInstalled:
        return "NOT_INSTALLED";
    case DriverState::InstalledButNotOperational:
        return "INSTALLED_BUT_NOT_OPERATIONAL";
    case DriverState::InstalledButMonitorNotAvailable:
        return "INSTALLED_BUT_MONITOR_NOT_AVAILABLE";
    case DriverState::Ready:
        return "READY";
    case DriverState::InstallationInProgress:
        return "INSTALLATION_IN_PROGRESS";
    case DriverState::RestartRequired:
        return "RESTART_REQUIRED";
    case DriverState::InstallationFailed:
        return "INSTALLATION_FAILED";
    case DriverState::UnsupportedOrBlocked:
        return "UNSUPPORTED_OR_BLOCKED";
    }
    return "UNKNOWN";
}

std::string DriverManager::GetStateDescription(DriverState state, uint32_t problemCode) {
    switch (state) {
    case DriverState::NotInstalled:
        return "DuetView Virtual Display Driver is not installed. Extended Display mode requires installation.";
    case DriverState::InstalledButNotOperational: {
        std::ostringstream ss;
        ss << "Driver is installed but reported device error (Code " << problemCode
           << "). Open Device Manager to verify device status.";
        return ss.str();
    }
    case DriverState::InstalledButMonitorNotAvailable:
        return "Driver is active, but virtual display is not attached. Press Win + P and choose 'Extend'.";
    case DriverState::Ready:
        return "Driver is operational and virtual display is ready for streaming.";
    case DriverState::InstallationInProgress:
        return "Installation in progress...";
    case DriverState::RestartRequired:
        return "Driver installation succeeded, but Windows requires a system restart before the device becomes operational.";
    case DriverState::InstallationFailed:
        return "Driver installation failed. Ensure administrator approval was granted and test-signing is configured if using a self-signed build.";
    case DriverState::UnsupportedOrBlocked:
        return "Indirect Display Driver (IddCx) is not supported on this Windows version (requires Windows 10 1809+).";
    }
    return "";
}

bool DriverManager::IsIddCxSupportedOnCurrentOS() {
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    // Dynamic query of RtlGetVersion to avoid application manifest version lie
    typedef LONG(NTAPI* PFN_RtlGetVersion)(PRTL_OSVERSIONINFOEXW);
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll) {
        FARPROC proc = GetProcAddress(hNtDll, "RtlGetVersion");
        if (proc) {
            auto pRtlGetVersion = reinterpret_cast<PFN_RtlGetVersion>(reinterpret_cast<void*>(proc));
            if (pRtlGetVersion && pRtlGetVersion(&osvi) == 0) {
                // IddCx 1.4 requires Windows 10 Build 17763 (Version 1809) or higher
                if (osvi.dwMajorVersion > 10) return true;
                if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 17763) return true;
                return false;
            }
        }
    }
    return true; // Fallback assume supported on modern NT
}

DriverInfo DriverManager::DetectDriverState() {
    if (mockState_.has_value()) {
        DriverInfo mockInfo;
        mockInfo.state = *mockState_;
        mockInfo.diagnosticMessage = GetStateDescription(mockInfo.state, 0);
        return mockInfo;
    }

    DriverInfo info{};
    info.hardwareId = L"Root\\SecondScreenIdd";

    if (!IsIddCxSupportedOnCurrentOS()) {
        info.state = DriverState::UnsupportedOrBlocked;
        info.diagnosticMessage = GetStateDescription(info.state, 0);
        return info;
    }

    // Query PnP Display Class devices
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&GUID_DISPLAY_CLASS, nullptr, nullptr, 0);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        info.state = DriverState::NotInstalled;
        info.diagnosticMessage = "Unable to enumerate Windows display devices.";
        return info;
    }

    bool foundDevice = false;
    SP_DEVINFO_DATA devInfoData{};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        // Query Hardware ID property
        wchar_t hwIdBuffer[512] = {0};
        DWORD reqSize = 0;
        if (SetupDiGetDeviceRegistryPropertyW(
                hDevInfo, &devInfoData, SPDRP_HARDWAREID,
                nullptr, reinterpret_cast<PBYTE>(hwIdBuffer), sizeof(hwIdBuffer), &reqSize)) {

            // Hardware ID is MULTI_SZ (multiple null-terminated strings)
            for (const wchar_t* p = hwIdBuffer; *p; p += wcslen(p) + 1) {
                if (StringContainsCaseInsensitive(p, L"Root\\SecondScreenIdd") ||
                    StringContainsCaseInsensitive(p, L"SecondScreenIdd")) {
                    foundDevice = true;
                    break;
                }
            }
        }

        // Also check device description
        wchar_t descBuffer[256] = {0};
        if (SetupDiGetDeviceRegistryPropertyW(
                hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
                nullptr, reinterpret_cast<PBYTE>(descBuffer), sizeof(descBuffer), nullptr)) {
            if (StringContainsCaseInsensitive(descBuffer, L"SecondScreen")) {
                foundDevice = true;
                info.deviceDesc = descBuffer;
            }
        }

        if (foundDevice) {
            // Check DevNode status via Configuration Manager
            ULONG status = 0;
            ULONG problemNumber = 0;
            CONFIGRET cr = CM_Get_DevNode_Status(&status, &problemNumber, devInfoData.DevInst, 0);

            info.devNodeStatus = status;
            info.problemCode = problemNumber;

            if (cr == CR_SUCCESS) {
                if (status & DN_HAS_PROBLEM) {
                    info.state = DriverState::InstalledButNotOperational;
                    info.diagnosticMessage = GetStateDescription(info.state, problemNumber);
                } else if (status & DN_STARTED) {
                    // Device is started. Now check if virtual display output is active.
                    int monitorCount = GetSystemMetrics(SM_CMONITORS);
                    if (monitorCount > 1) {
                        info.state = DriverState::Ready;
                        info.diagnosticMessage = GetStateDescription(info.state, 0);
                    } else {
                        info.state = DriverState::InstalledButMonitorNotAvailable;
                        info.diagnosticMessage = GetStateDescription(info.state, 0);
                    }
                } else {
                    info.state = DriverState::InstalledButNotOperational;
                    info.diagnosticMessage = "Device node is present but not started by driver host.";
                }
            } else {
                info.state = DriverState::InstalledButNotOperational;
                info.diagnosticMessage = "Unable to query device node configuration status.";
            }
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (!foundDevice) {
        info.state = DriverState::NotInstalled;
        info.diagnosticMessage = GetStateDescription(info.state, 0);
    }

    return info;
}

std::wstring DriverManager::FindDriverInfPath() const {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    // Candidates in order of precedence:
    // 1. Packaged driver subdirectory alongside executable: <exePath>\driver\SecondScreenIdd.inf
    std::wstring p1 = std::wstring(exePath) + L"\\driver\\SecondScreenIdd.inf";
    if (PathFileExistsW(p1.c_str())) return p1;

    // 2. Flat packaging in same directory as executable: <exePath>\SecondScreenIdd.inf
    std::wstring p2 = std::wstring(exePath) + L"\\SecondScreenIdd.inf";
    if (PathFileExistsW(p2.c_str())) return p2;

    // 3. Staged driver directory relative to parent: <exePath>\..\driver\SecondScreenIdd.inf
    std::wstring p3 = std::wstring(exePath) + L"\\..\\driver\\SecondScreenIdd.inf";
    if (PathFileExistsW(p3.c_str())) return p3;

    // 4. Source tree relative path from build output: <exePath>\..\..\native\virtual_display_driver\SecondScreenIdd.inf
    std::wstring p4 = std::wstring(exePath) + L"\\..\\..\\native\\virtual_display_driver\\SecondScreenIdd.inf";
    if (PathFileExistsW(p4.c_str())) return p4;

    // 5. Child virtual_display_driver directory
    std::wstring p5 = std::wstring(exePath) + L"\\native\\virtual_display_driver\\SecondScreenIdd.inf";
    if (PathFileExistsW(p5.c_str())) return p5;

    // 6. Absolute workspace fallback
    std::wstring p6 = L"d:\\Second Display\\native\\virtual_display_driver\\SecondScreenIdd.inf";
    if (PathFileExistsW(p6.c_str())) return p6;

    return L"SecondScreenIdd.inf";
}

bool DriverManager::InstallDriver(uint32_t& outExitCode, std::string& outErrorMessage) {
    outExitCode = 1;
    outErrorMessage.clear();

    if (!IsIddCxSupportedOnCurrentOS()) {
        outErrorMessage = "Indirect Display Driver (IddCx) is not supported on this Windows version (requires Windows 10 1809+ / Build 17763+).";
        return false;
    }

    const std::wstring infPath = FindDriverInfPath();
    if (!PathFileExistsW(infPath.c_str())) {
        outErrorMessage = "Driver package INF file not found (" +
                          std::string(infPath.begin(), infPath.end()) +
                          "). Please build or stage the driver package first.";
        return false;
    }

    // Verify driver DLL binary exists alongside the INF
    wchar_t driverDir[MAX_PATH] = {0};
    wcscpy_s(driverDir, infPath.c_str());
    PathRemoveFileSpecW(driverDir);
    std::wstring dllPath = std::wstring(driverDir) + L"\\SecondScreenIdd.dll";

    if (!PathFileExistsW(dllPath.c_str())) {
        outErrorMessage = "Driver binary 'SecondScreenIdd.dll' not found in:\n" +
                          std::string(driverDir, driverDir + wcslen(driverDir)) +
                          "\n\nThe driver source files and INF are present, but the binary has not been compiled yet.\n" +
                          "Compiling an IddCx driver requires Visual Studio 2022 with the Windows Driver Kit (WDK) 11:\n" +
                          "  msbuild native\\virtual_display_driver\\SecondScreenIdd.sln /p:Configuration=Release /p:Platform=x64\n\n" +
                          "Note: Desktop Mirroring is fully functional right now without any driver! You can click 'Start Host Service' to stream your screen immediately.";
        return false;
    }

    // Launch elevated installer via ShellExecuteExW with "runas" requesting Administrator permission
    std::wstring batPath = std::wstring(driverDir) + L"\\install_driver.bat";
    bool hasBat = (PathFileExistsW(batPath.c_str()) != FALSE);

    std::wstring fileToExec;
    std::wstring execParams;

    if (hasBat) {
        fileToExec = L"cmd.exe";
        execParams = L"/c \"\"" + batPath + L"\"\"";
    } else {
        fileToExec = L"pnputil.exe";
        execParams = L"/add-driver \"" + infPath + L"\" /install";
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = L"runas"; // Requests standard Windows Administrator (UAC) elevation
    sei.lpFile = fileToExec.c_str();
    sei.lpParameters = execParams.c_str();
    sei.lpDirectory = driverDir;
    sei.nShow = hasBat ? SW_SHOWNORMAL : SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            outErrorMessage = "Driver installation was canceled by the user (Administrator permission was not granted).";
        } else {
            outErrorMessage = "Failed to launch elevated installer (Error " + std::to_string(err) + ").";
        }
        return false;
    }

    if (sei.hProcess != NULL) {
        WaitForSingleObject(sei.hProcess, 60000); // 60s timeout
        DWORD exitCode = 0;
        if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
            outExitCode = exitCode;
            CloseHandle(sei.hProcess);

            // Re-detect driver state to verify success
            auto postInfo = DetectDriverState();
            if (postInfo.state == DriverState::Ready ||
                postInfo.state == DriverState::InstalledButMonitorNotAvailable) {
                return true;
            } else if (postInfo.state == DriverState::RestartRequired) {
                outExitCode = 3010;
                return true;
            } else if (exitCode == 0) {
                return true;
            } else if (exitCode == 3010 /* ERROR_SUCCESS_REBOOT_REQUIRED */) {
                outExitCode = 3010;
                return true;
            } else if (exitCode == 2 /* ERROR_FILE_NOT_FOUND */) {
                outErrorMessage = "PnPUtil reported Error 2 (File Not Found): The driver package or a dependency could not be located.\n\n"
                                  "Ensure SecondScreenIdd.dll is present in the driver directory and test-signing is enabled if self-signed.";
                return false;
            } else {
                outErrorMessage = "PnPUtil exited with error code " + std::to_string(exitCode) +
                                  ". If using a self-signed development driver, test-signing mode (bcdedit /set testsigning on) may be required.";
                return false;
            }
        }
        CloseHandle(sei.hProcess);
    }

    outErrorMessage = "Installer process did not return a valid status.";
    return false;
}

bool DriverManager::UninstallDriver(uint32_t& outExitCode, std::string& outErrorMessage) {
    outExitCode = 1;
    outErrorMessage.clear();

    const std::wstring infPath = FindDriverInfPath();

    wchar_t driverDir[MAX_PATH] = {0};
    wcscpy_s(driverDir, infPath.c_str());
    PathRemoveFileSpecW(driverDir);

    std::wstring uninstBat = std::wstring(driverDir) + L"\\uninstall_driver.bat";
    bool hasBat = (PathFileExistsW(uninstBat.c_str()) != FALSE);

    std::wstring fileToExec;
    std::wstring execParams;

    if (hasBat) {
        fileToExec = L"cmd.exe";
        execParams = L"/c \"\"" + uninstBat + L"\"\"";
    } else {
        wchar_t filename[MAX_PATH] = {0};
        wcscpy_s(filename, infPath.c_str());
        PathStripPathW(filename);
        fileToExec = L"pnputil.exe";
        execParams = L"/delete-driver \"" + std::wstring(filename) + L"\" /uninstall /force";
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = L"runas"; // Requests standard Windows Administrator (UAC) elevation
    sei.lpFile = fileToExec.c_str();
    sei.lpParameters = execParams.c_str();
    sei.lpDirectory = driverDir;
    sei.nShow = hasBat ? SW_SHOWNORMAL : SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            outErrorMessage = "Uninstallation was canceled by the user (Administrator permission was not granted).";
        } else {
            outErrorMessage = "Failed to launch elevated uninstaller (Error " + std::to_string(err) + ").";
        }
        return false;
    }

    if (sei.hProcess != NULL) {
        WaitForSingleObject(sei.hProcess, 30000);
        DWORD exitCode = 0;
        if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
            outExitCode = exitCode;
            CloseHandle(sei.hProcess);
            return (exitCode == 0);
        }
        CloseHandle(sei.hProcess);
    }

    return false;
}

} // namespace secondscreen::driver

