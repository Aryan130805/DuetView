#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "app/process_manager.hpp"
#include "pairing/pairing_manager.hpp"
#include "pairing/qrcodegen.hpp"
#include "driver/driver_manager.hpp"
#include "engine/engine_runner.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <mutex>
#include <algorithm>

// Control IDs
#define IDC_BTN_START_STOP       1001
#define IDC_BTN_REGEN_QR         1002
#define IDC_BTN_COPY_INFO        1003
#define IDC_BTN_TOGGLE_LOGS      1004
#define IDC_BTN_CLEAR_LOGS       1005
#define IDC_BTN_COPY_LOGS        1006
#define IDC_BTN_COPY_PCID        1007
#define IDC_EDIT_PC_NAME         1010
#define IDC_EDIT_PC_ID           1011
#define IDC_EDIT_PORT            1012
#define IDC_COMBO_FPS            1013
#define IDC_COMBO_BITRATE        1014
#define IDC_CHK_CONSOLE          1015
#define IDC_COMBO_MONITOR        1016
#define IDC_BTN_DISPLAY_SETTINGS 1017
#define IDC_BTN_DRIVER_GUIDE     1018
#define IDC_BTN_INSTALL_DRIVER   1021
#define IDC_BTN_UNINSTALL_DRIVER 1022
#define IDC_EDIT_LOGS            1020

#define WM_USER_APPEND_LOG       (WM_USER + 201)
#define WM_USER_FIRST_LAUNCH     (WM_USER + 202)
#define IDT_TICK_TIMER           1

namespace {

using secondscreen::app::ProcessManager;
using secondscreen::app::HostState;
using secondscreen::pairing::PairingManager;
using secondscreen::driver::DriverManager;
using secondscreen::driver::DriverState;
using secondscreen::driver::DriverInfo;

// Navigation Tabs
enum AppTab {
    TAB_HOME = 0,
    TAB_STREAMING = 1,
    TAB_DEVICES = 2,
    TAB_DRIVER = 3,
    TAB_SETTINGS = 4,
    TAB_GUIDE = 5,
    TAB_ABOUT = 6,
};

AppTab g_currentTab = TAB_HOME;

// UI Color Palette (Matching Reference Image 2)
const COLORREF COLOR_BG             = RGB(11, 19, 43);    // #0b132b
const COLORREF COLOR_SIDEBAR_BG     = RGB(8, 15, 32);     // #080f20
const COLORREF COLOR_SIDEBAR_BORDER = RGB(30, 41, 59);    // #1e293b
const COLORREF COLOR_CARD_BG        = RGB(20, 33, 59);    // #14213b
const COLORREF COLOR_CARD_BORDER    = RGB(43, 61, 93);    // #2b3d5d
const COLORREF COLOR_HERO_BG        = RGB(14, 27, 56);    // #0e1b38
const COLORREF COLOR_HERO_BORDER    = RGB(30, 53, 96);    // #1e3560
const COLORREF COLOR_TAB_ACTIVE     = RGB(29, 78, 216);   // #1d4ed8
const COLORREF COLOR_TEXT_MAIN      = RGB(248, 250, 252); // #f8fafc
const COLORREF COLOR_TEXT_MUTED     = RGB(148, 163, 184); // #94a3b8
const COLORREF COLOR_ACCENT         = RGB(66, 217, 255);  // #42d9ff (Cyan Accent)
const COLORREF COLOR_VIOLET         = RGB(121, 87, 255);  // #7957ff (Violet Accent)
const COLORREF COLOR_SUCCESS        = RGB(32, 214, 160);  // #20d6a0 (Green)
const COLORREF COLOR_DANGER         = RGB(255, 93, 112);  // #ff5d70 (Danger/Stop)
const COLORREF COLOR_INPUT_BG       = RGB(11, 20, 38);    // #0b1426

// Application Global State
HWND g_hWnd = NULL;
HWND g_hEditLogs = NULL;
HWND g_hBtnStartStop = NULL;
HWND g_hBtnToggleLogs = NULL;
HWND g_hBtnClearLogs = NULL;
HWND g_hBtnCopyLogs = NULL;
HWND g_hEditPcName = NULL;
HWND g_hEditPcId = NULL;
HWND g_hBtnCopyId = NULL;
HWND g_hEditPort = NULL;
HWND g_hComboFps = NULL;
HWND g_hComboBitrate = NULL;
HWND g_hComboMonitor = NULL;
HWND g_hChkConsole = NULL;
HWND g_hBtnRegen = NULL;
HWND g_hBtnCopyInfo = NULL;
HWND g_hBtnInstallDriver = NULL;
HWND g_hBtnUninstallDriver = NULL;
HWND g_hBtnGuide = NULL;
HWND g_hBtnDisplay = NULL;

HFONT g_hFontTitle = NULL;
HFONT g_hFontSub = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontNormal = NULL;
HFONT g_hFontMono = NULL;

HBRUSH g_hBrushBg = NULL;
HBRUSH g_hBrushSidebarBg = NULL;
HBRUSH g_hBrushCardBg = NULL;
HBRUSH g_hBrushInputBg = NULL;
HICON  g_hAppIcon = NULL;

std::unique_ptr<ProcessManager> g_procMgr;
std::unique_ptr<PairingManager> g_pairingMgr;
std::unique_ptr<DriverManager> g_driverMgr;
DriverInfo g_driverInfo;

std::vector<std::string> g_cachedEndpoints;
bool g_logsExpanded = false;
std::mutex g_logMutex;
std::vector<std::string> g_pendingLogLines;

void RefreshDriverStatus();
void UpdateTabVisibility();

void AppendLogLine(const std::string& line) {
    if (!g_hWnd || !g_hEditLogs) return;

    std::string safeLine = line + "\r\n";
    int wLen = MultiByteToWideChar(CP_UTF8, 0, safeLine.c_str(), -1, nullptr, 0);
    if (wLen <= 0) return;

    std::wstring wStr(wLen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, safeLine.c_str(), -1, wStr.data(), wLen);

    int len = GetWindowTextLengthW(g_hEditLogs);
    SendMessageW(g_hEditLogs, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hEditLogs, EM_REPLACESEL, FALSE, (LPARAM)wStr.c_str());
}

void DrawCard(HDC hdc, const RECT& rc, const wchar_t* title, const wchar_t* subtitle = nullptr) {
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, g_hBrushCardBg);
    HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_CARD_BORDER);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 14, 14);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    if (title && wcslen(title) > 0) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT_MAIN);
        SelectObject(hdc, g_hFontBold);
        RECT rcTitle = { rc.left + 18, rc.top + 14, rc.right - 18, rc.top + 34 };
        DrawTextW(hdc, title, -1, &rcTitle, DT_LEFT | DT_SINGLELINE);

        if (subtitle && wcslen(subtitle) > 0) {
            SetTextColor(hdc, COLOR_TEXT_MUTED);
            SelectObject(hdc, g_hFontNormal);
            RECT rcSub = { rc.left + 18, rc.top + 34, rc.right - 18, rc.top + 52 };
            DrawTextW(hdc, subtitle, -1, &rcSub, DT_LEFT | DT_SINGLELINE);
        }
    }
}

void DrawCyanBrackets(HDC hdc, const RECT& rc) {
    HPEN hCyanPen = CreatePen(PS_SOLID, 3, COLOR_ACCENT);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hCyanPen);
    const int len = 18;
    const int pad = 6;

    // Top-Left
    MoveToEx(hdc, rc.left - pad, rc.top - pad + len, NULL);
    LineTo(hdc, rc.left - pad, rc.top - pad);
    LineTo(hdc, rc.left - pad + len, rc.top - pad);

    // Top-Right
    MoveToEx(hdc, rc.right + pad - len, rc.top - pad, NULL);
    LineTo(hdc, rc.right + pad, rc.top - pad);
    LineTo(hdc, rc.right + pad, rc.top - pad + len);

    // Bottom-Left
    MoveToEx(hdc, rc.left - pad, rc.bottom + pad - len, NULL);
    LineTo(hdc, rc.left - pad, rc.bottom + pad);
    LineTo(hdc, rc.left - pad + len, rc.bottom + pad);

    // Bottom-Right
    MoveToEx(hdc, rc.right + pad - len, rc.bottom + pad, NULL);
    LineTo(hdc, rc.right + pad, rc.bottom + pad);
    LineTo(hdc, rc.right + pad, rc.bottom + pad - len);

    SelectObject(hdc, hOldPen);
    DeleteObject(hCyanPen);
}

void DrawQrCodeBox(HDC hdc, const RECT& rcBox) {
    if (!g_pairingMgr) return;

    const auto payload = g_pairingMgr->GetCurrentPayload();
    const std::string json = payload.ToJson();

    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(json.c_str(), qrcodegen::QrCode::Ecc::LOW);
    const int size = qr.getSize();
    const int border = 2;
    const int totalModules = size + border * 2;

    const int availableW = rcBox.right - rcBox.left;
    const int availableH = rcBox.bottom - rcBox.top;
    const int boxDim = std::min(availableW, availableH);

    const int scale = std::max(1, boxDim / totalModules);
    const int actualDim = totalModules * scale;
    const int xStart = rcBox.left + (availableW - actualDim) / 2;
    const int yStart = rcBox.top + (availableH - actualDim) / 2;

    // White background for QR code quiet zone
    HBRUSH hWhiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    RECT rcBg = { xStart, yStart, xStart + actualDim, yStart + actualDim };
    FillRect(hdc, &rcBg, hWhiteBrush);

    // Black modules
    HBRUSH hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qr.getModule(x, y)) {
                RECT rcMod = {
                    xStart + (x + border) * scale,
                    yStart + (y + border) * scale,
                    xStart + (x + border + 1) * scale,
                    yStart + (y + border + 1) * scale
                };
                FillRect(hdc, &rcMod, hBlackBrush);
            }
        }
    }

    // Draw Cyan corner brackets around the QR Code box
    DrawCyanBrackets(hdc, rcBg);
}

void RefreshUiState() {
    if (!g_hWnd || !g_procMgr) return;

    const bool isRunning = g_procMgr->IsRunning();

    if (g_hBtnStartStop) {
        SetWindowTextW(g_hBtnStartStop, isRunning ? L"■  Stop Host Service" : L"▶  Start Host Service");
    }

    EnableWindow(g_hEditPort, !isRunning);
    EnableWindow(g_hComboFps, !isRunning);
    EnableWindow(g_hComboBitrate, !isRunning);
    EnableWindow(g_hComboMonitor, !isRunning);

    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void UpdateTabVisibility() {
    const bool isHome = (g_currentTab == TAB_HOME);
    int showHome = isHome ? SW_SHOW : SW_HIDE;

    if (g_hEditPcName) ShowWindow(g_hEditPcName, showHome);
    if (g_hEditPcId) ShowWindow(g_hEditPcId, showHome);
    if (g_hBtnCopyId) ShowWindow(g_hBtnCopyId, showHome);
    if (g_hEditPort) ShowWindow(g_hEditPort, showHome);
    if (g_hComboFps) ShowWindow(g_hComboFps, showHome);
    if (g_hComboBitrate) ShowWindow(g_hComboBitrate, showHome);
    if (g_hComboMonitor) ShowWindow(g_hComboMonitor, showHome);
    if (g_hChkConsole) ShowWindow(g_hChkConsole, showHome);
    if (g_hBtnStartStop) ShowWindow(g_hBtnStartStop, showHome);
    if (g_hBtnRegen) ShowWindow(g_hBtnRegen, showHome);
    if (g_hBtnCopyInfo) ShowWindow(g_hBtnCopyInfo, showHome);
    if (g_hBtnInstallDriver) ShowWindow(g_hBtnInstallDriver, showHome);
    if (g_hBtnUninstallDriver) ShowWindow(g_hBtnUninstallDriver, showHome);
    if (g_hBtnGuide) ShowWindow(g_hBtnGuide, showHome);

    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void OnStartStopClicked() {
    if (!g_procMgr) return;

    if (g_procMgr->IsRunning()) {
        AppendLogLine("[INFO] Stopping host streaming service...");
        g_procMgr->Stop();
        AppendLogLine("[INFO] Host streaming service stopped.");
        RefreshUiState();
        return;
    }

    wchar_t nameBuf[128] = {0};
    GetWindowTextW(g_hEditPcName, nameBuf, 128);
    char utf8Name[256] = {0};
    WideCharToMultiByte(CP_UTF8, 0, nameBuf, -1, utf8Name, 256, nullptr, nullptr);

    wchar_t portBuf[16] = {0};
    GetWindowTextW(g_hEditPort, portBuf, 16);
    uint16_t port = 52025;
    try {
        int p = _wtoi(portBuf);
        if (p > 0 && p <= 65535) port = static_cast<uint16_t>(p);
    } catch (...) {}

    if (g_pairingMgr) {
        g_pairingMgr->Initialize(utf8Name, port);
    }

    uint32_t fps = 60;
    int fpsIdx = (int)SendMessageW(g_hComboFps, CB_GETCURSEL, 0, 0);
    if (fpsIdx == 0) fps = 120;
    else if (fpsIdx == 1) fps = 90;
    else if (fpsIdx == 2) fps = 60;
    else if (fpsIdx == 3) fps = 30;

    uint32_t bitrateKbps = 4000;
    int bitIdx = (int)SendMessageW(g_hComboBitrate, CB_GETCURSEL, 0, 0);
    if (bitIdx == 0) bitrateKbps = 2000;
    else if (bitIdx == 1) bitrateKbps = 4000;
    else if (bitIdx == 2) bitrateKbps = 8000;
    else if (bitIdx == 3) bitrateKbps = 12000;

    int monitorIdx = -1;
    int monSel = (int)SendMessageW(g_hComboMonitor, CB_GETCURSEL, 0, 0);
    if (monSel == 1) monitorIdx = 0;
    else if (monSel == 2) monitorIdx = 1;

    const bool showConsole = (SendMessageW(g_hChkConsole, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (showConsole) {
        AllocConsole();
    }

    AppendLogLine("[INFO] Starting DuetView Streaming Engine (" + std::to_string(port) + ", " +
                  std::to_string(fps) + " FPS, " + std::to_string(bitrateKbps) + " Kbps, " +
                  (monitorIdx == -1 ? "Auto Monitor" : "Monitor #" + std::to_string(monitorIdx)) + ")...");

    bool started = g_procMgr->Start(
        port,
        utf8Name,
        fps,
        bitrateKbps,
        monitorIdx,
        g_pairingMgr.get(),
        [](const std::string& line) {
            std::lock_guard<std::mutex> lock(g_logMutex);
            g_pendingLogLines.push_back(line);
            if (g_hWnd) {
                PostMessageW(g_hWnd, WM_USER_APPEND_LOG, 0, 0);
            }
        }
    );

    if (!started) {
        std::string err = g_procMgr->GetLastError();
        AppendLogLine("[ERROR] " + err);
        MessageBoxA(g_hWnd, err.c_str(), "DuetView PC Engine Error", MB_ICONERROR | MB_OK);
    } else {
        AppendLogLine("[INFO] Streaming engine online (in-process).");
    }

    RefreshUiState();
}

void RefreshDriverStatus() {
    DriverInfo newInfo{};
    if (g_driverMgr) {
        newInfo = g_driverMgr->DetectDriverState();
    }

    bool hasDriverBinary = false;
    if (g_driverMgr) {
        std::wstring infPath = g_driverMgr->FindDriverInfPath();
        wchar_t driverDir[MAX_PATH] = {0};
        wcscpy_s(driverDir, infPath.c_str());
        PathRemoveFileSpecW(driverDir);
        std::wstring dllPath = std::wstring(driverDir) + L"\\SecondScreenIdd.dll";
        hasDriverBinary = PathFileExistsW(dllPath.c_str());
    }

    static DriverState lastReportedState = static_cast<DriverState>(999);
    bool stateChanged = (newInfo.state != lastReportedState);
    g_driverInfo = newInfo;

    if (stateChanged) {
        lastReportedState = newInfo.state;

        if (g_hBtnInstallDriver) {
            if (g_driverInfo.state == DriverState::NotInstalled ||
                g_driverInfo.state == DriverState::InstallationFailed) {
                ShowWindow(g_hBtnInstallDriver, (g_currentTab == TAB_HOME) ? SW_SHOW : SW_HIDE);
                EnableWindow(g_hBtnInstallDriver, TRUE);
                if (hasDriverBinary) {
                    SetWindowTextW(g_hBtnInstallDriver, L"🛡️ Install Virtual Display Driver");
                } else {
                    SetWindowTextW(g_hBtnInstallDriver, L"ℹ️ Display Mode Info");
                }
            } else if (g_driverInfo.state == DriverState::InstalledButNotOperational) {
                ShowWindow(g_hBtnInstallDriver, (g_currentTab == TAB_HOME) ? SW_SHOW : SW_HIDE);
                EnableWindow(g_hBtnInstallDriver, TRUE);
                SetWindowTextW(g_hBtnInstallDriver, L"🛡️ Repair / Reinstall Driver");
            } else {
                ShowWindow(g_hBtnInstallDriver, SW_HIDE);
            }
        }

        if (g_hBtnUninstallDriver) {
            if (g_driverInfo.state == DriverState::Ready ||
                g_driverInfo.state == DriverState::InstalledButMonitorNotAvailable ||
                g_driverInfo.state == DriverState::InstalledButNotOperational) {
                ShowWindow(g_hBtnUninstallDriver, (g_currentTab == TAB_HOME) ? SW_SHOW : SW_HIDE);
            } else {
                ShowWindow(g_hBtnUninstallDriver, SW_HIDE);
            }
        }

        if (g_hWnd) {
            InvalidateRect(g_hWnd, nullptr, FALSE);
        }
    }
}

void OnInstallDriverClicked() {
    if (!g_driverMgr) return;

    std::wstring infPath = g_driverMgr->FindDriverInfPath();
    wchar_t driverDir[MAX_PATH] = {0};
    wcscpy_s(driverDir, infPath.c_str());
    PathRemoveFileSpecW(driverDir);
    std::wstring dllPath = std::wstring(driverDir) + L"\\SecondScreenIdd.dll";
    bool hasDriverBinary = PathFileExistsW(dllPath.c_str());

    if (!hasDriverBinary) {
        MessageBoxW(g_hWnd,
            L"DuetView PC Display Modes\n\n"
            L"• Desktop Mirroring: ACTIVE & READY!\n"
            L"  DuetView PC is currently set up for wireless Desktop Mirroring. No driver is needed!\n"
            L"  Click 'Start Host Service' and scan the QR code with your Android app to stream your screen immediately with touch and keyboard controls.\n\n"
            L"• Extended Display Mode (Virtual Second Monitor):\n"
            L"  Extended mode uses an optional Microsoft IddCx virtual display driver. To use Extended Mode, "
            L"place the compiled 'SecondScreenIdd.dll' into your DuetView 'driver' directory.\n\n"
            L"Desktop Mirroring is 100% functional right now without any driver.",
            L"DuetView PC Display Modes",
            MB_ICONINFORMATION | MB_OK);
        return;
    }

    AppendLogLine("[INFO] Requesting administrative approval for virtual display driver installation...");
    AppendLogLine("[INFO] Windows requires administrative approval (UAC) to register display drivers.");

    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->InstallDriver(exitCode, errMsg);

    if (ok) {
        if (exitCode == 3010) {
            AppendLogLine("[WARN] Driver package deployed successfully. Windows reported that a restart is required.");
            MessageBoxW(g_hWnd,
                L"DuetView Virtual Display Driver installed successfully!\n\n"
                L"Windows reported that a system restart is required before the virtual display device becomes operational.\n\n"
                L"Desktop Mirroring remains fully available immediately without restarting.",
                L"DuetView PC Driver Setup",
                MB_ICONINFORMATION | MB_OK);
        } else {
            AppendLogLine("[INFO] Virtual display driver installed and verified successfully (Code 0).");
            MessageBoxW(g_hWnd,
                L"DuetView Virtual Display Driver installed successfully!\n\n"
                L"Press Win + P and select 'Extend' to activate your virtual second monitor.",
                L"DuetView PC Driver Setup",
                MB_ICONINFORMATION | MB_OK);
        }
    } else {
        AppendLogLine("[ERROR] Driver installation failed: " + errMsg);
        MessageBoxA(g_hWnd, errMsg.c_str(), "DuetView PC Driver Installation", MB_ICONWARNING | MB_OK);
    }

    RefreshDriverStatus();
}

void OnUninstallDriverClicked() {
    if (!g_driverMgr) return;

    int confirm = MessageBoxW(g_hWnd,
        L"Are you sure you want to uninstall the DuetView Virtual Display Driver?\n\n"
        L"This will remove the virtual display device node and disable Extend mode.\n\n"
        L"(Desktop Mirroring will remain functional.)",
        L"Uninstall DuetView Driver",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

    if (confirm != IDYES) return;

    AppendLogLine("[INFO] Uninstalling DuetView Virtual Display Driver...");
    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->UninstallDriver(exitCode, errMsg);

    if (ok) {
        AppendLogLine("[INFO] Virtual display driver uninstalled successfully.");
        MessageBoxW(g_hWnd, L"DuetView Virtual Display Driver uninstalled successfully.", L"DuetView PC", MB_ICONINFORMATION | MB_OK);
    } else {
        AppendLogLine("[ERROR] Failed to uninstall driver: " + errMsg);
        MessageBoxA(g_hWnd, errMsg.c_str(), "DuetView PC Driver Uninstallation", MB_ICONWARNING | MB_OK);
    }

    RefreshDriverStatus();
}

void CheckFirstLaunchDriverSetup() {
    if (!g_driverMgr) return;

    DriverInfo info = g_driverMgr->DetectDriverState();

    if (info.state == DriverState::Ready) {
        AppendLogLine("[INFO] Virtual Extended Display Driver is active and operational.");
        return;
    }
    if (info.state == DriverState::InstalledButMonitorNotAvailable) {
        AppendLogLine("[INFO] Virtual Extended Display Driver is active. Press Win + P and choose 'Extend' to enable.");
        return;
    }
    if (info.state == DriverState::RestartRequired) {
        AppendLogLine("[WARN] System restart is required before the virtual display device becomes operational.");
        return;
    }
    if (info.state == DriverState::InstalledButNotOperational) {
        AppendLogLine("[WARN] Driver reported a device error: " + info.diagnosticMessage);
        return;
    }

    std::wstring infPath = g_driverMgr->FindDriverInfPath();
    wchar_t driverDir[MAX_PATH] = {0};
    wcscpy_s(driverDir, infPath.c_str());
    PathRemoveFileSpecW(driverDir);
    std::wstring dllPath = std::wstring(driverDir) + L"\\SecondScreenIdd.dll";
    bool hasDriverBinary = PathFileExistsW(dllPath.c_str());

    if (!hasDriverBinary) {
        AppendLogLine("[INFO] DuetView PC operating in Desktop Mirroring mode (driver-free). Ready to stream.");
        return;
    }

    HKEY hKey = NULL;
    DWORD promptSeen = 0;
    DWORD cbData = sizeof(promptSeen);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\DuetView\\Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"FirstLaunchPromptSeen", nullptr, nullptr, reinterpret_cast<LPBYTE>(&promptSeen), &cbData);
        RegCloseKey(hKey);
    }

    if (promptSeen) {
        AppendLogLine("[INFO] Operating in Desktop Mirroring mode. Virtual display driver available to install.");
        return;
    }

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\DuetView\\Settings", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = 1;
        RegSetValueExW(hKey, L"FirstLaunchPromptSeen", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hKey);
    }
}

void OnRegenerateQrClicked() {
    if (g_pairingMgr) {
        g_pairingMgr->GenerateNewPairingToken(300);
        AppendLogLine("[INFO] Regenerated fresh 5-minute QR pairing token.");
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
}

void OnCopyPairingInfoClicked() {
    if (!g_pairingMgr) return;
    const auto payload = g_pairingMgr->GetCurrentPayload();
    std::string json = payload.ToJson();
    if (OpenClipboard(g_hWnd)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, json.size() + 1);
        if (hGlob) {
            memcpy(GlobalLock(hGlob), json.data(), json.size() + 1);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
        AppendLogLine("[INFO] Copied QR pairing payload to Windows clipboard.");
    }
}

void OnToggleLogsClicked() {
    g_logsExpanded = !g_logsExpanded;
    SetWindowTextW(g_hBtnToggleLogs, g_logsExpanded ? L"📄 Diagnostic Logs  ▲" : L"📄 Diagnostic Logs  ▼");

    RECT wndRc;
    GetWindowRect(g_hWnd, &wndRc);
    int newH = g_logsExpanded ? 800 : 660;
    SetWindowPos(g_hWnd, NULL, 0, 0, wndRc.right - wndRc.left, newH, SWP_NOMOVE | SWP_NOZORDER);

    ShowWindow(g_hEditLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnClearLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnCopyLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

} // namespace

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // High quality typography
        g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontSub = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontNormal = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontMono = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        g_hBrushBg = CreateSolidBrush(COLOR_BG);
        g_hBrushSidebarBg = CreateSolidBrush(COLOR_SIDEBAR_BG);
        g_hBrushCardBg = CreateSolidBrush(COLOR_CARD_BG);
        g_hBrushInputBg = CreateSolidBrush(COLOR_INPUT_BG);

        // Top Header Action: Display Settings (Win + P)
        g_hBtnDisplay = CreateWindowW(L"BUTTON", L"🖥️ Display Settings (Win + P)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            670, 16, 195, 30, hWnd, (HMENU)IDC_BTN_DISPLAY_SETTINGS, NULL, NULL);
        SendMessageW(g_hBtnDisplay, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // 1. Left Card: Host Configuration Controls
        // PC Display Name
        g_hEditPcName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"DuetView-PC",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            218, 186, 360, 26, hWnd, (HMENU)IDC_EDIT_PC_NAME, NULL, NULL);
        SendMessageW(g_hEditPcName, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        if (g_pairingMgr) {
            std::string name = g_pairingMgr->GetPcName();
            int wLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
            if (wLen > 0) {
                std::wstring wName(wLen - 1, 0);
                MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wLen);
                SetWindowTextW(g_hEditPcName, wName.c_str());
            }
        }

        // PC Hardware ID (Read-only) + Copy Button
        g_hEditPcId = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            218, 240, 280, 26, hWnd, (HMENU)IDC_EDIT_PC_ID, NULL, NULL);
        SendMessageW(g_hEditPcId, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
        if (g_pairingMgr) {
            std::string id = g_pairingMgr->GetPcId();
            std::wstring wId(id.begin(), id.end());
            SetWindowTextW(g_hEditPcId, wId.c_str());
        }

        g_hBtnCopyId = CreateWindowW(L"BUTTON", L"📋 Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            505, 240, 73, 26, hWnd, (HMENU)IDC_BTN_COPY_PCID, NULL, NULL);
        SendMessageW(g_hBtnCopyId, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Port
        g_hEditPort = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"52025",
            WS_CHILD | WS_VISIBLE | ES_NUMBER,
            218, 294, 160, 26, hWnd, (HMENU)IDC_EDIT_PORT, NULL, NULL);
        SendMessageW(g_hEditPort, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Target FPS
        g_hComboFps = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            395, 294, 183, 160, hWnd, (HMENU)IDC_COMBO_FPS, NULL, NULL);
        SendMessageW(g_hComboFps, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"120 FPS (120Hz Ultra)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"90 FPS (90Hz Fluid)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"60 FPS (60Hz Smooth)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"30 FPS (30Hz Low Bandwidth)");
        SendMessageW(g_hComboFps, CB_SETCURSEL, 2, 0);

        // Target Bitrate
        g_hComboBitrate = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            218, 348, 360, 150, hWnd, (HMENU)IDC_COMBO_BITRATE, NULL, NULL);
        SendMessageW(g_hComboBitrate, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"2000 Kbps (Lightweight / 2.4 GHz)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"4000 Kbps (Balanced / 5 GHz)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"8000 Kbps (High Fidelity)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"12000 Kbps (Lossless / USB)");
        SendMessageW(g_hComboBitrate, CB_SETCURSEL, 1, 0);

        // Target Monitor / Virtual Display
        g_hComboMonitor = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            218, 402, 360, 150, hWnd, (HMENU)IDC_COMBO_MONITOR, NULL, NULL);
        SendMessageW(g_hComboMonitor, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Auto: Extended / Virtual Screen (Recommended)");
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #0: Primary Desktop Monitor");
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #1: Secondary / Virtual Monitor");
        SendMessageW(g_hComboMonitor, CB_SETCURSEL, 0, 0);

        // Diagnostic Console Checkbox
        g_hChkConsole = CreateWindowW(L"BUTTON", L"Show raw engine console window (Diagnostic)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            218, 442, 360, 20, hWnd, (HMENU)IDC_CHK_CONSOLE, NULL, NULL);
        SendMessageW(g_hChkConsole, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Start / Stop Button
        g_hBtnStartStop = CreateWindowW(L"BUTTON", L"▶  Start Host Service",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            218, 480, 360, 42, hWnd, (HMENU)IDC_BTN_START_STOP, NULL, NULL);
        SendMessageW(g_hBtnStartStop, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // 2. Right Card: Pair & Connect Controls
        g_hBtnRegen = CreateWindowW(L"BUTTON", L"🔄 Regenerate QR",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            625, 365, 175, 32, hWnd, (HMENU)IDC_BTN_REGEN_QR, NULL, NULL);
        SendMessageW(g_hBtnRegen, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnCopyInfo = CreateWindowW(L"BUTTON", L"📋 Copy Info",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            810, 365, 175, 32, hWnd, (HMENU)IDC_BTN_COPY_INFO, NULL, NULL);
        SendMessageW(g_hBtnCopyInfo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnInstallDriver = CreateWindowW(L"BUTTON", L"🛡️ Install Virtual Display Driver",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_INSTALL_DRIVER, NULL, NULL);
        SendMessageW(g_hBtnInstallDriver, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hBtnInstallDriver, BCM_SETSHIELD, 0, TRUE);

        g_hBtnUninstallDriver = CreateWindowW(L"BUTTON", L"Uninstall Driver",
            WS_CHILD | BS_PUSHBUTTON,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_UNINSTALL_DRIVER, NULL, NULL);
        SendMessageW(g_hBtnUninstallDriver, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnGuide = CreateWindowW(L"BUTTON", L"📖 Guide",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            875, 405, 110, 32, hWnd, (HMENU)IDC_BTN_DRIVER_GUIDE, NULL, NULL);
        SendMessageW(g_hBtnGuide, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // 3. Bottom Status Bar & Expandable Logs Drawer
        g_hBtnToggleLogs = CreateWindowW(L"BUTTON", L"📄 Diagnostic Logs  ▼",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 565, 170, 28, hWnd, (HMENU)IDC_BTN_TOGGLE_LOGS, NULL, NULL);
        SendMessageW(g_hBtnToggleLogs, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear",
            WS_CHILD | BS_PUSHBUTTON,
            845, 608, 65, 26, hWnd, (HMENU)IDC_BTN_CLEAR_LOGS, NULL, NULL);
        SendMessageW(g_hBtnClearLogs, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnCopyLogs = CreateWindowW(L"BUTTON", L"Copy All",
            WS_CHILD | BS_PUSHBUTTON,
            918, 608, 72, 26, hWnd, (HMENU)IDC_BTN_COPY_LOGS, NULL, NULL);
        SendMessageW(g_hBtnCopyLogs, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hEditLogs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            200, 640, 790, 110, hWnd, (HMENU)IDC_EDIT_LOGS, NULL, NULL);
        SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

        g_driverMgr = std::make_unique<DriverManager>();
        RefreshDriverStatus();

        SetTimer(hWnd, IDT_TICK_TIMER, 1000, NULL);
        AppendLogLine("[INFO] DuetView PC Desktop Host Manager Initialized.");
        PostMessageW(hWnd, WM_USER_FIRST_LAUNCH, 0, 0);
        return 0;
    }

    case WM_USER_FIRST_LAUNCH: {
        CheckFirstLaunchDriverSetup();
        if (g_procMgr && !g_procMgr->IsRunning()) {
            OnStartStopClicked();
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Sidebar navigation clicks
        if (x >= 12 && x <= 172) {
            if (y >= 84 && y < 124) { g_currentTab = TAB_HOME; UpdateTabVisibility(); }
            else if (y >= 128 && y < 168) { g_currentTab = TAB_STREAMING; UpdateTabVisibility(); }
            else if (y >= 172 && y < 212) { g_currentTab = TAB_DEVICES; UpdateTabVisibility(); }
            else if (y >= 216 && y < 256) { g_currentTab = TAB_DRIVER; UpdateTabVisibility(); }
            else if (y >= 260 && y < 300) { g_currentTab = TAB_SETTINGS; UpdateTabVisibility(); }
            else if (y >= 304 && y < 344) { g_currentTab = TAB_GUIDE; UpdateTabVisibility(); }
            else if (y >= 560 && y < 600) { g_currentTab = TAB_ABOUT; UpdateTabVisibility(); }
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_START_STOP) {
            OnStartStopClicked();
        } else if (id == IDC_BTN_REGEN_QR) {
            OnRegenerateQrClicked();
        } else if (id == IDC_BTN_COPY_INFO) {
            OnCopyPairingInfoClicked();
        } else if (id == IDC_BTN_INSTALL_DRIVER) {
            OnInstallDriverClicked();
        } else if (id == IDC_BTN_UNINSTALL_DRIVER) {
            OnUninstallDriverClicked();
        } else if (id == IDC_BTN_DISPLAY_SETTINGS) {
            SetDisplayConfig(0, NULL, 0, NULL, SDC_TOPOLOGY_EXTEND | SDC_APPLY);
            AppendLogLine("[INFO] Requested Windows Extend display topology (SDC_TOPOLOGY_EXTEND).");
            ShellExecuteW(NULL, L"open", L"ms-settings:display", NULL, NULL, SW_SHOWNORMAL);
        } else if (id == IDC_BTN_DRIVER_GUIDE) {
            ShellExecuteW(NULL, L"open", L"docs\\driver-installation-guide.md", NULL, NULL, SW_SHOWNORMAL);
        } else if (id == IDC_BTN_TOGGLE_LOGS) {
            OnToggleLogsClicked();
        } else if (id == IDC_BTN_CLEAR_LOGS) {
            SetWindowTextW(g_hEditLogs, L"");
        } else if (id == IDC_BTN_COPY_LOGS) {
            int len = GetWindowTextLengthW(g_hEditLogs);
            if (len > 0) {
                std::vector<wchar_t> buf(len + 1);
                GetWindowTextW(g_hEditLogs, buf.data(), len + 1);
                if (OpenClipboard(hWnd)) {
                    EmptyClipboard();
                    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                    if (hGlob) {
                        memcpy(GlobalLock(hGlob), buf.data(), (len + 1) * sizeof(wchar_t));
                        GlobalUnlock(hGlob);
                        SetClipboardData(CF_UNICODETEXT, hGlob);
                    }
                    CloseClipboard();
                }
            }
        } else if (id == IDC_BTN_COPY_PCID) {
            HWND hId = GetDlgItem(hWnd, IDC_EDIT_PC_ID);
            int len = GetWindowTextLengthW(hId);
            if (len > 0) {
                std::vector<wchar_t> buf(len + 1);
                GetWindowTextW(hId, buf.data(), len + 1);
                if (OpenClipboard(hWnd)) {
                    EmptyClipboard();
                    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                    if (hGlob) {
                        memcpy(GlobalLock(hGlob), buf.data(), (len + 1) * sizeof(wchar_t));
                        GlobalUnlock(hGlob);
                        SetClipboardData(CF_UNICODETEXT, hGlob);
                    }
                    CloseClipboard();
                }
            }
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_USER_APPEND_LOG: {
        std::vector<std::string> lines;
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            lines.swap(g_pendingLogLines);
        }
        for (const auto& line : lines) {
            AppendLogLine(line);
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == IDT_TICK_TIMER) {
            if (g_procMgr) {
                static bool wasRunning = false;
                bool isNowRunning = g_procMgr->IsRunning();
                if (wasRunning != isNowRunning) {
                    wasRunning = isNowRunning;
                    AppendLogLine(isNowRunning ? "[INFO] Streaming engine online (in-process)." : "[INFO] Streaming engine stopped.");
                    RefreshUiState();
                }
            }

            // Invalidate QR timer area and status
            if (g_currentTab == TAB_HOME) {
                RECT rcTimer = { 820, 180, 990, 350 };
                InvalidateRect(hWnd, &rcTimer, FALSE);
            }

            static uint32_t timerTick = 0;
            if (++timerTick % 3 == 0) {
                RefreshDriverStatus();
            }
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRc;
        GetClientRect(hWnd, &clientRc);
        int clientW = clientRc.right - clientRc.left;
        int clientH = clientRc.bottom - clientRc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        // Fill background
        FillRect(memDC, &clientRc, g_hBrushBg);

        // ---------------------------------------------------------------------
        // 1. LEFT SIDEBAR (Width 185px)
        // ---------------------------------------------------------------------
        RECT rcSidebar = { 0, 0, 185, clientH };
        FillRect(memDC, &rcSidebar, g_hBrushSidebarBg);

        HPEN hSidebarPen = CreatePen(PS_SOLID, 1, COLOR_SIDEBAR_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(memDC, hSidebarPen);
        MoveToEx(memDC, 185, 0, NULL);
        LineTo(memDC, 185, clientH);
        SelectObject(memDC, hOldPen);
        DeleteObject(hSidebarPen);

        // Sidebar Logo Box (24, 20, 72, 68)
        RECT rcLogo = { 24, 20, 72, 68 };
        HBRUSH hLogoBrush = CreateSolidBrush(RGB(20, 35, 68));
        HPEN hLogoPen = CreatePen(PS_SOLID, 1, RGB(45, 75, 135));
        SelectObject(memDC, hLogoBrush);
        SelectObject(memDC, hLogoPen);
        RoundRect(memDC, rcLogo.left, rcLogo.top, rcLogo.right, rcLogo.bottom, 12, 12);
        DeleteObject(hLogoBrush);
        DeleteObject(hLogoPen);

        if (g_hAppIcon) {
            DrawIconEx(memDC, 32, 28, g_hAppIcon, 32, 32, 0, NULL, DI_NORMAL);
        }

        // Sidebar Nav Items
        struct NavTabItem {
            AppTab tab;
            int y;
            const wchar_t* text;
        };

        const NavTabItem navItems[] = {
            { TAB_HOME, 84, L"  🏠  Home" },
            { TAB_STREAMING, 128, L"  🖥️  Streaming" },
            { TAB_DEVICES, 172, L"  📱  Devices" },
            { TAB_DRIVER, 216, L"  🛡️  Driver" },
            { TAB_SETTINGS, 260, L"  ⚙️  Settings" },
            { TAB_GUIDE, 304, L"  📖  Guide" },
            { TAB_ABOUT, clientH - 52, L"  ℹ️   About" },
        };

        SetBkMode(memDC, TRANSPARENT);
        for (const auto& item : navItems) {
            RECT rcTab = { 12, item.y, 172, item.y + 36 };
            bool isActive = (g_currentTab == item.tab);

            if (isActive) {
                HBRUSH hActiveBrush = CreateSolidBrush(COLOR_TAB_ACTIVE);
                HBRUSH hOldB = (HBRUSH)SelectObject(memDC, hActiveBrush);
                HPEN hNullPen = (HPEN)GetStockObject(NULL_PEN);
                HPEN hOldP = (HPEN)SelectObject(memDC, hNullPen);
                RoundRect(memDC, rcTab.left, rcTab.top, rcTab.right, rcTab.bottom, 10, 10);
                SelectObject(memDC, hOldB);
                SelectObject(memDC, hOldP);
                DeleteObject(hActiveBrush);

                SelectObject(memDC, g_hFontBold);
                SetTextColor(memDC, RGB(255, 255, 255));
            } else {
                SelectObject(memDC, g_hFontNormal);
                SetTextColor(memDC, COLOR_TEXT_MUTED);
            }

            RECT rcText = { rcTab.left + 12, rcTab.top, rcTab.right - 10, rcTab.bottom };
            DrawTextW(memDC, item.text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // ---------------------------------------------------------------------
        // 2. TOP HEADER (Right of sidebar)
        // ---------------------------------------------------------------------
        // App Title
        SelectObject(memDC, g_hFontTitle);
        SetTextColor(memDC, COLOR_TEXT_MAIN);
        TextOutW(memDC, 202, 18, L"DuetView PC", 11);

        // Subtitle & Separator
        SelectObject(memDC, g_hFontSub);
        SetTextColor(memDC, COLOR_TEXT_MUTED);
        TextOutW(memDC, 335, 22, L"|   Windows Host Manager & Streaming Hub", 40);

        // Status Badge Pill
        const bool isRunning = (g_procMgr && g_procMgr->IsRunning());
        COLORREF badgeBg = isRunning ? RGB(6, 78, 59) : RGB(30, 41, 59);
        COLORREF badgeBorder = isRunning ? RGB(16, 185, 129) : RGB(71, 85, 105);
        const wchar_t* statusText = isRunning ? L"●  Host Online" : L"○  Host Stopped";

        HBRUSH hBadgeBrush = CreateSolidBrush(badgeBg);
        HPEN hBadgePen = CreatePen(PS_SOLID, 1, badgeBorder);
        RECT rcBadge = { clientRc.right - 150, 16, clientRc.right - 20, 46 };
        HBRUSH hOldB = (HBRUSH)SelectObject(memDC, hBadgeBrush);
        HPEN hOldP = (HPEN)SelectObject(memDC, hBadgePen);
        RoundRect(memDC, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 14, 14);
        SelectObject(memDC, hOldB);
        SelectObject(memDC, hOldP);
        DeleteObject(hBadgeBrush);
        DeleteObject(hBadgePen);

        SetTextColor(memDC, isRunning ? COLOR_SUCCESS : COLOR_TEXT_MUTED);
        SelectObject(memDC, g_hFontBold);
        DrawTextW(memDC, statusText, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // ---------------------------------------------------------------------
        // 3. TAB CONTENT
        // ---------------------------------------------------------------------
        if (g_currentTab == TAB_HOME) {
            // Hero Banner
            RECT rcHero = { 200, 58, clientRc.right - 20, 118 };
            HBRUSH hHeroBrush = CreateSolidBrush(COLOR_HERO_BG);
            HPEN hHeroPen = CreatePen(PS_SOLID, 1, COLOR_HERO_BORDER);
            SelectObject(memDC, hHeroBrush);
            SelectObject(memDC, hHeroPen);
            RoundRect(memDC, rcHero.left, rcHero.top, rcHero.right, rcHero.bottom, 12, 12);
            DeleteObject(hHeroBrush);
            DeleteObject(hHeroPen);

            // Hero Text
            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 218, 68, L"Turn your Android device into a second display", 46);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 218, 92, L"Extend, mirror, and control your PC seamlessly over Wi-Fi or USB.", 65);

            // Left Card: Host Configuration
            RECT rcSettings = { 200, 128, 595, 545 };
            DrawCard(memDC, rcSettings, L"Host Configuration", L"Configure your streaming and display settings.");

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 218, 168, L"PC Display Name:", 16);
            TextOutW(memDC, 218, 222, L"PC Hardware ID:", 15);
            TextOutW(memDC, 218, 276, L"Port:", 5);
            TextOutW(memDC, 395, 276, L"Target FPS:", 11);
            TextOutW(memDC, 218, 330, L"Streaming Bitrate Profile:", 26);
            TextOutW(memDC, 218, 384, L"Target Monitor Source:", 22);

            // Right Card: Pair & Connect
            RECT rcPairing = { 610, 128, clientRc.right - 20, 545 };
            DrawCard(memDC, rcPairing, L"Pair & Connect", L"Scan this QR code with the DuetView Android app (Scan to Connect).");

            // QR Box with Cyan Brackets
            RECT rcQrBox = { 625, 180, 795, 350 };
            DrawQrCodeBox(memDC, rcQrBox);

            // Side Metadata Panel (next to QR code)
            RECT rcMeta = { 815, 185, clientRc.right - 35, 345 };
            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, rcMeta.left, 192, L"Token expires in:", 17);

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_ACCENT);
            if (g_pairingMgr) {
                const auto payload = g_pairingMgr->GetCurrentPayload();
                const auto nowSec = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                uint64_t remaining = (payload.expirySec > nowSec) ? (payload.expirySec - nowSec) : 0;
                uint64_t mins = remaining / 60;
                uint64_t secs = remaining % 60;
                std::wostringstream ss;
                ss << std::setfill(L'0') << std::setw(2) << mins << L"m "
                   << std::setfill(L'0') << std::setw(2) << secs << L"s";
                std::wstring timeStr = ss.str();
                TextOutW(memDC, rcMeta.left, 210, timeStr.c_str(), (int)timeStr.size());
            }

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, rcMeta.left, 245, L"Network:", 8);

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            std::string primIp = "127.0.0.1";
            if (g_pairingMgr) {
                const auto endpoints = g_pairingMgr->GetLocalIPv4Endpoints();
                if (!endpoints.empty()) primIp = endpoints[0];
            }
            std::wstring wPrimIp(primIp.begin(), primIp.end());
            TextOutW(memDC, rcMeta.left, 263, wPrimIp.c_str(), (int)wPrimIp.size());

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, rcMeta.left, 298, L"Status:", 7);

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, isRunning ? COLOR_SUCCESS : COLOR_TEXT_MUTED);
            TextOutW(memDC, rcMeta.left, 316, isRunning ? L"Ready to Stream" : L"Host Inactive", isRunning ? 15 : 13);

            // Bottom Status Banner in Right Card
            RECT rcStatusBanner = { 625, 452, clientRc.right - 35, 528 };
            HBRUSH hBannerBrush = CreateSolidBrush(RGB(12, 38, 48));
            HPEN hBannerPen = CreatePen(PS_SOLID, 1, RGB(22, 78, 88));
            SelectObject(memDC, hBannerBrush);
            SelectObject(memDC, hBannerPen);
            RoundRect(memDC, rcStatusBanner.left, rcStatusBanner.top, rcStatusBanner.right, rcStatusBanner.bottom, 10, 10);
            DeleteObject(hBannerBrush);
            DeleteObject(hBannerPen);

            SelectObject(memDC, g_hFontBold);
            if (g_driverInfo.state == DriverState::Ready) {
                SetTextColor(memDC, COLOR_SUCCESS);
                TextOutW(memDC, rcStatusBanner.left + 14, rcStatusBanner.top + 12, L"✓ Extended Display: Active & Operational", 40);
                SelectObject(memDC, g_hFontNormal);
                SetTextColor(memDC, COLOR_TEXT_MUTED);
                TextOutW(memDC, rcStatusBanner.left + 14, rcStatusBanner.top + 34, L"Virtual second display connected. Independent desktop active.", 61);
            } else {
                SetTextColor(memDC, COLOR_SUCCESS);
                TextOutW(memDC, rcStatusBanner.left + 14, rcStatusBanner.top + 12, L"✓ Desktop Mirroring: Ready to Stream", 36);
                SelectObject(memDC, g_hFontNormal);
                SetTextColor(memDC, COLOR_TEXT_MUTED);
                TextOutW(memDC, rcStatusBanner.left + 14, rcStatusBanner.top + 34, L"Primary display mirror active. Touch & keyboard supported.", 58);
            }

            // -----------------------------------------------------------------
            // 4. BOTTOM STATUS BAR
            // -----------------------------------------------------------------
            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            std::wstring ipNotice = L"ℹ  Reachable LAN Address: " + wPrimIp;
            TextOutW(memDC, 395, 570, ipNotice.c_str(), (int)ipNotice.size());

            TextOutW(memDC, 700, 570, L"📶  Android and PC must be connected to the same Wi-Fi.", 55);
        } else if (g_currentTab == TAB_STREAMING) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"Streaming Pipeline & Engine Telemetry", L"Real-time DXGI capture and Media Foundation H.264 hardware encoder metrics.");

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_ACCENT);
            TextOutW(memDC, 225, 120, L"Capture & Pipeline Status", 25);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 150, L"• Screen Capture API: Direct3D 11 & DXGI Desktop Duplication (Hardware Accelerated)", 82);
            TextOutW(memDC, 225, 180, L"• Video Encoder: Windows Media Foundation (IMFTransform) H.264 Baseline/High Profile", 84);
            TextOutW(memDC, 225, 210, L"• Packet Framing: SecondScreen SS02 with SPS/PPS Configuration Insertion", 72);
            TextOutW(memDC, 225, 240, L"• Transport Layer: Low-latency non-blocking TCP socket stream", 60);

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_SUCCESS);
            TextOutW(memDC, 225, 290, L"Active Streaming Performance", 28);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 225, 320, isRunning ? L"Host Engine: ONLINE and capturing display output." : L"Host Engine: STOPPED (Click 'Home' -> 'Start Host Service' to begin).", isRunning ? 49 : 71);
            TextOutW(memDC, 225, 350, L"Dynamic bitrates adjust smoothly between 2,000 Kbps and 12,000 Kbps based on Wi-Fi conditions.", 94);
        } else if (g_currentTab == TAB_DEVICES) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"Connected & Paired Android Devices", L"Manage devices permitted to stream and control this PC.");

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 130, L"Automatic Discovery Port: 52026 (UDP)", 37);
            TextOutW(memDC, 225, 160, L"Pairing Authentication Port: 52027 (TCP)", 40);
            TextOutW(memDC, 225, 190, L"Primary Video Streaming Port: 52025 (TCP)", 41);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 225, 240, L"When an Android client connects, pairing tokens are verified securely over the local network.", 94);
            TextOutW(memDC, 225, 270, L"Touch inputs and keyboard events from authorized devices are injected directly via SendInput.", 94);
        } else if (g_currentTab == TAB_DRIVER) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"Windows Virtual Display Driver (IddCx)", L"Configure the optional indirect display driver for extended second monitor mode.");

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_ACCENT);
            TextOutW(memDC, 225, 130, L"Driver Status:", 14);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            if (g_driverInfo.state == DriverState::Ready) {
                TextOutW(memDC, 340, 130, L"Installed and Operational (Virtual Monitor Ready)", 49);
            } else if (g_driverInfo.state == DriverState::NotInstalled) {
                TextOutW(memDC, 340, 130, L"Not Installed (Operating in Desktop Mirroring Mode)", 51);
            } else {
                TextOutW(memDC, 340, 130, L"Driver Configured (See Home tab to manage)", 42);
            }

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 225, 180, L"• Architecture: Microsoft Indirect Display Driver (IddCx / UMDF 2.x)", 68);
            TextOutW(memDC, 225, 210, L"• Hardware ID: Root\\SecondScreenIdd", 35);
            TextOutW(memDC, 225, 240, L"• Virtual Modes: 1920x1080 @ 60Hz, 1280x720 @ 60Hz, 1024x768 @ 60Hz", 68);
            TextOutW(memDC, 225, 270, L"• Windows Shortcut: Press Win + P at any time and select 'Extend'.", 66);
        } else if (g_currentTab == TAB_SETTINGS) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"Host Settings & Preferences", L"Configure application behavior, autostart, and network interface bindings.");

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 130, L"• Auto-Start Streaming: Host automatically starts on PC application launch.", 75);
            TextOutW(memDC, 225, 160, L"• LAN Discovery Broadcasts: Broadcasts UDP beacon every 2 seconds for Android discovery.", 89);
            TextOutW(memDC, 225, 190, L"• Touch Input Injection: Translates normalized touch points to absolute Windows screen coordinates.", 100);
            TextOutW(memDC, 225, 220, L"• Hardware Acceleration: Direct3D 11 Video Acceleration enabled.", 64);
        } else if (g_currentTab == TAB_GUIDE) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"Quick Connection Guide", L"Step-by-step instructions for connecting your Android device.");

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_ACCENT);
            TextOutW(memDC, 225, 120, L"Connecting Over Wi-Fi (Wireless):", 34);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 150, L"1. Connect both your PC and Android phone/tablet to the same Wi-Fi network (5 GHz recommended).", 95);
            TextOutW(memDC, 225, 175, L"2. Launch the DuetView Android app and tap 'Scan to Connect'.", 61);
            TextOutW(memDC, 225, 200, L"3. Point your phone camera at the QR code on the Home tab. Your screen will begin streaming immediately!", 104);

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_ACCENT);
            TextOutW(memDC, 225, 250, L"Connecting Over USB (Ultra-Low Latency):", 40);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 280, L"1. Connect your Android device via USB and enable 'USB Tethering' in Android Settings.", 87);
            TextOutW(memDC, 225, 305, L"2. Open DuetView Android app, tap 'Discover PCs' or 'Scan to Connect'.", 70);
            TextOutW(memDC, 225, 330, L"3. Experience lightning-fast sub-5ms display streaming with zero Wi-Fi interference.", 84);
        } else if (g_currentTab == TAB_ABOUT) {
            RECT rcTabCard = { 200, 60, clientRc.right - 20, 545 };
            DrawCard(memDC, rcTabCard, L"About DuetView PC", L"Free, high-performance second display software for Windows and Android.");

            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, COLOR_TEXT_MAIN);
            TextOutW(memDC, 225, 130, L"DuetView PC — Version 0.4.0 (Unified Release)", 46);

            SelectObject(memDC, g_hFontNormal);
            SetTextColor(memDC, COLOR_TEXT_MUTED);
            TextOutW(memDC, 225, 165, L"Turn your Android device into a high-performance second monitor for your Windows computer.", 90);
            TextOutW(memDC, 225, 190, L"Powered by Direct3D 11, DXGI Desktop Duplication, and Windows Media Foundation H.264.", 86);
            TextOutW(memDC, 225, 215, L"Touch input and keyboard controls injected natively via Windows SendInput.", 75);
            TextOutW(memDC, 225, 240, L"100% Free, Private, and Local. No cloud servers required.", 57);
        }

        // BitBlt memory backbuffer to primary window surface
        BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdcCtrl = (HDC)wParam;
        HWND hwndCtrl = (HWND)lParam;

        if (hwndCtrl == g_hEditLogs) {
            SetTextColor(hdcCtrl, RGB(226, 232, 240));
            SetBkColor(hdcCtrl, RGB(10, 15, 30));
            static HBRUSH hLogBrush = CreateSolidBrush(RGB(10, 15, 30));
            return (LRESULT)hLogBrush;
        }

        SetTextColor(hdcCtrl, COLOR_TEXT_MAIN);
        SetBkColor(hdcCtrl, COLOR_INPUT_BG);
        return (LRESULT)g_hBrushInputBg;
    }

    case WM_CLOSE: {
        if (g_procMgr && g_procMgr->IsRunning()) {
            int res = MessageBoxW(hWnd,
                L"The host streaming service is currently active.\nDo you want to stop it and exit DuetView PC?",
                L"Confirm Exit", MB_YESNO | MB_ICONQUESTION);
            if (res != IDYES) return 0;
            g_procMgr->Stop();
        }
        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY: {
        KillTimer(hWnd, IDT_TICK_TIMER);
        if (g_procMgr) g_procMgr->Stop();

        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hFontSub) DeleteObject(g_hFontSub);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontMono) DeleteObject(g_hFontMono);

        if (g_hBrushBg) DeleteObject(g_hBrushBg);
        if (g_hBrushSidebarBg) DeleteObject(g_hBrushSidebarBg);
        if (g_hBrushCardBg) DeleteObject(g_hBrushCardBg);
        if (g_hBrushInputBg) DeleteObject(g_hBrushInputBg);

        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    for (int i = 1; i < __argc; ++i) {
        std::string arg = __argv[i];
        if (arg == "--help" || arg == "-h") {
            if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
                FILE* fpOut = nullptr;
                freopen_s(&fpOut, "CONOUT$", "w", stdout);
                std::cout << "========================================================\n"
                          << "  DuetView PC (Unified Host & Streaming Engine)        \n"
                          << "========================================================\n"
                          << "Usage: DuetView-PC.exe [options]\n"
                          << "  --cli               Run streaming engine headlessly in terminal\n"
                          << "  --port <P>          Port to listen on (default: 52025)\n"
                          << "  --fps <F>           Target frame rate (default: 60)\n"
                          << "  --bitrate <B>       Bitrate in Kbps (default: 4000)\n"
                          << "  --monitor <M>       Monitor index or 'auto' (default: auto)\n"
                          << "  --name <N>          Custom PC display name\n"
                          << "  --help              Display this help message\n";
            }
            return 0;
        } else if (arg == "--cli" || arg == "--headless") {
            if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
                FILE* fpOut = nullptr;
                FILE* fpIn = nullptr;
                freopen_s(&fpOut, "CONOUT$", "w", stdout);
                freopen_s(&fpIn, "CONIN$", "r", stdin);

                uint16_t port = 52025;
                uint32_t fps = 60;
                uint32_t bitrate = 4000;
                int monitor = -1;
                std::string name;
                for (int j = 1; j < __argc; ++j) {
                    std::string a = __argv[j];
                    if (a == "--port" && j + 1 < __argc) port = static_cast<uint16_t>(std::atoi(__argv[++j]));
                    else if (a == "--fps" && j + 1 < __argc) fps = static_cast<uint32_t>(std::atoi(__argv[++j]));
                    else if (a == "--bitrate" && j + 1 < __argc) bitrate = static_cast<uint32_t>(std::atoi(__argv[++j]));
                    else if (a == "--name" && j + 1 < __argc) name = __argv[++j];
                    else if (a == "--monitor" && j + 1 < __argc) {
                        std::string m = __argv[++j];
                        if (m != "auto") monitor = std::atoi(m.c_str());
                    }
                }

                std::cout << "[INFO] Starting DuetView PC Engine (CLI mode) on port " << port << "...\n";
                secondscreen::engine::EngineRunner runner;
                secondscreen::pairing::PairingManager pairing;
                pairing.Initialize(name, port);
                pairing.StartPairingServer(52027);
                pairing.PrintConsoleQrCode();

                secondscreen::engine::EngineOptions opt{};
                opt.port = port;
                opt.targetFps = fps;
                opt.bitrateKbps = bitrate;
                opt.selectedMonitor = monitor;
                opt.pcName = name;

                runner.Start(opt, &pairing, [](const std::string& line) {
                    std::cout << line << "\n";
                });

                std::cout << "[INFO] DuetView streaming engine online. Press Enter to stop.\n";
                std::string dummy;
                std::getline(std::cin, dummy);
                runner.Stop();
            }
            return 0;
        }
    }

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    g_procMgr = std::make_unique<ProcessManager>();
    g_pairingMgr = std::make_unique<PairingManager>();
    g_pairingMgr->Initialize();
    g_pairingMgr->StartPairingServer(52027);
    g_cachedEndpoints = g_pairingMgr->GetLocalIPv4Endpoints();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;

    HICON hIcon = (HICON)LoadImageW(hInstance, L"IDI_APP_ICON", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(NULL, L"duetview.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(NULL, L"resources\\duetview.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(NULL, L"native\\windows_engine\\resources\\duetview.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    if (!hIcon) {
        hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    g_hAppIcon = hIcon;
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.lpszClassName = L"DuetViewMainWindowClass";

    if (!RegisterClassExW(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "DuetView PC Error", MB_ICONERROR);
        return 1;
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int wndW = 1040;
    int wndH = 660;
    int posX = (screenW - wndW) / 2;
    int posY = (screenH - wndH) / 2;

    g_hWnd = CreateWindowExW(
        0,
        L"DuetViewMainWindowClass",
        L"DuetView PC",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        posX, posY, wndW, wndH,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        MessageBoxA(NULL, "Failed to create main application window.", "DuetView PC Error", MB_ICONERROR);
        return 1;
    }

    if (hIcon) {
        SendMessageW(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
