#include "driver/driver_manager.hpp"

#include <cassert>
#include <iostream>
#include <string>

using secondscreen::driver::DriverManager;
using secondscreen::driver::DriverState;
using secondscreen::driver::DriverInfo;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
        std::exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "Assertion failed at line " << __LINE__ << ": " #a " != " #b "\n"; \
        std::exit(1); \
    } \
} while(0)

void TestStateToString() {
    std::cout << "[RUN ] TestStateToString...\n";
    ASSERT_EQ(DriverManager::StateToString(DriverState::NotInstalled), "NOT_INSTALLED");
    ASSERT_EQ(DriverManager::StateToString(DriverState::InstalledButNotOperational), "INSTALLED_BUT_NOT_OPERATIONAL");
    ASSERT_EQ(DriverManager::StateToString(DriverState::InstalledButMonitorNotAvailable), "INSTALLED_BUT_MONITOR_NOT_AVAILABLE");
    ASSERT_EQ(DriverManager::StateToString(DriverState::Ready), "READY");
    ASSERT_EQ(DriverManager::StateToString(DriverState::UpdateAvailable), "UPDATE_AVAILABLE");
    ASSERT_EQ(DriverManager::StateToString(DriverState::DetectionFailed), "DETECTION_FAILED");
    ASSERT_EQ(DriverManager::StateToString(DriverState::InstallationInProgress), "INSTALLATION_IN_PROGRESS");
    ASSERT_EQ(DriverManager::StateToString(DriverState::RestartRequired), "RESTART_REQUIRED");
    ASSERT_EQ(DriverManager::StateToString(DriverState::InstallationFailed), "INSTALLATION_FAILED");
    ASSERT_EQ(DriverManager::StateToString(DriverState::UnsupportedOrBlocked), "UNSUPPORTED_OR_BLOCKED");
    std::cout << "  [PASS] TestStateToString\n";
}

void TestStateDescription() {
    std::cout << "[RUN ] TestStateDescription...\n";
    std::string notInstalledDesc = DriverManager::GetStateDescription(DriverState::NotInstalled);
    ASSERT_TRUE(notInstalledDesc.find("not installed") != std::string::npos);

    std::string problemDesc = DriverManager::GetStateDescription(DriverState::InstalledButNotOperational, 43);
    ASSERT_TRUE(problemDesc.find("Code 43") != std::string::npos);

    std::string readyDesc = DriverManager::GetStateDescription(DriverState::Ready);
    ASSERT_TRUE(readyDesc.find("operational") != std::string::npos);

    std::string restartDesc = DriverManager::GetStateDescription(DriverState::RestartRequired);
    ASSERT_TRUE(restartDesc.find("restart") != std::string::npos);
    std::cout << "  [PASS] TestStateDescription\n";
}

void TestIsIddCxSupportedOnCurrentOS() {
    std::cout << "[RUN ] TestIsIddCxSupportedOnCurrentOS...\n";
    // Modern Windows 10/11 should return true
    bool supported = DriverManager::IsIddCxSupportedOnCurrentOS();
    std::cout << "  IddCx Supported on host: " << (supported ? "YES" : "NO") << "\n";
    ASSERT_TRUE(supported);
    std::cout << "  [PASS] TestIsIddCxSupportedOnCurrentOS\n";
}

void TestDetectDriverStateDefault() {
    std::cout << "[RUN ] TestDetectDriverStateDefault...\n";
    DriverManager mgr;
    DriverInfo info = mgr.DetectDriverState();

    std::cout << "  Detected Driver State: " << DriverManager::StateToString(info.state) << "\n";
    std::cout << "  Hardware ID: " << std::string(info.hardwareId.begin(), info.hardwareId.end()) << "\n";
    std::cout << "  Diagnostic Message: " << info.diagnosticMessage << "\n";

    // Valid state must be one of the known authoritative enums
    ASSERT_TRUE(info.state == DriverState::NotInstalled ||
                info.state == DriverState::InstalledButNotOperational ||
                info.state == DriverState::InstalledButMonitorNotAvailable ||
                info.state == DriverState::Ready ||
                info.state == DriverState::UpdateAvailable ||
                info.state == DriverState::DetectionFailed ||
                info.state == DriverState::UnsupportedOrBlocked);
    std::cout << "  [PASS] TestDetectDriverStateDefault\n";
}

void TestMockStateLifecycle() {
    std::cout << "[RUN ] TestMockStateLifecycle...\n";
    DriverManager mgr;

    // Default mock is nullopt
    ASSERT_TRUE(!mgr.GetMockState().has_value());

    // 1. Simulate NotInstalled
    mgr.SetMockState(DriverState::NotInstalled);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::NotInstalled);

    // 2. Simulate InstallationInProgress
    mgr.SetMockState(DriverState::InstallationInProgress);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::InstallationInProgress);

    // 3. Simulate RestartRequired
    mgr.SetMockState(DriverState::RestartRequired);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::RestartRequired);

    // 4. Simulate InstalledButMonitorNotAvailable (Needs Win + P Extend)
    mgr.SetMockState(DriverState::InstalledButMonitorNotAvailable);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::InstalledButMonitorNotAvailable);

    // 5. Simulate Ready
    mgr.SetMockState(DriverState::Ready);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::Ready);

    // Reset mock
    mgr.SetMockState(std::nullopt);
    ASSERT_TRUE(!mgr.GetMockState().has_value());
    std::cout << "  [PASS] TestMockStateLifecycle\n";
}

void TestFindDriverInfPath() {
    std::cout << "[RUN ] TestFindDriverInfPath...\n";
    DriverManager mgr;
    std::wstring infPath = mgr.FindDriverInfPath();
    std::string sPath(infPath.begin(), infPath.end());
    std::cout << "  Resolved Driver INF Path: " << sPath << "\n";
    ASSERT_TRUE(!infPath.empty());
    ASSERT_TRUE(sPath.find("SecondScreenIdd.inf") != std::string::npos);
    std::cout << "  [PASS] TestFindDriverInfPath\n";
}

void TestInstallDriverPreFlightChecks() {
    std::cout << "[RUN ] TestInstallDriverPreFlightChecks...\n";
    DriverManager mgr;

    std::wstring infPath = mgr.FindDriverInfPath();
    ASSERT_TRUE(!infPath.empty());
    ASSERT_TRUE(infPath.find(L"SecondScreenIdd.inf") != std::wstring::npos);

    // Preflight checks: OS must support IddCx
    ASSERT_TRUE(DriverManager::IsIddCxSupportedOnCurrentOS());

    // Verify mock driver installation handling
    mgr.SetMockState(DriverState::InstallationInProgress);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::InstallationInProgress);
    mgr.SetMockState(DriverState::Ready);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::Ready);
    mgr.SetMockState(DriverState::UpdateAvailable);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::UpdateAvailable);
    mgr.SetMockState(DriverState::DetectionFailed);
    ASSERT_EQ(mgr.DetectDriverState().state, DriverState::DetectionFailed);
    mgr.SetMockState(std::nullopt);

    std::cout << "  [PASS] TestInstallDriverPreFlightChecks\n";
}

int main() {
    std::cout << "========================================================\n"
              << "  Running SecondScreen Driver Manager Unit Tests        \n"
              << "========================================================\n";

    TestStateToString();
    TestStateDescription();
    TestIsIddCxSupportedOnCurrentOS();
    TestDetectDriverStateDefault();
    TestMockStateLifecycle();
    TestFindDriverInfPath();
    TestInstallDriverPreFlightChecks();

    std::cout << "========================================================\n"
              << "  Driver Manager Tests Summary: ALL PASSED              \n"
              << "========================================================\n";
    return 0;
}

