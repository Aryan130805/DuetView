#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace secondscreen::driver {

/// Authoritative driver lifecycle states
enum class DriverState {
    NotInstalled,                    // State 1: Driver not installed
    InstalledButNotOperational,      // State 3: Driver installed but device not ready / problem code
    InstalledButMonitorNotAvailable, // State 2: Driver installed & operational, but monitor not extended
    Ready,                           // State 2: Driver installed and functioning
    UpdateAvailable,                 // State 4: Driver update available
    DetectionFailed,                 // State 5: Detection failed / query error
    InstallationInProgress,
    RestartRequired,
    InstallationFailed,
    UnsupportedOrBlocked
};

/// Comprehensive driver diagnostic information
struct DriverInfo {
    DriverState state{DriverState::NotInstalled};
    std::wstring hardwareId{L"Root\\SecondScreenIdd"};
    std::wstring deviceDesc{};
    std::wstring installedVersion{};
    std::wstring bundledVersion{};
    uint32_t devNodeStatus{0};
    uint32_t problemCode{0};
    std::string diagnosticMessage{};
};

/// Dedicated manager for virtual display driver detection, installation, and lifecycle
class DriverManager {
public:
    DriverManager();
    ~DriverManager();

    DriverManager(const DriverManager&) = delete;
    DriverManager& operator=(const DriverManager&) = delete;

    /// Authoritative driver detection querying Windows PnP device tree
    [[nodiscard]] DriverInfo DetectDriverState();

    /// Returns human-readable label for a DriverState
    [[nodiscard]] static std::string StateToString(DriverState state);

    /// Returns true if current Windows OS version supports IddCx (Windows 10 1809 / Build 17763+)
    [[nodiscard]] static bool IsIddCxSupportedOnCurrentOS();

    /// Locates the driver package INF file in known build/project relative directories
    [[nodiscard]] std::wstring FindDriverInfPath() const;

    /// Launches the elevated installer via ShellExecuteExW with the 'runas' verb
    /// Returns true if installation succeeded and driver is verified
    bool InstallDriver(uint32_t& outExitCode, std::string& outErrorMessage);

    /// Launches elevated uninstallation to remove driver and device node
    bool UninstallDriver(uint32_t& outExitCode, std::string& outErrorMessage);

    /// Launches elevated driver repair (clean uninstallation followed by re-installation)
    bool RepairDriver(uint32_t& outExitCode, std::string& outErrorMessage);

    /// Checks and updates driver files and PnP device state
    bool UpdateDriver(uint32_t& outExitCode, std::string& outErrorMessage);

    /// Formats a diagnostic explanation for a given driver state
    [[nodiscard]] static std::string GetStateDescription(DriverState state, uint32_t problemCode = 0);

    /// Testing helper to mock/override state in unit tests without requiring admin rights
    void SetMockState(std::optional<DriverState> mockState);

    /// Returns the currently active or mocked state
    [[nodiscard]] std::optional<DriverState> GetMockState() const noexcept;

private:
    std::optional<DriverState> mockState_;
};

} // namespace secondscreen::driver

