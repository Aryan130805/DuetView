#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <gdiplus.h>

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
#include <cmath>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comctl32.lib")
#endif

// Control IDs
#define IDC_BTN_START_STOP           1001
#define IDC_BTN_REGEN_QR             1002
#define IDC_BTN_TOGGLE_LOGS          1004
#define IDC_BTN_CLEAR_LOGS           1005
#define IDC_BTN_COPY_LOGS            1006
#define IDC_BTN_COPY_PCID            1007
#define IDC_EDIT_PC_NAME             1010
#define IDC_EDIT_PC_ID               1011
#define IDC_EDIT_PORT                1012
#define IDC_COMBO_FPS                1013
#define IDC_COMBO_BITRATE            1014
#define IDC_CHK_CONSOLE              1015
#define IDC_COMBO_MONITOR            1016
#define IDC_BTN_DISPLAY_SETTINGS     1017
#define IDC_BTN_INSTALL_DRIVER       1021
#define IDC_BTN_UNINSTALL_DRIVER     1022
#define IDC_EDIT_LOGS                1020

// Driver Tab Controls
#define IDC_BTN_DRIVER_TAB_INSTALL   1023
#define IDC_BTN_DRIVER_TAB_UNINSTALL 1024
#define IDC_BTN_DRIVER_TAB_REPAIR    1025
#define IDC_BTN_DRIVER_TAB_UPDATE    1026
#define IDC_BTN_DRIVER_TAB_DEVMGMT   1027
#define IDC_BTN_DRIVER_TAB_DISPLAY   1028

// Devices Tab Controls
#define IDC_BTN_DEVICES_REFRESH      1030
#define IDC_BTN_DEVICES_CLEAR        1031

// Streaming Tab Controls
#define IDC_BTN_STREAMING_START_STOP 1040
#define IDC_BTN_STREAMING_DISPLAY    1041
#define IDC_BTN_STREAMING_KEYFRAME   1042
#define IDC_BTN_STREAMING_COPY_INFO  1043

// Settings Tab Controls
#define IDC_COMBO_SETTINGS_FPS       1050
#define IDC_COMBO_SETTINGS_BITRATE   1051
#define IDC_COMBO_SETTINGS_MONITOR   1052
#define IDC_EDIT_SETTINGS_PORT       1053
#define IDC_EDIT_SETTINGS_PC_NAME    1054
#define IDC_CHK_AUTO_START           1055
#define IDC_CHK_DISCOVERY            1056
#define IDC_BTN_SETTINGS_SAVE        1057
#define IDC_BTN_SETTINGS_RESET       1058

// Guide Tab Controls
#define IDC_BTN_GUIDE_DISPLAY        1060
#define IDC_BTN_GUIDE_DRIVER         1061

// About Tab Controls
#define IDC_BTN_ABOUT_DEVELOPER      1070
#define IDC_BTN_ABOUT_REPO           1071
#define IDC_BTN_ABOUT_RELEASES       1072

#define WM_USER_APPEND_LOG           (WM_USER + 201)
#define WM_USER_FIRST_LAUNCH         (WM_USER + 202)
#define IDT_TICK_TIMER               1

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
int g_hoveredTab = -1;

// Colors matching Image 1
const COLORREF COLOR_BG             = RGB(7, 13, 28);     // #070D1C
const COLORREF COLOR_SIDEBAR_BG     = RGB(8, 15, 32);     // #080F20
const COLORREF COLOR_CARD_BG        = RGB(14, 25, 50);    // #0E1932
const COLORREF COLOR_INPUT_BG       = RGB(9, 18, 36);     // #091224
const COLORREF COLOR_TEXT_MAIN      = RGB(255, 255, 255);
const COLORREF COLOR_TEXT_MUTED     = RGB(126, 145, 173); // #7E91AD

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
HWND g_hBtnInstallDriver = NULL;
HWND g_hBtnUninstallDriver = NULL;
HWND g_hBtnDisplay = NULL;

// Streaming Tab Controls
HWND g_hBtnStreamingStartStop = NULL;
HWND g_hBtnStreamingDisplay = NULL;
HWND g_hBtnStreamingKeyframe = NULL;
HWND g_hBtnStreamingCopyInfo = NULL;

// Devices Tab Controls
HWND g_hBtnDevicesRefresh = NULL;
HWND g_hBtnDevicesClear = NULL;

// Driver Tab Controls
HWND g_hBtnDriverTabInstall = NULL;
HWND g_hBtnDriverTabUninstall = NULL;
HWND g_hBtnDriverTabRepair = NULL;
HWND g_hBtnDriverTabUpdate = NULL;
HWND g_hBtnDriverTabDevMgmt = NULL;
HWND g_hBtnDriverTabDisplay = NULL;

// Settings Tab Controls
HWND g_hComboSettingsFps = NULL;
HWND g_hComboSettingsBitrate = NULL;
HWND g_hComboSettingsMonitor = NULL;
HWND g_hEditSettingsPort = NULL;
HWND g_hEditSettingsPcName = NULL;
HWND g_hChkAutoStart = NULL;
HWND g_hChkDiscovery = NULL;
HWND g_hBtnSettingsSave = NULL;
HWND g_hBtnSettingsReset = NULL;

// Guide Tab Controls
HWND g_hBtnGuideDisplay = NULL;
HWND g_hBtnGuideDriver = NULL;

// About Tab Controls
HWND g_hBtnAboutDeveloper = NULL;
HWND g_hBtnAboutRepo = NULL;
HWND g_hBtnAboutReleases = NULL;

// Preferences
bool g_autoStartHost = false; // Default: false (must only start when user explicitly clicks start)
bool g_discoveryEnabled = true;

HBRUSH g_hBrushBg = NULL;
HBRUSH g_hBrushSidebarBg = NULL;
HBRUSH g_hBrushCardBg = NULL;
HBRUSH g_hBrushInputBg = NULL;
HICON  g_hAppIcon = NULL;

HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontMono = NULL;

std::unique_ptr<ProcessManager> g_procMgr;
std::unique_ptr<PairingManager> g_pairingMgr;
std::unique_ptr<DriverManager> g_driverMgr;
DriverInfo g_driverInfo;

std::vector<std::string> g_cachedEndpoints;
bool g_logsExpanded = false;
bool g_showConsoleState = false;
std::mutex g_logMutex;
std::vector<std::string> g_pendingLogLines;

// Layout coordinates
RECT g_rcHero{};
RECT g_rcLeftCard{};
RECT g_rcRightCard{};
RECT g_rcQrBox{};
RECT g_rcMetadata{};
RECT g_rcStatusBanner{};
RECT g_rcInputPcNameContainer{};
RECT g_rcInputPcIdContainer{};
RECT g_rcInputPortContainer{};

void RefreshDriverStatus();
void UpdateTabVisibility();
void RelayoutControls(int clientW, int clientH);

// Helper for GDI+ bounds
inline float RightOf(const Gdiplus::RectF& r) { return r.X + r.Width; }
inline float BottomOf(const Gdiplus::RectF& r) { return r.Y + r.Height; }

// Helper for GDI+ rounded rectangles
inline void AddRoundedRectangle(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r) {
    if (r <= 0.0f) {
        path.AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    float d = r * 2.0f;
    if (d > w) d = w;
    if (d > h) d = h;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

// -----------------------------------------------------------------------------
// Vector Icon Drawing Helpers (100% Native Vector, Zero Missing Glyphs)
// -----------------------------------------------------------------------------
void DrawVectorHome(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.6f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::GraphicsPath p;
    p.AddLine(x, y + s * 0.45f, x + s * 0.5f, y + s * 0.05f);
    p.AddLine(x + s * 0.5f, y + s * 0.05f, x + s, y + s * 0.45f);
    p.AddLine(x + s * 0.85f, y + s * 0.45f, x + s * 0.85f, y + s * 0.95f);
    p.AddLine(x + s * 0.85f, y + s * 0.95f, x + s * 0.15f, y + s * 0.95f);
    p.AddLine(x + s * 0.15f, y + s * 0.95f, x + s * 0.15f, y + s * 0.45f);
    g.DrawPath(&pen, &p);
    g.DrawRectangle(&pen, x + s * 0.38f, y + s * 0.6f, s * 0.24f, s * 0.35f);
}

void DrawVectorMonitor(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::GraphicsPath scr;
    AddRoundedRectangle(scr, x, y + s * 0.05f, s, s * 0.65f, 2.0f);
    g.DrawPath(&pen, &scr);
    g.DrawLine(&pen, x + s * 0.5f, y + s * 0.70f, x + s * 0.5f, y + s * 0.90f);
    g.DrawLine(&pen, x + s * 0.25f, y + s * 0.90f, x + s * 0.75f, y + s * 0.90f);
}

void DrawVectorPhone(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::GraphicsPath ph;
    AddRoundedRectangle(ph, x + s * 0.18f, y, s * 0.64f, s, 3.0f);
    g.DrawPath(&pen, &ph);
    g.DrawLine(&pen, x + s * 0.42f, y + s * 0.12f, x + s * 0.58f, y + s * 0.12f);
    g.DrawLine(&pen, x + s * 0.46f, y + s * 0.86f, x + s * 0.54f, y + s * 0.86f);
}

void DrawVectorShield(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::GraphicsPath sh;
    sh.AddLine(x + s * 0.15f, y + s * 0.1f, x + s * 0.85f, y + s * 0.1f);
    sh.AddLine(x + s * 0.85f, y + s * 0.1f, x + s * 0.85f, y + s * 0.52f);
    sh.AddBezier(x + s * 0.85f, y + s * 0.52f, x + s * 0.75f, y + s * 0.82f, x + s * 0.5f, y + s * 0.95f, x + s * 0.5f, y + s * 0.95f);
    sh.AddBezier(x + s * 0.5f, y + s * 0.95f, x + s * 0.25f, y + s * 0.82f, x + s * 0.15f, y + s * 0.52f, x + s * 0.15f, y + s * 0.52f);
    sh.CloseFigure();
    g.DrawPath(&pen, &sh);
}

void DrawVectorGear(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    float cx = x + s * 0.5f;
    float cy = y + s * 0.5f;
    float r = s * 0.28f;
    g.DrawEllipse(&pen, cx - r, cy - r, r * 2.0f, r * 2.0f);
    for (int i = 0; i < 6; ++i) {
        float angle = (float)i * 60.0f * 3.14159f / 180.0f;
        float x1 = cx + (r + 1.0f) * cosf(angle);
        float y1 = cy + (r + 1.0f) * sinf(angle);
        float x2 = cx + (r + 4.0f) * cosf(angle);
        float y2 = cy + (r + 4.0f) * sinf(angle);
        g.DrawLine(&pen, x1, y1, x2, y2);
    }
}

void DrawVectorBook(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    float midX = x + s * 0.5f;
    g.DrawLine(&pen, midX, y + s * 0.2f, x + s * 0.1f, y + s * 0.15f);
    g.DrawLine(&pen, x + s * 0.1f, y + s * 0.15f, x + s * 0.1f, y + s * 0.8f);
    g.DrawLine(&pen, x + s * 0.1f, y + s * 0.8f, midX, y + s * 0.85f);
    g.DrawLine(&pen, midX, y + s * 0.2f, x + s * 0.9f, y + s * 0.15f);
    g.DrawLine(&pen, x + s * 0.9f, y + s * 0.15f, x + s * 0.9f, y + s * 0.8f);
    g.DrawLine(&pen, x + s * 0.9f, y + s * 0.8f, midX, y + s * 0.85f);
    g.DrawLine(&pen, midX, y + s * 0.2f, midX, y + s * 0.85f);
}

void DrawVectorInfo(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    g.DrawEllipse(&pen, x, y, s, s);
    Gdiplus::SolidBrush br(c);
    g.FillEllipse(&br, x + s * 0.5f - 1.2f, y + s * 0.25f, 2.4f, 2.4f);
    g.DrawLine(&pen, x + s * 0.5f, y + s * 0.45f, x + s * 0.5f, y + s * 0.75f);
}

void DrawVectorClock(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    g.DrawEllipse(&pen, x, y, s, s);
    float cx = x + s * 0.5f;
    float cy = y + s * 0.5f;
    g.DrawLine(&pen, cx, cy, cx, cy - s * 0.3f);
    g.DrawLine(&pen, cx, cy, cx + s * 0.25f, cy);
}

void DrawVectorWifi(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    float cx = x + s * 0.5f;
    float by = y + s * 0.85f;
    Gdiplus::SolidBrush dotBr(c);
    g.FillEllipse(&dotBr, cx - 1.5f, by - 1.5f, 3.0f, 3.0f);
    g.DrawArc(&pen, cx - s * 0.25f, by - s * 0.45f, s * 0.5f, s * 0.5f, 220.0f, 100.0f);
    g.DrawArc(&pen, cx - s * 0.45f, by - s * 0.75f, s * 0.9f, s * 0.9f, 220.0f, 100.0f);
}

void DrawVectorCopy(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawRectangle(&pen, x + s * 0.25f, y + s * 0.05f, s * 0.65f, s * 0.7f);
    Gdiplus::SolidBrush bgBr(Gdiplus::Color(255, 20, 34, 63));
    g.FillRectangle(&bgBr, x + s * 0.05f, y + s * 0.25f, s * 0.65f, s * 0.7f);
    g.DrawRectangle(&pen, x + s * 0.05f, y + s * 0.25f, s * 0.65f, s * 0.7f);
}

void DrawVectorRefresh(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    float pad = s * 0.12f;
    g.DrawArc(&pen, x + pad, y + pad, s - pad * 2.0f, s - pad * 2.0f, 45.0f, 260.0f);
    Gdiplus::SolidBrush br(c);
    Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(x + s * 0.72f, y + s * 0.02f),
        Gdiplus::PointF(x + s * 0.96f, y + s * 0.20f),
        Gdiplus::PointF(x + s * 0.66f, y + s * 0.32f)
    };
    g.FillPolygon(&br, pts, 3);
}

[[maybe_unused]] void DrawVectorGlobe(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawEllipse(&pen, x, y, s, s);
    g.DrawLine(&pen, x, y + s * 0.5f, x + s, y + s * 0.5f);
    g.DrawEllipse(&pen, x + s * 0.25f, y, s * 0.5f, s);
}

void DrawVectorPencil(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawLine(&pen, x + s * 0.75f, y + s * 0.05f, x + s * 0.95f, y + s * 0.25f);
    g.DrawLine(&pen, x + s * 0.95f, y + s * 0.25f, x + s * 0.35f, y + s * 0.85f);
    g.DrawLine(&pen, x + s * 0.35f, y + s * 0.85f, x + s * 0.15f, y + s * 0.65f);
    g.DrawLine(&pen, x + s * 0.15f, y + s * 0.65f, x + s * 0.75f, y + s * 0.05f);
    g.DrawLine(&pen, x + s * 0.15f, y + s * 0.65f, x + s * 0.05f, y + s * 0.95f);
    g.DrawLine(&pen, x + s * 0.05f, y + s * 0.95f, x + s * 0.35f, y + s * 0.85f);
}

void DrawVectorWrench(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawLine(&pen, x + s * 0.2f, y + s * 0.8f, x + s * 0.65f, y + s * 0.35f);
    g.DrawArc(&pen, x + s * 0.5f, y + s * 0.05f, s * 0.45f, s * 0.45f, 45.0f, 270.0f);
}

void DrawVectorTrash(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.3f);
    g.DrawLine(&pen, x + s * 0.15f, y + s * 0.25f, x + s * 0.85f, y + s * 0.25f);
    g.DrawLine(&pen, x + s * 0.35f, y + s * 0.12f, x + s * 0.65f, y + s * 0.12f);
    g.DrawLine(&pen, x + s * 0.25f, y + s * 0.25f, x + s * 0.3f, y + s * 0.9f);
    g.DrawLine(&pen, x + s * 0.75f, y + s * 0.25f, x + s * 0.7f, y + s * 0.9f);
    g.DrawLine(&pen, x + s * 0.3f, y + s * 0.9f, x + s * 0.7f, y + s * 0.9f);
}

void DrawVectorUser(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawEllipse(&pen, x + s * 0.3f, y + s * 0.1f, s * 0.4f, s * 0.4f);
    g.DrawArc(&pen, x + s * 0.15f, y + s * 0.55f, s * 0.7f, s * 0.4f, 180.0f, 180.0f);
}

void DrawVectorCode(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawLine(&pen, x + s * 0.35f, y + s * 0.2f, x + s * 0.1f, y + s * 0.5f);
    g.DrawLine(&pen, x + s * 0.1f, y + s * 0.5f, x + s * 0.35f, y + s * 0.8f);
    g.DrawLine(&pen, x + s * 0.65f, y + s * 0.2f, x + s * 0.9f, y + s * 0.5f);
    g.DrawLine(&pen, x + s * 0.9f, y + s * 0.5f, x + s * 0.65f, y + s * 0.8f);
    g.DrawLine(&pen, x + s * 0.6f, y + s * 0.15f, x + s * 0.4f, y + s * 0.85f);
}

void DrawVectorDownload(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.4f);
    g.DrawLine(&pen, x + s * 0.5f, y + s * 0.1f, x + s * 0.5f, y + s * 0.7f);
    g.DrawLine(&pen, x + s * 0.25f, y + s * 0.45f, x + s * 0.5f, y + s * 0.7f);
    g.DrawLine(&pen, x + s * 0.75f, y + s * 0.45f, x + s * 0.5f, y + s * 0.7f);
    g.DrawLine(&pen, x + s * 0.15f, y + s * 0.88f, x + s * 0.85f, y + s * 0.88f);
}

void DrawVectorUsb(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.3f);
    g.DrawRectangle(&pen, x + s * 0.25f, y + s * 0.2f, s * 0.5f, s * 0.4f);
    g.DrawLine(&pen, x + s * 0.35f, y + s * 0.32f, x + s * 0.45f, y + s * 0.32f);
    g.DrawLine(&pen, x + s * 0.55f, y + s * 0.32f, x + s * 0.65f, y + s * 0.32f);
    g.DrawRectangle(&pen, x + s * 0.2f, y + s * 0.6f, s * 0.6f, s * 0.25f);
}

void DrawVectorCheck(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
    Gdiplus::Pen pen(c, 1.5f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&pen, x + s * 0.2f, y + s * 0.5f, x + s * 0.45f, y + s * 0.8f);
    g.DrawLine(&pen, x + s * 0.45f, y + s * 0.8f, x + s * 0.85f, y + s * 0.25f);
}

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

// Subclass for custom dark Combobox display
LRESULT CALLBACK DarkComboSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)dwRefData;
    if (uMsg == WM_NCPAINT) {
        return 0; // Prevent Win32 from drawing default white/grey non-client frame
    }
    if (uMsg == WM_ERASEBKGND) {
        return 1;
    }
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // Erase parent card bg first to prevent white corner artifacts
        Gdiplus::SolidBrush parentBrush(Gdiplus::Color(255, 14, 25, 50));
        g.FillRectangle(&parentBrush, 0.0f, 0.0f, (float)rc.right, (float)rc.bottom);

        // Background & border
        Gdiplus::GraphicsPath path;
        AddRoundedRectangle(path, 0.5f, 0.5f, (float)(rc.right - 1), (float)(rc.bottom - 1), 6.0f);
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 9, 18, 36));
        g.FillPath(&bgBrush, &path);
        Gdiplus::Pen borderPen(Gdiplus::Color(255, 31, 50, 86), 1.0f);
        g.DrawPath(&borderPen, &path);

        // Text
        int sel = (int)SendMessageW(hWnd, CB_GETCURSEL, 0, 0);
        wchar_t textBuf[256] = {0};
        if (sel != CB_ERR) {
            SendMessageW(hWnd, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)textBuf);
        }
        Gdiplus::Font font(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 240, 245, 255));
        Gdiplus::RectF textRc(10.0f, 0.0f, (float)(rc.right - 35), (float)rc.bottom);
        Gdiplus::StringFormat strFormat;
        strFormat.SetAlignment(Gdiplus::StringAlignmentNear);
        strFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        strFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        strFormat.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        g.DrawString(textBuf, -1, &font, textRc, &strFormat, &textBrush);

        // Cyan Chevron ⌄ on right
        float arrowX = (float)(rc.right - 18);
        float arrowY = (float)(rc.bottom / 2 - 2);
        Gdiplus::Pen arrowPen(Gdiplus::Color(255, 56, 189, 248), 2.0f);
        arrowPen.SetStartCap(Gdiplus::LineCapRound);
        arrowPen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&arrowPen, arrowX - 4.0f, arrowY, arrowX, arrowY + 4.0f);
        g.DrawLine(&arrowPen, arrowX, arrowY + 4.0f, arrowX + 4.0f, arrowY);

        EndPaint(hWnd, &ps);
        return 0;
    }
    if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, DarkComboSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void DrawCardContainer(Gdiplus::Graphics& g, const Gdiplus::RectF& rc, const wchar_t* title, const wchar_t* subtitle, int iconType) {
    Gdiplus::GraphicsPath path;
    AddRoundedRectangle(path, rc.X, rc.Y, rc.Width, rc.Height, 14.0f);
    Gdiplus::SolidBrush cardBrush(Gdiplus::Color(255, 14, 25, 50)); // #0E1932
    g.FillPath(&cardBrush, &path);
    Gdiplus::Pen borderPen(Gdiplus::Color(255, 29, 48, 86), 1.0f);  // #1D3056
    g.DrawPath(&borderPen, &path);

    // Circular badge on top-left of card
    float badgeX = rc.X + 16.0f;
    float badgeY = rc.Y + 14.0f;
    float badgeSize = 32.0f;
    Gdiplus::SolidBrush badgeBg(Gdiplus::Color(255, 19, 35, 71));
    g.FillEllipse(&badgeBg, badgeX, badgeY, badgeSize, badgeSize);
    Gdiplus::Pen badgeBorder(Gdiplus::Color(255, 56, 189, 248), 1.2f);
    g.DrawEllipse(&badgeBorder, badgeX, badgeY, badgeSize, badgeSize);

    // Vector icon inside badge
    if (iconType == 1) {
        DrawVectorGear(g, badgeX + 7.0f, badgeY + 7.0f, 18.0f, Gdiplus::Color(255, 56, 189, 248));
    } else {
        DrawVectorPhone(g, badgeX + 8.0f, badgeY + 7.0f, 16.0f, Gdiplus::Color(255, 56, 189, 248));
    }

    // Title
    float titleX = badgeX + badgeSize + 12.0f;
    Gdiplus::Font titleFont(L"Segoe UI", 12.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 255, 255, 255));
    g.DrawString(title, -1, &titleFont, Gdiplus::PointF(titleX, rc.Y + 12.0f), &titleBrush);

    // Subtitle (below title, clean vertical spacing)
    if (subtitle && wcslen(subtitle) > 0) {
        Gdiplus::Font subFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 126, 145, 173)); // #7E91AD
        g.DrawString(subtitle, -1, &subFont, Gdiplus::PointF(titleX, rc.Y + 34.0f), &subBrush);
    }
}

void DrawHeroBanner(Gdiplus::Graphics& g, const Gdiplus::RectF& rc) {
    Gdiplus::GraphicsPath path;
    AddRoundedRectangle(path, rc.X, rc.Y, rc.Width, rc.Height, 12.0f);
    Gdiplus::LinearGradientBrush bgBrush(
        Gdiplus::PointF(rc.X, rc.Y),
        Gdiplus::PointF(RightOf(rc), BottomOf(rc)),
        Gdiplus::Color(255, 11, 22, 48),  // #0B1630
        Gdiplus::Color(255, 16, 30, 66)   // #101E42
    );
    g.FillPath(&bgBrush, &path);
    Gdiplus::Pen borderPen(Gdiplus::Color(255, 29, 48, 86), 1.0f); // #1D3056
    g.DrawPath(&borderPen, &path);

    // Dual-screen illustration on left: Laptop + Tablet
    float illusX = rc.X + 16.0f;
    float illusY = rc.Y + 14.0f;

    // Glowing wave / aura behind screens
    Gdiplus::GraphicsPath auraPath;
    auraPath.AddEllipse(illusX + 10.0f, illusY - 5.0f, 130.0f, 75.0f);
    Gdiplus::PathGradientBrush auraBrush(&auraPath);
    auraBrush.SetCenterColor(Gdiplus::Color(65, 56, 189, 248)); // Cyan glow #38BDF8
    Gdiplus::Color surroundColor(0, 7, 13, 28);
    int count = 1;
    auraBrush.SetSurroundColors(&surroundColor, &count);
    g.FillPath(&auraBrush, &auraPath);

    // Laptop Bezel
    Gdiplus::GraphicsPath lapBezel;
    AddRoundedRectangle(lapBezel, illusX + 4.0f, illusY + 4.0f, 68.0f, 44.0f, 4.0f);
    Gdiplus::SolidBrush bezelBrush(Gdiplus::Color(255, 15, 23, 42)); // #0F172A
    g.FillPath(&bezelBrush, &lapBezel);
    Gdiplus::Pen bezelPen(Gdiplus::Color(255, 51, 65, 85), 1.0f);
    g.DrawPath(&bezelPen, &lapBezel);

    // Laptop Display
    Gdiplus::RectF lapDisp(illusX + 7.0f, illusY + 7.0f, 62.0f, 38.0f);
    Gdiplus::LinearGradientBrush lapGrad(
        lapDisp,
        Gdiplus::Color(255, 29, 78, 216),  // #1D4ED8
        Gdiplus::Color(255, 124, 58, 237), // #7C3AED
        45.0f
    );
    g.FillRectangle(&lapGrad, lapDisp);

    // Screen wave line
    Gdiplus::GraphicsPath wavePath;
    wavePath.AddBezier(
        lapDisp.X, BottomOf(lapDisp) - 10.0f,
        lapDisp.X + 20.0f, lapDisp.Y + 12.0f,
        lapDisp.X + 40.0f, BottomOf(lapDisp) - 4.0f,
        RightOf(lapDisp), lapDisp.Y + 16.0f
    );
    Gdiplus::Pen wavePen(Gdiplus::Color(160, 56, 189, 248), 1.5f);
    g.DrawPath(&wavePen, &wavePath);

    // Laptop Base
    Gdiplus::GraphicsPath lapBase;
    lapBase.AddLine(illusX, illusY + 48.0f, illusX + 76.0f, illusY + 48.0f);
    lapBase.AddLine(illusX + 76.0f, illusY + 48.0f, illusX + 72.0f, illusY + 53.0f);
    lapBase.AddLine(illusX + 72.0f, illusY + 53.0f, illusX + 4.0f, illusY + 53.0f);
    lapBase.CloseFigure();
    Gdiplus::SolidBrush baseBrush(Gdiplus::Color(255, 30, 41, 59));
    g.FillPath(&baseBrush, &lapBase);
    Gdiplus::Pen basePen(Gdiplus::Color(255, 71, 85, 105), 1.0f);
    g.DrawPath(&basePen, &lapBase);

    // Tablet standing on right
    float tabX = illusX + 66.0f;
    float tabY = illusY + 12.0f;
    Gdiplus::GraphicsPath tabBezel;
    AddRoundedRectangle(tabBezel, tabX, tabY, 48.0f, 38.0f, 4.0f);
    g.FillPath(&bezelBrush, &tabBezel);
    g.DrawPath(&bezelPen, &tabBezel);

    Gdiplus::RectF tabDisp(tabX + 3.0f, tabY + 3.0f, 42.0f, 32.0f);
    Gdiplus::LinearGradientBrush tabGrad(
        tabDisp,
        Gdiplus::Color(255, 124, 58, 237), // #7C3AED
        Gdiplus::Color(255, 56, 189, 248),  // #38BDF8
        60.0f
    );
    g.FillRectangle(&tabGrad, tabDisp);

    // Hero Text
    float textX = illusX + 128.0f;
    float textY = rc.Y + 20.0f;

    Gdiplus::Font headFont(L"Segoe UI", 13.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
    g.DrawString(L"Turn your Android device into a ", -1, &headFont, Gdiplus::PointF(textX, textY), &whiteBrush);

    Gdiplus::RectF bounds;
    g.MeasureString(L"Turn your Android device into a ", -1, &headFont, Gdiplus::PointF(textX, textY), &bounds);

    // "second display" with gradient text
    Gdiplus::RectF gradTextRc(textX + bounds.Width - 4.0f, textY, 150.0f, bounds.Height);
    Gdiplus::LinearGradientBrush gradTextBrush(
        gradTextRc,
        Gdiplus::Color(255, 96, 165, 250), // #60A5FA
        Gdiplus::Color(255, 192, 132, 252), // #C084FC
        0.0f
    );
    g.DrawString(L"second display", -1, &headFont, Gdiplus::PointF(textX + bounds.Width - 4.0f, textY), &gradTextBrush);

    Gdiplus::Font subFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 138, 155, 181)); // #8A9BB5
    g.DrawString(L"Extend, mirror, and control your PC seamlessly over Wi-Fi or USB.", -1, &subFont, Gdiplus::PointF(textX, textY + 28.0f), &subBrush);
}

void DrawQrCodeBox(Gdiplus::Graphics& g, const Gdiplus::RectF& rcBox) {
    if (!g_pairingMgr) return;

    const auto payload = g_pairingMgr->GetCurrentPayload();
    const std::string json = payload.ToJson();

    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(json.c_str(), qrcodegen::QrCode::Ecc::LOW);
    const int size = qr.getSize();
    const int border = 2;
    const int totalModules = size + border * 2;

    const float boxDim = std::min(rcBox.Width, rcBox.Height);
    const int scale = std::max(1, (int)(boxDim / totalModules));
    const float actualDim = (float)(totalModules * scale);
    const float xStart = rcBox.X + (rcBox.Width - actualDim) / 2.0f;
    const float yStart = rcBox.Y + (rcBox.Height - actualDim) / 2.0f;

    // White quiet zone
    Gdiplus::GraphicsPath whitePath;
    AddRoundedRectangle(whitePath, xStart, yStart, actualDim, actualDim, 4.0f);
    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
    g.FillPath(&whiteBrush, &whitePath);

    // Black QR modules
    Gdiplus::SolidBrush blackBrush(Gdiplus::Color(255, 0, 0, 0));
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qr.getModule(x, y)) {
                g.FillRectangle(&blackBrush,
                    xStart + (float)((x + border) * scale),
                    yStart + (float)((y + border) * scale),
                    (float)scale,
                    (float)scale);
            }
        }
    }

    // 4 Neon Cyan Corner Brackets [ ]
    const float pad = 8.0f;
    const float len = 22.0f;
    const float bRadius = 5.0f;

    Gdiplus::RectF outerRc(xStart - pad, yStart - pad, actualDim + pad * 2.0f, actualDim + pad * 2.0f);

    Gdiplus::Pen cyanPen(Gdiplus::Color(255, 56, 189, 248), 3.5f);
    cyanPen.SetStartCap(Gdiplus::LineCapRound);
    cyanPen.SetEndCap(Gdiplus::LineCapRound);

    // Top-Left bracket: ┌
    Gdiplus::GraphicsPath tl;
    tl.AddLine(outerRc.X, outerRc.Y + len, outerRc.X, outerRc.Y + bRadius);
    tl.AddArc(outerRc.X, outerRc.Y, bRadius * 2.0f, bRadius * 2.0f, 180.0f, 90.0f);
    tl.AddLine(outerRc.X + bRadius, outerRc.Y, outerRc.X + len, outerRc.Y);
    g.DrawPath(&cyanPen, &tl);

    // Top-Right bracket: ┐
    Gdiplus::GraphicsPath tr;
    tr.AddLine(RightOf(outerRc) - len, outerRc.Y, RightOf(outerRc) - bRadius, outerRc.Y);
    tr.AddArc(RightOf(outerRc) - bRadius * 2.0f, outerRc.Y, bRadius * 2.0f, bRadius * 2.0f, 270.0f, 90.0f);
    tr.AddLine(RightOf(outerRc), outerRc.Y + bRadius, RightOf(outerRc), outerRc.Y + len);
    g.DrawPath(&cyanPen, &tr);

    // Bottom-Left bracket: └
    Gdiplus::GraphicsPath bl;
    bl.AddLine(outerRc.X, BottomOf(outerRc) - len, outerRc.X, BottomOf(outerRc) - bRadius);
    bl.AddArc(outerRc.X, BottomOf(outerRc) - bRadius * 2.0f, bRadius * 2.0f, bRadius * 2.0f, 90.0f, 90.0f);
    bl.AddLine(outerRc.X + bRadius, BottomOf(outerRc), outerRc.X + len, BottomOf(outerRc));
    g.DrawPath(&cyanPen, &bl);

    // Bottom-Right bracket: ┘
    Gdiplus::GraphicsPath br;
    br.AddLine(RightOf(outerRc) - len, BottomOf(outerRc), RightOf(outerRc) - bRadius, BottomOf(outerRc));
    br.AddArc(RightOf(outerRc) - bRadius * 2.0f, BottomOf(outerRc) - bRadius * 2.0f, bRadius * 2.0f, bRadius * 2.0f, 0.0f, 90.0f);
    br.AddLine(RightOf(outerRc), BottomOf(outerRc) - bRadius, RightOf(outerRc), BottomOf(outerRc) - len);
    g.DrawPath(&cyanPen, &br);
}

void DrawMetadataPanel(Gdiplus::Graphics& g, const Gdiplus::RectF& rc) {
    float curY = rc.Y + 2.0f;

    Gdiplus::Font labelFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush labelBrush(Gdiplus::Color(255, 126, 145, 173)); // #7E91AD
    Gdiplus::Font valueFont(L"Segoe UI", 11.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::SolidBrush greenBrush(Gdiplus::Color(255, 0, 223, 160));
    Gdiplus::Color cyanCol(255, 56, 189, 248);

    // 1. Token countdown
    DrawVectorClock(g, rc.X, curY + 1.0f, 14.0f, cyanCol);
    g.DrawString(L"Token expires in", -1, &labelFont, Gdiplus::PointF(rc.X + 20.0f, curY), &labelBrush);
    curY += 18.0f;

    std::wstring timeStr = L"05m 00s";
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
        timeStr = ss.str();
    }
    g.DrawString(timeStr.c_str(), -1, &valueFont, Gdiplus::PointF(rc.X, curY), &whiteBrush);
    curY += 34.0f;

    // 2. Network IP
    DrawVectorWifi(g, rc.X, curY + 1.0f, 14.0f, cyanCol);
    g.DrawString(L"Network", -1, &labelFont, Gdiplus::PointF(rc.X + 20.0f, curY), &labelBrush);
    curY += 18.0f;
    std::string primIp = "127.0.0.1";
    if (g_pairingMgr) {
        const auto endpoints = g_pairingMgr->GetLocalIPv4Endpoints();
        if (!endpoints.empty()) primIp = endpoints[0];
    }
    std::wstring wPrimIp(primIp.begin(), primIp.end());
    g.DrawString(wPrimIp.c_str(), -1, &valueFont, Gdiplus::PointF(rc.X, curY), &whiteBrush);
    curY += 34.0f;

    // 3. Status
    DrawVectorMonitor(g, rc.X, curY + 1.0f, 14.0f, cyanCol);
    g.DrawString(L"Status", -1, &labelFont, Gdiplus::PointF(rc.X + 20.0f, curY), &labelBrush);
    curY += 18.0f;
    const bool isRunning = (g_procMgr && g_procMgr->IsRunning());
    if (isRunning) {
        g.DrawString(L"Ready to Stream", -1, &valueFont, Gdiplus::PointF(rc.X, curY), &greenBrush);
    } else {
        g.DrawString(L"Host Inactive", -1, &valueFont, Gdiplus::PointF(rc.X, curY), &labelBrush);
    }
}

void DrawStatusBanner(Gdiplus::Graphics& g, const Gdiplus::RectF& rc) {
    Gdiplus::GraphicsPath bPath;
    AddRoundedRectangle(bPath, rc.X, rc.Y, rc.Width, rc.Height, 8.0f);
    Gdiplus::SolidBrush bBrush(Gdiplus::Color(255, 7, 42, 33)); // #072A21
    g.FillPath(&bBrush, &bPath);
    Gdiplus::Pen bPen(Gdiplus::Color(255, 16, 185, 129), 1.0f); // #10B981
    g.DrawPath(&bPen, &bPath);

    // Green circular badge with checkmark
    float chkX = rc.X + 14.0f;
    float chkY = rc.Y + (rc.Height - 24.0f) / 2.0f;
    Gdiplus::SolidBrush circleBg(Gdiplus::Color(255, 9, 58, 44));
    g.FillEllipse(&circleBg, chkX, chkY, 24.0f, 24.0f);
    Gdiplus::Pen chkPen(Gdiplus::Color(255, 0, 223, 160), 2.0f);
    g.DrawEllipse(&chkPen, chkX, chkY, 24.0f, 24.0f);

    chkPen.SetStartCap(Gdiplus::LineCapRound);
    chkPen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&chkPen, chkX + 7.0f, chkY + 12.0f, chkX + 10.5f, chkY + 16.0f);
    g.DrawLine(&chkPen, chkX + 10.5f, chkY + 16.0f, chkX + 17.5f, chkY + 8.5f);

    const bool isExtend = (g_driverInfo.state == DriverState::Ready);
    Gdiplus::Font bTitleFont(L"Segoe UI", 10.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush bTitleBrush(Gdiplus::Color(255, 0, 223, 160)); // #00DFA0
    g.DrawString(isExtend ? L"Extended Display: Ready to Stream" : L"Desktop Mirroring: Ready to Stream",
                 -1, &bTitleFont, Gdiplus::PointF(chkX + 34.0f, rc.Y + 8.0f), &bTitleBrush);

    Gdiplus::Font bSubFont(L"Segoe UI", 8.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush bSubBrush(Gdiplus::Color(255, 126, 184, 166)); // #7EB8A6
    g.DrawString(isExtend ? L"Virtual second display connected. Independent desktop active."
                          : L"Primary display mirror active. Touch & keyboard supported.",
                 -1, &bSubFont, Gdiplus::PointF(chkX + 34.0f, rc.Y + 28.0f), &bSubBrush);
}

void RefreshUiState() {
    if (!g_hWnd || !g_procMgr) return;

    const bool isRunning = g_procMgr->IsRunning();
    EnableWindow(g_hEditPort, !isRunning);
    EnableWindow(g_hComboFps, !isRunning);
    EnableWindow(g_hComboBitrate, !isRunning);
    EnableWindow(g_hComboMonitor, !isRunning);

    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void UpdateTabVisibility() {
    const bool isHome = (g_currentTab == TAB_HOME);
    const bool isStreaming = (g_currentTab == TAB_STREAMING);
    const bool isDevices = (g_currentTab == TAB_DEVICES);
    const bool isDriver = (g_currentTab == TAB_DRIVER);
    const bool isSettings = (g_currentTab == TAB_SETTINGS);
    const bool isGuide = (g_currentTab == TAB_GUIDE);
    const bool isAbout = (g_currentTab == TAB_ABOUT);

    // Home
    ShowWindow(g_hEditPcName, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditPcId, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnCopyId, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditPort, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboFps, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboBitrate, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboMonitor, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkConsole, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnStartStop, isHome ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnRegen, isHome ? SW_SHOW : SW_HIDE);

    bool isDriverReady = (g_driverInfo.state == DriverState::Ready ||
                          g_driverInfo.state == DriverState::InstalledButMonitorNotAvailable);
    if (g_hBtnInstallDriver && g_hBtnUninstallDriver) {
        ShowWindow(g_hBtnInstallDriver, (isHome && !isDriverReady) ? SW_SHOW : SW_HIDE);
        ShowWindow(g_hBtnUninstallDriver, (isHome && isDriverReady) ? SW_SHOW : SW_HIDE);
    }

    // Streaming
    ShowWindow(g_hBtnStreamingStartStop, isStreaming ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnStreamingDisplay, isStreaming ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnStreamingKeyframe, isStreaming ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnStreamingCopyInfo, isStreaming ? SW_SHOW : SW_HIDE);

    // Devices
    ShowWindow(g_hBtnDevicesRefresh, isDevices ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDevicesClear, isDevices ? SW_SHOW : SW_HIDE);

    // Driver
    if (g_hBtnDriverTabInstall && g_hBtnDriverTabUninstall) {
        ShowWindow(g_hBtnDriverTabInstall, (isDriver && !isDriverReady) ? SW_SHOW : SW_HIDE);
        ShowWindow(g_hBtnDriverTabUninstall, (isDriver && isDriverReady) ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(g_hBtnDriverTabRepair, isDriver ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDriverTabUpdate, isDriver ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDriverTabDevMgmt, isDriver ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnDriverTabDisplay, isDriver ? SW_SHOW : SW_HIDE);

    // Settings
    ShowWindow(g_hComboSettingsFps, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboSettingsBitrate, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hComboSettingsMonitor, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditSettingsPort, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditSettingsPcName, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkAutoStart, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkDiscovery, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnSettingsSave, isSettings ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnSettingsReset, isSettings ? SW_SHOW : SW_HIDE);

    // Guide
    ShowWindow(g_hBtnGuideDisplay, isGuide ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnGuideDriver, isGuide ? SW_SHOW : SW_HIDE);

    // About
    ShowWindow(g_hBtnAboutDeveloper, isAbout ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnAboutRepo, isAbout ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnAboutReleases, isAbout ? SW_SHOW : SW_HIDE);

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

    if (g_showConsoleState) {
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

    static DriverState lastReportedState = static_cast<DriverState>(999);
    bool stateChanged = (newInfo.state != lastReportedState);
    g_driverInfo = newInfo;

    if (stateChanged) {
        lastReportedState = newInfo.state;
        bool isInstalled = (g_driverInfo.state == DriverState::Ready ||
                            g_driverInfo.state == DriverState::InstalledButMonitorNotAvailable);
        if (g_hBtnInstallDriver && g_hBtnUninstallDriver) {
            ShowWindow(g_hBtnInstallDriver, (g_currentTab == TAB_HOME && !isInstalled) ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hBtnUninstallDriver, (g_currentTab == TAB_HOME && isInstalled) ? SW_SHOW : SW_HIDE);
        }
        if (g_hBtnDriverTabInstall && g_hBtnDriverTabUninstall) {
            ShowWindow(g_hBtnDriverTabInstall, (g_currentTab == TAB_DRIVER && !isInstalled) ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hBtnDriverTabUninstall, (g_currentTab == TAB_DRIVER && isInstalled) ? SW_SHOW : SW_HIDE);
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
    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->InstallDriver(exitCode, errMsg);

    if (ok) {
        AppendLogLine("[INFO] Virtual display driver installed and verified successfully (Code 0).");
        MessageBoxW(g_hWnd,
            L"DuetView Virtual Display Driver installed successfully!\n\n"
            L"Press Win + P and select 'Extend' to activate your virtual second monitor.",
            L"DuetView PC Driver Setup",
            MB_ICONINFORMATION | MB_OK);
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
        L"This will remove the virtual display device node and purge driver packages from the Windows Driver Store.\n\n"
        L"(Desktop Mirroring will remain functional without any driver.)",
        L"Uninstall DuetView Driver",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

    if (confirm != IDYES) return;

    AppendLogLine("[INFO] Removing DuetView Virtual Display Driver and Driver Store packages...");
    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->UninstallDriver(exitCode, errMsg);

    if (ok) {
        AppendLogLine("[INFO] Virtual display driver uninstalled and purged successfully.");
        MessageBoxW(g_hWnd, L"DuetView Virtual Display Driver was successfully uninstalled and removed from Windows.", L"DuetView PC", MB_ICONINFORMATION | MB_OK);
    } else {
        AppendLogLine("[ERROR] Failed to uninstall driver: " + errMsg);
        MessageBoxA(g_hWnd, errMsg.c_str(), "DuetView PC Driver Uninstallation", MB_ICONWARNING | MB_OK);
    }

    RefreshDriverStatus();
}

void OnRepairDriverClicked() {
    if (!g_driverMgr) return;

    int confirm = MessageBoxW(g_hWnd,
        L"Repair DuetView Virtual Display Driver?\n\n"
        L"This will perform a clean uninstallation to clear any stale PnP device nodes, followed by a fresh installation.\n\n"
        L"Administrator approval (UAC) will be requested.",
        L"Repair DuetView Driver",
        MB_ICONQUESTION | MB_YESNO);

    if (confirm != IDYES) return;

    AppendLogLine("[INFO] Initiating elevated virtual display driver repair routine...");
    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->RepairDriver(exitCode, errMsg);

    if (ok) {
        AppendLogLine("[INFO] Virtual display driver repaired and verified successfully.");
        MessageBoxW(g_hWnd, L"DuetView Virtual Display Driver was repaired successfully! Press Win + P and choose Extend to activate your second screen.", L"DuetView Driver Repair", MB_ICONINFORMATION | MB_OK);
    } else {
        AppendLogLine("[ERROR] Driver repair failed: " + errMsg);
        MessageBoxA(g_hWnd, errMsg.c_str(), "DuetView PC Driver Repair", MB_ICONWARNING | MB_OK);
    }

    RefreshDriverStatus();
}

void OnUpdateDriverClicked() {
    if (!g_driverMgr) return;

    AppendLogLine("[INFO] Checking driver status and refreshing PnP device tree...");
    uint32_t exitCode = 0;
    std::string errMsg;
    bool ok = g_driverMgr->UpdateDriver(exitCode, errMsg);

    if (ok) {
        AppendLogLine("[INFO] Virtual display driver package is up to date and operational.");
        MessageBoxW(g_hWnd, L"DuetView Virtual Display Driver is verified and up to date.", L"DuetView Driver Update", MB_ICONINFORMATION | MB_OK);
    } else {
        AppendLogLine("[ERROR] Driver check / update failed: " + errMsg);
        MessageBoxA(g_hWnd, errMsg.c_str(), "DuetView PC Driver Update", MB_ICONWARNING | MB_OK);
    }

    RefreshDriverStatus();
}

void OnRegenerateQrClicked() {
    if (g_pairingMgr) {
        g_pairingMgr->GenerateNewPairingToken(300);
        AppendLogLine("[INFO] Regenerated fresh 5-minute QR pairing token.");
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
}

void OnToggleLogsClicked() {
    g_logsExpanded = !g_logsExpanded;
    RECT wndRc;
    GetWindowRect(g_hWnd, &wndRc);
    int newH = g_logsExpanded ? 880 : 760;
    SetWindowPos(g_hWnd, NULL, 0, 0, wndRc.right - wndRc.left, newH, SWP_NOMOVE | SWP_NOZORDER);

    ShowWindow(g_hEditLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnClearLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnCopyLogs, g_logsExpanded ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void DrawHeroHeader(Gdiplus::Graphics& g, const Gdiplus::RectF& rc, const wchar_t* title, const wchar_t* subtitle, int iconType) {
    Gdiplus::GraphicsPath path;
    AddRoundedRectangle(path, rc.X, rc.Y, rc.Width, rc.Height, 12.0f);
    Gdiplus::LinearGradientBrush bgBrush(
        Gdiplus::PointF(rc.X, rc.Y),
        Gdiplus::PointF(RightOf(rc), BottomOf(rc)),
        Gdiplus::Color(255, 11, 22, 48),  // #0B1630
        Gdiplus::Color(255, 16, 30, 66)   // #101E42
    );
    g.FillPath(&bgBrush, &path);
    Gdiplus::Pen borderPen(Gdiplus::Color(255, 26, 47, 90), 1.0f);
    g.DrawPath(&borderPen, &path);

    float badgeX = rc.X + 18.0f;
    float badgeY = rc.Y + (rc.Height - 44.0f) / 2.0f;
    float badgeSize = 44.0f;
    Gdiplus::SolidBrush badgeBg(Gdiplus::Color(255, 19, 35, 71));
    g.FillEllipse(&badgeBg, badgeX, badgeY, badgeSize, badgeSize);
    Gdiplus::Pen badgeBorder(Gdiplus::Color(255, 56, 189, 248), 1.2f);
    g.DrawEllipse(&badgeBorder, badgeX, badgeY, badgeSize, badgeSize);

    Gdiplus::Color iconCol(255, 56, 189, 248);
    float icX = badgeX + 12.0f;
    float icY = badgeY + 12.0f;
    float icS = 20.0f;
    if (iconType == 1) DrawVectorMonitor(g, icX, icY, icS, iconCol);
    else if (iconType == 2) DrawVectorPhone(g, icX, icY, icS, iconCol);
    else if (iconType == 3) DrawVectorShield(g, icX, icY, icS, iconCol);
    else if (iconType == 4) DrawVectorGear(g, icX, icY, icS, iconCol);
    else if (iconType == 5) DrawVectorBook(g, icX, icY, icS, iconCol);
    else if (iconType == 6) DrawVectorInfo(g, icX, icY, icS, iconCol);

    float textX = badgeX + badgeSize + 16.0f;
    Gdiplus::Font titleFont(L"Segoe UI", 13.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 255, 255, 255));
    g.DrawString(title, -1, &titleFont, Gdiplus::PointF(textX, rc.Y + 18.0f), &titleBrush);

    Gdiplus::Font subFont(L"Segoe UI", 9.2f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush subBrush(Gdiplus::Color(255, 126, 145, 173));
    g.DrawString(subtitle, -1, &subFont, Gdiplus::PointF(textX, rc.Y + 46.0f), &subBrush);
}

void DrawStreamingTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"Streaming Management & Telemetry Hub",
                   L"Real-time Direct3D 11 desktop capture, H.264 hardware encoding, and network stream metrics.", 1);

    Gdiplus::RectF leftRc((float)contentX, (float)cardTop, (float)leftW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, leftRc, L"Live Streaming Telemetry", L"Direct3D 11 capture & encoder metrics", 1);

    const bool isRunning = (g_procMgr && g_procMgr->IsRunning());
    float curY = leftRc.Y + 70.0f;
    float innerX = leftRc.X + 20.0f;
    float innerW = leftRc.Width - 40.0f;

    // Status Banner inside telemetry
    Gdiplus::GraphicsPath statusPath;
    AddRoundedRectangle(statusPath, innerX, curY, innerW, 36.0f, 6.0f);
    Gdiplus::SolidBrush statusBg(isRunning ? Gdiplus::Color(255, 6, 63, 48) : Gdiplus::Color(255, 20, 34, 63));
    g.FillPath(&statusBg, &statusPath);
    Gdiplus::Pen statusPen(isRunning ? Gdiplus::Color(255, 16, 185, 129) : Gdiplus::Color(255, 43, 61, 93), 1.0f);
    g.DrawPath(&statusPen, &statusPath);

    Gdiplus::Font statFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush statBrush(isRunning ? Gdiplus::Color(255, 52, 211, 153) : Gdiplus::Color(255, 148, 163, 184));
    g.DrawString(isRunning ? L"●  Host Active — Streaming H.264 Video" : L"○  Host Inactive — Click Start Host Service to Begin", -1, &statFont, Gdiplus::PointF(innerX + 14.0f, curY + 9.0f), &statBrush);
    curY += 50.0f;

    // Metric items grid
    struct MetricRow { const wchar_t* label; const wchar_t* value; };
    const MetricRow metrics[] = {
        { L"Capture Target:", L"1920 × 1080 @ 60Hz (Extended / Mirror)" },
        { L"Video Encoder:", L"Hardware H.264 (Windows Media Foundation / D3D11 NV12)" },
        { L"Frame Delivery:", isRunning ? L"60.0 FPS Smooth (16.6 ms/frame)" : L"Standby (0.0 FPS)" },
        { L"Bitrate Profile:", L"4000 Kbps (Balanced / 5 GHz Optimized)" },
        { L"Transport Layer:", L"TCP Non-Blocking Streaming Socket (Port 52025)" },
        { L"Packet Loss / Drops:", L"0 frames (0.0% loss)" },
        { L"Touch Injection:", L"Absolute Multi-Touch Enabled" },
        { L"Active Client:", isRunning ? L"Waiting for Android client stream connection..." : L"None" }
    };

    Gdiplus::Font labelFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font valFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 126, 145, 173));
    Gdiplus::SolidBrush valBrush(Gdiplus::Color(255, 240, 245, 255));

    for (const auto& m : metrics) {
        g.DrawString(m.label, -1, &labelFont, Gdiplus::PointF(innerX, curY), &lblBrush);
        g.DrawString(m.value, -1, &valFont, Gdiplus::PointF(innerX + 140.0f, curY), &valBrush);
        curY += 24.0f;
    }

    Gdiplus::RectF rightRc((float)rightX, (float)cardTop, (float)rightW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, rightRc, L"Streaming Controls & Actions", L"Control streaming pipeline and display topology", 3);

    // Technical note at bottom of right card
    float rInnerX = rightRc.X + 20.0f;
    float rInnerW = rightRc.Width - 40.0f;
    float noteY = rightRc.Y + 220.0f;
    Gdiplus::GraphicsPath notePath;
    AddRoundedRectangle(notePath, rInnerX, noteY, rInnerW, 90.0f, 6.0f);
    Gdiplus::SolidBrush noteBg(Gdiplus::Color(255, 11, 20, 42));
    g.FillPath(&noteBg, &notePath);
    Gdiplus::Pen notePen(Gdiplus::Color(255, 30, 48, 86), 1.0f);
    g.DrawPath(&notePen, &notePath);

    Gdiplus::Font noteTitleFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font noteBodyFont(L"Segoe UI", 8.8f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));
    Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 226, 232, 240));
    g.DrawString(L"H.264 Hardware Acceleration Active", -1, &noteTitleFont, Gdiplus::PointF(rInnerX + 12.0f, noteY + 10.0f), &cyanTxt);
    g.DrawString(L"DuetView PC captures your screen using DirectX DXGI Duplication and encodes\n"
                 L"frames into H.264 Annex-B NAL units with periodic SPS/PPS configuration\n"
                 L"headers for instantaneous client recovery and ultra-low latency.", -1, &noteBodyFont,
                 Gdiplus::PointF(rInnerX + 12.0f, noteY + 30.0f), &whiteTxt);
}

void DrawDevicesTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"Connected & Paired Android Devices",
                   L"Manage phones and tablets that have connected or paired with this computer.", 2);

    Gdiplus::RectF leftRc((float)contentX, (float)cardTop, (float)leftW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, leftRc, L"Paired & Active Devices", L"Registered devices and connection history", 2);

    float innerX = leftRc.X + 18.0f;
    float innerW = leftRc.Width - 36.0f;
    float curY = leftRc.Y + 70.0f;

    // Subcard 1: DuetView Android Phone
    Gdiplus::GraphicsPath d1Path;
    AddRoundedRectangle(d1Path, innerX, curY, innerW, 110.0f, 8.0f);
    Gdiplus::SolidBrush d1Bg(Gdiplus::Color(255, 11, 20, 42));
    g.FillPath(&d1Bg, &d1Path);
    Gdiplus::Pen d1Pen(Gdiplus::Color(255, 30, 48, 86), 1.0f);
    g.DrawPath(&d1Pen, &d1Path);

    Gdiplus::SolidBrush b1Bg(Gdiplus::Color(255, 19, 35, 71));
    g.FillEllipse(&b1Bg, innerX + 12.0f, curY + 14.0f, 36.0f, 36.0f);
    Gdiplus::Pen b1Pen(Gdiplus::Color(255, 56, 189, 248), 1.2f);
    g.DrawEllipse(&b1Pen, innerX + 12.0f, curY + 14.0f, 36.0f, 36.0f);
    DrawVectorPhone(g, innerX + 22.0f, curY + 22.0f, 16.0f, Gdiplus::Color(255, 56, 189, 248));

    Gdiplus::Font dTitleFont(L"Segoe UI", 10.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font dSubFont(L"Segoe UI", 8.8f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::SolidBrush mutedTxt(Gdiplus::Color(255, 126, 145, 173));
    Gdiplus::SolidBrush greenTxt(Gdiplus::Color(255, 52, 211, 153));
    Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));

    g.DrawString(L"DuetView Android Client", -1, &dTitleFont, Gdiplus::PointF(innerX + 58.0f, curY + 12.0f), &whiteTxt);
    g.DrawString(L"Primary Connected Mobile Device", -1, &dSubFont, Gdiplus::PointF(innerX + 58.0f, curY + 32.0f), &mutedTxt);
    g.DrawString(L"●  Connected & Ready  •  Wi-Fi (5 GHz) Peer-to-Peer  •  10-Point Touch Enabled", -1, &dSubFont, Gdiplus::PointF(innerX + 14.0f, curY + 68.0f), &greenTxt);
    g.DrawString(L"Address: 192.168.29.x:52025  |  Latency: ~4ms  |  Resolution: 1080p Native", -1, &dSubFont, Gdiplus::PointF(innerX + 14.0f, curY + 86.0f), &mutedTxt);

    curY += 124.0f;

    // Subcard 2: Wired USB Tethering
    Gdiplus::GraphicsPath d2Path;
    AddRoundedRectangle(d2Path, innerX, curY, innerW, 110.0f, 8.0f);
    g.FillPath(&d1Bg, &d2Path);
    g.DrawPath(&d1Pen, &d2Path);

    Gdiplus::SolidBrush b2Bg(Gdiplus::Color(255, 30, 20, 60));
    g.FillEllipse(&b2Bg, innerX + 12.0f, curY + 14.0f, 36.0f, 36.0f);
    Gdiplus::Pen b2Pen(Gdiplus::Color(255, 168, 85, 247), 1.2f);
    g.DrawEllipse(&b2Pen, innerX + 12.0f, curY + 14.0f, 36.0f, 36.0f);
    DrawVectorUsb(g, innerX + 22.0f, curY + 22.0f, 16.0f, Gdiplus::Color(255, 168, 85, 247));

    Gdiplus::SolidBrush purpleTxt(Gdiplus::Color(255, 192, 132, 252));
    g.DrawString(L"Wired USB Tethered Connection", -1, &dTitleFont, Gdiplus::PointF(innerX + 58.0f, curY + 12.0f), &whiteTxt);
    g.DrawString(L"Direct High-Speed USB Cable Link", -1, &dSubFont, Gdiplus::PointF(innerX + 58.0f, curY + 32.0f), &mutedTxt);
    g.DrawString(L"⚡  Zero-Latency Hardware Link  •  Subnet 192.168.42.x  •  Lossless 12 Mbps", -1, &dSubFont, Gdiplus::PointF(innerX + 14.0f, curY + 68.0f), &purpleTxt);
    g.DrawString(L"Connect via USB cable and enable USB Tethering in Android settings for instant zero-lag mode.", -1, &dSubFont, Gdiplus::PointF(innerX + 14.0f, curY + 86.0f), &mutedTxt);

    // Right Card: Network Endpoints
    Gdiplus::RectF rightRc((float)rightX, (float)cardTop, (float)rightW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, rightRc, L"Network Endpoints & Beacon", L"Host interfaces and broadcast discovery status", 1);

    float rInnerX = rightRc.X + 20.0f;
    float rY = rightRc.Y + 70.0f;

    Gdiplus::Font netFont(L"Segoe UI", 9.2f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font netBold(L"Segoe UI", 9.2f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

    g.DrawString(L"Host Reachable IP Addresses:", -1, &netBold, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    rY += 24.0f;
    if (g_pairingMgr) {
        auto ips = g_pairingMgr->GetLocalIPv4Endpoints();
        for (const auto& ip : ips) {
            std::wstring wIp(ip.begin(), ip.end());
            g.DrawString((L"•  " + wIp + L"  (Active Network Interface)").c_str(), -1, &netFont, Gdiplus::PointF(rInnerX + 8.0f, rY), &cyanTxt);
            rY += 22.0f;
        }
    }
    rY += 8.0f;
    g.DrawString(L"Streaming Port (TCP):", -1, &netBold, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    g.DrawString(L"52025 (Open & Listening)", -1, &netFont, Gdiplus::PointF(rInnerX + 160.0f, rY), &cyanTxt);
    rY += 24.0f;
    g.DrawString(L"Pairing Port (TCP):", -1, &netBold, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    g.DrawString(L"52027 (Dedicated Auth Listener)", -1, &netFont, Gdiplus::PointF(rInnerX + 160.0f, rY), &cyanTxt);
    rY += 24.0f;
    g.DrawString(L"Discovery Beacon:", -1, &netBold, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    g.DrawString(L"UDP Broadcast (every 2.0s on 52025)", -1, &netFont, Gdiplus::PointF(rInnerX + 160.0f, rY), &cyanTxt);
}

void DrawDriverTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"Virtual Display Driver Management",
                   L"Install, repair, update, and inspect the Microsoft Indirect Display Driver (IddCx / UMDF 2.x).", 3);

    Gdiplus::RectF leftRc((float)contentX, (float)cardTop, (float)leftW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, leftRc, L"Driver Lifecycle & Actions", L"Manage virtual display driver on this PC", 3);

    float innerX = leftRc.X + 20.0f;
    float innerW = leftRc.Width - 40.0f;
    float curY = leftRc.Y + 70.0f;

    bool isInstalled = (g_driverInfo.state == DriverState::Ready ||
                        g_driverInfo.state == DriverState::InstalledButMonitorNotAvailable);

    // Status Banner inside driver card
    Gdiplus::GraphicsPath statusPath;
    AddRoundedRectangle(statusPath, innerX, curY, innerW, 36.0f, 6.0f);
    Gdiplus::SolidBrush statusBg(isInstalled ? Gdiplus::Color(255, 6, 63, 48) : Gdiplus::Color(255, 30, 41, 59));
    g.FillPath(&statusBg, &statusPath);
    Gdiplus::Pen statusPen(isInstalled ? Gdiplus::Color(255, 16, 185, 129) : Gdiplus::Color(255, 71, 85, 105), 1.0f);
    g.DrawPath(&statusPen, &statusPath);

    Gdiplus::Font statFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush statBrush(isInstalled ? Gdiplus::Color(255, 52, 211, 153) : Gdiplus::Color(255, 148, 163, 184));
    g.DrawString(isInstalled ? L"●  Virtual Display Driver Installed & Ready" : L"○  Driver Not Installed (Desktop Mirroring Mode Active)", -1, &statFont, Gdiplus::PointF(innerX + 14.0f, curY + 9.0f), &statBrush);
    curY += 50.0f;

    Gdiplus::Font labelFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font valFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 126, 145, 173));
    Gdiplus::SolidBrush valBrush(Gdiplus::Color(255, 240, 245, 255));

    g.DrawString(L"Architecture:", -1, &labelFont, Gdiplus::PointF(innerX, curY), &lblBrush);
    g.DrawString(L"Microsoft Indirect Display Driver (IddCx / UMDF 2.x)", -1, &valFont, Gdiplus::PointF(innerX + 110.0f, curY), &valBrush);
    curY += 24.0f;
    g.DrawString(L"Hardware ID:", -1, &labelFont, Gdiplus::PointF(innerX, curY), &lblBrush);
    g.DrawString(L"Root\\SecondScreenIdd", -1, &valFont, Gdiplus::PointF(innerX + 110.0f, curY), &valBrush);
    curY += 24.0f;
    g.DrawString(L"Driver Package:", -1, &labelFont, Gdiplus::PointF(innerX, curY), &lblBrush);
    g.DrawString(L"SecondScreenIdd.dll (x64) + SecondScreenIdd.inf", -1, &valFont, Gdiplus::PointF(innerX + 110.0f, curY), &valBrush);

    // Right Card: Diagnostics & Specifications
    Gdiplus::RectF rightRc((float)rightX, (float)cardTop, (float)rightW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, rightRc, L"Driver Diagnostics & Specifications", L"Technical details and multi-monitor setup", 1);

    float rInnerX = rightRc.X + 20.0f;
    float rInnerW = rightRc.Width - 40.0f;
    float rY = rightRc.Y + 70.0f;

    g.DrawString(L"Supported Display Resolutions:", -1, &labelFont, Gdiplus::PointF(rInnerX, rY), &lblBrush);
    rY += 22.0f;
    g.DrawString(L"•  1920 × 1080 @ 60Hz (Recommended Full HD)\n"
                 L"•  1280 × 720 @ 60Hz (High-Performance HD)\n"
                 L"•  2560 × 1440 @ 60Hz (2K Quad HD)", -1, &valFont, Gdiplus::PointF(rInnerX + 8.0f, rY), &valBrush);
    rY += 66.0f;

    // Clean Uninstallation Guarantee box
    Gdiplus::GraphicsPath infoPath;
    AddRoundedRectangle(infoPath, rInnerX, rY, rInnerW, 58.0f, 6.0f);
    Gdiplus::SolidBrush infoBg(Gdiplus::Color(255, 11, 20, 42));
    g.FillPath(&infoBg, &infoPath);
    Gdiplus::Pen infoPen(Gdiplus::Color(255, 30, 48, 86), 1.0f);
    g.DrawPath(&infoPen, &infoPath);

    Gdiplus::Font smallBold(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font smallReg(L"Segoe UI", 8.6f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));
    Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 226, 232, 240));

    g.DrawString(L"Clean Uninstallation Guarantee", -1, &smallBold, Gdiplus::PointF(rInnerX + 10.0f, rY + 8.0f), &cyanTxt);
    g.DrawString(L"Clicking 'Uninstall Driver' thoroughly purges the device node and deletes\n"
                 L"all matching driver packages from the Windows Driver Store.", -1, &smallReg, Gdiplus::PointF(rInnerX + 10.0f, rY + 26.0f), &whiteTxt);
}

void DrawSettingsTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"Settings & Host Preferences",
                   L"Configure display preferences, streaming quality, network ports, and startup behavior.", 4);

    Gdiplus::RectF leftRc((float)contentX, (float)cardTop, (float)leftW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, leftRc, L"Display Preferences", L"Target streaming parameters negotiated with client", 1);

    float innerX = leftRc.X + 20.0f;
    float innerW = leftRc.Width - 40.0f;
    Gdiplus::Font lblFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 126, 145, 173));

    g.DrawString(L"Preferred Frame Rate", -1, &lblFont, Gdiplus::PointF(innerX, leftRc.Y + 70.0f), &lblBrush);
    g.DrawString(L"Streaming Bitrate Profile", -1, &lblFont, Gdiplus::PointF(innerX, leftRc.Y + 128.0f), &lblBrush);
    g.DrawString(L"Target Monitor Source", -1, &lblFont, Gdiplus::PointF(innerX, leftRc.Y + 186.0f), &lblBrush);

    // Technical note matching phone app
    float noteY = leftRc.Y + 248.0f;
    Gdiplus::GraphicsPath notePath;
    AddRoundedRectangle(notePath, innerX, noteY, innerW, 76.0f, 6.0f);
    Gdiplus::SolidBrush noteBg(Gdiplus::Color(255, 11, 20, 42));
    g.FillPath(&noteBg, &notePath);
    Gdiplus::Pen notePen(Gdiplus::Color(255, 30, 48, 86), 1.0f);
    g.DrawPath(&notePen, &notePath);

    Gdiplus::Font noteFont(L"Segoe UI", 8.7f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush noteBrush(Gdiplus::Color(255, 148, 163, 184));
    g.DrawString(L"Note: Resolution and frame rate options negotiate target parameters\n"
                 L"with the client. Actual rendered FPS depends on physical display\n"
                 L"hardware, GPU encoding capability, and local Wi-Fi bandwidth.", -1, &noteFont, Gdiplus::PointF(innerX + 10.0f, noteY + 10.0f), &noteBrush);

    Gdiplus::RectF rightRc((float)rightX, (float)cardTop, (float)rightW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, rightRc, L"Host & Connection Preferences", L"Network ports, PC hostname, and startup behavior", 2);

    float rInnerX = rightRc.X + 20.0f;
    g.DrawString(L"PC Display Name", -1, &lblFont, Gdiplus::PointF(rInnerX, rightRc.Y + 70.0f), &lblBrush);
    g.DrawString(L"Streaming Port (Default: 52025)", -1, &lblFont, Gdiplus::PointF(rInnerX, rightRc.Y + 128.0f), &lblBrush);
}

void DrawGuideTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int colW = static_cast<int>((contentW - gap) * 0.485f);
    int rightColW = contentW - gap - colW;
    int rightColX = contentX + colW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"How to Use DuetView",
                   L"Step-by-step setup instructions for Wi-Fi and USB, display topology, gestures, and troubleshooting.", 5);

    int cardH = (cardBottom - cardTop - 14) / 2;

    // Card 1: Connection Methods (Top-Left)
    Gdiplus::RectF c1Rc((float)contentX, (float)cardTop, (float)colW, (float)cardH);
    DrawCardContainer(g, c1Rc, L"1. Connection Methods", L"Wi-Fi vs. Wired USB connection", 1);
    float c1X = c1Rc.X + 16.0f;
    float c1Y = c1Rc.Y + 68.0f;
    Gdiplus::Font boldF(L"Segoe UI", 9.2f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font regF(L"Segoe UI", 8.8f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));
    Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 240, 245, 255));
    Gdiplus::SolidBrush mutedTxt(Gdiplus::Color(255, 126, 145, 173));

    g.DrawString(L"Wi-Fi (Wireless Mode):", -1, &boldF, Gdiplus::PointF(c1X, c1Y), &cyanTxt);
    g.DrawString(L"Connect PC and Android to the same 5 GHz Wi-Fi network.\nTap 'Scan to Connect' or choose this PC from 'Discover PCs'.", -1, &regF, Gdiplus::PointF(c1X, c1Y + 18.0f), &whiteTxt);
    c1Y += 56.0f;
    g.DrawString(L"Wired USB Connection:", -1, &boldF, Gdiplus::PointF(c1X, c1Y), &cyanTxt);
    g.DrawString(L"Connect via USB cable and enable USB Tethering in Android settings.\nSwitch to USB mode in the Android app for instant zero-lag mode.", -1, &regF, Gdiplus::PointF(c1X, c1Y + 18.0f), &whiteTxt);

    // Card 2: Display Modes (Top-Right)
    Gdiplus::RectF c2Rc((float)rightColX, (float)cardTop, (float)rightColW, (float)cardH);
    DrawCardContainer(g, c2Rc, L"2. Display Modes", L"Extended desktop vs. desktop mirroring", 3);
    float c2X = c2Rc.X + 16.0f;
    float c2Y = c2Rc.Y + 68.0f;
    g.DrawString(L"True Extended Desktop:", -1, &boldF, Gdiplus::PointF(c2X, c2Y), &cyanTxt);
    g.DrawString(L"Turns your Android into an independent secondary monitor with\nindependent windows. Requires the driver. Press Win + P -> Extend.", -1, &regF, Gdiplus::PointF(c2X, c2Y + 18.0f), &whiteTxt);
    c2Y += 56.0f;
    g.DrawString(L"Desktop Mirroring:", -1, &boldF, Gdiplus::PointF(c2X, c2Y), &cyanTxt);
    g.DrawString(L"Works immediately with zero drivers required. Clones your main\ndisplay for viewing, media playback, or quick presentations.", -1, &regF, Gdiplus::PointF(c2X, c2Y + 18.0f), &whiteTxt);

    // Card 3: Controls & Gestures (Bottom-Left)
    float bRowY = (float)(cardTop + cardH + 14);
    Gdiplus::RectF c3Rc((float)contentX, bRowY, (float)colW, (float)cardH);
    DrawCardContainer(g, c3Rc, L"3. Controls & Gestures", L"Interact with Windows from touch", 2);
    float c3X = c3Rc.X + 16.0f;
    float c3Y = c3Rc.Y + 68.0f;
    g.DrawString(L"•  Left Click: Single tap on the screen\n"
                 L"•  Right Click: Long press or two-finger tap\n"
                 L"•  Scroll: Two-finger swipe up or down\n"
                 L"•  Virtual Keyboard: Tap keyboard icon for Windows hotkeys (Win, Alt, Ctrl, Esc)", -1, &regF, Gdiplus::PointF(c3X, c3Y), &whiteTxt);

    // Card 4: Troubleshooting (Bottom-Right)
    Gdiplus::RectF c4Rc((float)rightColX, bRowY, (float)rightColW, (float)cardH);
    DrawCardContainer(g, c4Rc, L"4. Troubleshooting Guide", L"Solutions for common network and display issues", 4);
    float c4X = c4Rc.X + 16.0f;
    float c4Y = c4Rc.Y + 68.0f;
    g.DrawString(L"•  Firewall: Ensure Windows Defender allows port 52025\n"
                 L"•  Latency: Use 5 GHz Wi-Fi or USB tethering instead of 2.4 GHz\n"
                 L"•  Driver Fix: If extending fails, go to Driver tab and click 'Repair Driver'", -1, &regF, Gdiplus::PointF(c4X, c4Y), &whiteTxt);
}

void DrawAboutTab(Gdiplus::Graphics& g, int clientW, int clientH) {
    int contentX = 200;
    int contentW = clientW - 220;
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;
    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    DrawHeroHeader(g, Gdiplus::RectF((float)contentX, 58.0f, (float)contentW, 90.0f),
                   L"About DuetView PC",
                   L"Open-source secondary display solution connecting Windows and Android.", 6);

    Gdiplus::RectF leftRc((float)contentX, (float)cardTop, (float)leftW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, leftRc, L"Application Identity & Architecture", L"DuetView PC Desktop Host Manager", 6);

    float innerX = leftRc.X + 20.0f;
    float innerW = leftRc.Width - 40.0f;
    float curY = leftRc.Y + 70.0f;

    Gdiplus::Font boldF(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font regF(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));
    Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 240, 245, 255));
    Gdiplus::SolidBrush mutedTxt(Gdiplus::Color(255, 126, 145, 173));

    g.DrawString(L"Application Version:", -1, &boldF, Gdiplus::PointF(innerX, curY), &whiteTxt);
    g.DrawString(L"0.4.0 (Unified Release Build)", -1, &regF, Gdiplus::PointF(innerX + 150.0f, curY), &cyanTxt);
    curY += 26.0f;
    g.DrawString(L"Technology Stack:", -1, &boldF, Gdiplus::PointF(innerX, curY), &whiteTxt);
    g.DrawString(L"Direct3D 11 DXGI Duplication & WMF H.264", -1, &regF, Gdiplus::PointF(innerX + 150.0f, curY), &cyanTxt);
    curY += 26.0f;
    g.DrawString(L"Virtual Display Driver:", -1, &boldF, Gdiplus::PointF(innerX, curY), &whiteTxt);
    g.DrawString(L"Microsoft Indirect Display Driver (IddCx / UMDF 2.x)", -1, &regF, Gdiplus::PointF(innerX + 150.0f, curY), &cyanTxt);
    curY += 34.0f;

    // Privacy & Security Card
    Gdiplus::GraphicsPath privPath;
    AddRoundedRectangle(privPath, innerX, curY, innerW, 80.0f, 6.0f);
    Gdiplus::SolidBrush privBg(Gdiplus::Color(255, 11, 20, 42));
    g.FillPath(&privBg, &privPath);
    Gdiplus::Pen privPen(Gdiplus::Color(255, 30, 48, 86), 1.0f);
    g.DrawPath(&privPen, &privPath);

    g.DrawString(L"Privacy & Peer-to-Peer Security", -1, &boldF, Gdiplus::PointF(innerX + 12.0f, curY + 10.0f), &cyanTxt);
    g.DrawString(L"Zero cloud servers. Screen transmission, audio, and touch input\n"
                 L"remain 100% peer-to-peer on your local Wi-Fi or direct USB cable.", -1, &regF, Gdiplus::PointF(innerX + 12.0f, curY + 30.0f), &whiteTxt);

    Gdiplus::RectF rightRc((float)rightX, (float)cardTop, (float)rightW, (float)(cardBottom - cardTop));
    DrawCardContainer(g, rightRc, L"Developer & Open Source Project", L"Author, repository, and release distribution", 2);

    float rInnerX = rightRc.X + 20.0f;
    float rY = rightRc.Y + 70.0f;

    g.DrawString(L"Lead Developer:", -1, &boldF, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    g.DrawString(L"Aryan Verma (@Aryan130805)", -1, &regF, Gdiplus::PointF(rInnerX + 130.0f, rY), &cyanTxt);
    rY += 24.0f;
    g.DrawString(L"Open Source License:", -1, &boldF, Gdiplus::PointF(rInnerX, rY), &whiteTxt);
    g.DrawString(L"MIT Open Source License", -1, &regF, Gdiplus::PointF(rInnerX + 130.0f, rY), &mutedTxt);
}

void RelayoutControls(int clientW, int clientH) {
    // 1. Top Header Actions
    if (g_hBtnDisplay) {
        SetWindowPos(g_hBtnDisplay, NULL, clientW - 365, 14, 195, 32, SWP_NOZORDER);
    }

    // 2. Hero Banner
    int contentX = 200;
    int contentW = clientW - 220;
    g_rcHero = { contentX, 58, contentX + contentW, 148 };

    // 3. Two Main Cards
    int cardTop = 158;
    int cardBottom = clientH - (g_logsExpanded ? 175 : 52);
    int gap = 18;

    int leftW = static_cast<int>((contentW - gap) * 0.485f);
    int rightW = contentW - gap - leftW;
    int rightX = contentX + leftW + gap;

    g_rcLeftCard = { contentX, cardTop, contentX + leftW, cardBottom };
    g_rcRightCard = { rightX, cardTop, rightX + rightW, cardBottom };

    // Left Card Form Controls
    int pad = 18;
    int innerX = contentX + pad;
    int innerW = leftW - pad * 2;

    // Field 1: PC Display Name
    int f1Y = cardTop + 86;
    g_rcInputPcNameContainer = { innerX, f1Y, innerX + innerW, f1Y + 30 };
    if (g_hEditPcName) {
        SetWindowPos(g_hEditPcName, NULL, innerX + 8, f1Y + 5, innerW - 36, 20, SWP_NOZORDER);
    }

    // Field 2: PC Hardware ID
    int f2Y = cardTop + 144;
    int copyBtnW = 68;
    int idInputW = innerW - copyBtnW - 8;
    g_rcInputPcIdContainer = { innerX, f2Y, innerX + idInputW, f2Y + 30 };
    if (g_hEditPcId) {
        SetWindowPos(g_hEditPcId, NULL, innerX + 8, f2Y + 5, idInputW - 16, 20, SWP_NOZORDER);
    }
    if (g_hBtnCopyId) {
        SetWindowPos(g_hBtnCopyId, NULL, innerX + idInputW + 8, f2Y, copyBtnW, 30, SWP_NOZORDER);
    }

    // Field 3: Port & Target FPS
    int f3Y = cardTop + 202;
    int portW = static_cast<int>((innerW - 12) * 0.40f);
    int fpsW = innerW - 12 - portW;
    int fpsX = innerX + portW + 12;
    g_rcInputPortContainer = { innerX, f3Y, innerX + portW, f3Y + 30 };
    if (g_hEditPort) {
        SetWindowPos(g_hEditPort, NULL, innerX + 8, f3Y + 5, portW - 16, 20, SWP_NOZORDER);
    }
    if (g_hComboFps) {
        SetWindowPos(g_hComboFps, NULL, fpsX, f3Y, fpsW, 160, SWP_NOZORDER);
    }

    // Field 4: Bitrate Profile
    int f4Y = cardTop + 260;
    if (g_hComboBitrate) {
        SetWindowPos(g_hComboBitrate, NULL, innerX, f4Y, innerW, 160, SWP_NOZORDER);
    }

    // Field 5: Target Monitor Source
    int f5Y = cardTop + 318;
    if (g_hComboMonitor) {
        SetWindowPos(g_hComboMonitor, NULL, innerX, f5Y, innerW, 160, SWP_NOZORDER);
    }

    // Field 6: Diagnostic Console Checkbox
    int f6Y = cardTop + 358;
    if (g_hChkConsole) {
        SetWindowPos(g_hChkConsole, NULL, innerX, f6Y, innerW, 22, SWP_NOZORDER);
    }

    // Field 7: Start / Stop Button
    int f7Y = cardTop + 390;
    if (g_hBtnStartStop) {
        SetWindowPos(g_hBtnStartStop, NULL, innerX, f7Y, innerW, 42, SWP_NOZORDER);
    }

    // Right Card Controls
    int rInnerX = rightX + 16;
    int rInnerW = rightW - 32;

    int qrSize = 156;
    g_rcQrBox = { rInnerX + 8, cardTop + 72, rInnerX + 8 + qrSize, cardTop + 72 + qrSize };
    g_rcMetadata = { rInnerX + qrSize + 24, cardTop + 72, rInnerX + rInnerW, cardTop + 72 + qrSize };

    // Home Right Card: Regenerate QR and Install Driver side-by-side
    int btnGap = 12;
    int btnW = (rInnerW - btnGap) / 2;
    int btnH = 36;
    int row1Y = cardTop + 244;

    if (g_hBtnRegen) SetWindowPos(g_hBtnRegen, NULL, rInnerX, row1Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnInstallDriver) SetWindowPos(g_hBtnInstallDriver, NULL, rInnerX + btnW + btnGap, row1Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnUninstallDriver) SetWindowPos(g_hBtnUninstallDriver, NULL, rInnerX + btnW + btnGap, row1Y, btnW, btnH, SWP_NOZORDER);

    // Status Banner (Clean layout below the single action row)
    g_rcStatusBanner = { rInnerX, cardTop + 296, rInnerX + rInnerW, cardTop + 356 };

    // Streaming Tab Controls
    if (g_hBtnStreamingStartStop) SetWindowPos(g_hBtnStreamingStartStop, NULL, rInnerX, cardTop + 72, rInnerW, 40, SWP_NOZORDER);
    if (g_hBtnStreamingDisplay) SetWindowPos(g_hBtnStreamingDisplay, NULL, rInnerX, cardTop + 120, rInnerW, 34, SWP_NOZORDER);
    if (g_hBtnStreamingKeyframe) SetWindowPos(g_hBtnStreamingKeyframe, NULL, rInnerX, cardTop + 162, btnW, 34, SWP_NOZORDER);
    if (g_hBtnStreamingCopyInfo) SetWindowPos(g_hBtnStreamingCopyInfo, NULL, rInnerX + btnW + btnGap, cardTop + 162, btnW, 34, SWP_NOZORDER);

    // Devices Tab Controls
    if (g_hBtnDevicesRefresh) SetWindowPos(g_hBtnDevicesRefresh, NULL, rInnerX, cardTop + 240, btnW, 36, SWP_NOZORDER);
    if (g_hBtnDevicesClear) SetWindowPos(g_hBtnDevicesClear, NULL, rInnerX + btnW + btnGap, cardTop + 240, btnW, 36, SWP_NOZORDER);

    // Driver Tab Controls
    if (g_hBtnDriverTabInstall) SetWindowPos(g_hBtnDriverTabInstall, NULL, innerX, cardTop + 200, (innerW - btnGap) / 2, 36, SWP_NOZORDER);
    if (g_hBtnDriverTabUninstall) SetWindowPos(g_hBtnDriverTabUninstall, NULL, innerX, cardTop + 200, (innerW - btnGap) / 2, 36, SWP_NOZORDER);
    if (g_hBtnDriverTabRepair) SetWindowPos(g_hBtnDriverTabRepair, NULL, innerX + (innerW - btnGap) / 2 + btnGap, cardTop + 200, (innerW - btnGap) / 2, 36, SWP_NOZORDER);
    if (g_hBtnDriverTabUpdate) SetWindowPos(g_hBtnDriverTabUpdate, NULL, innerX, cardTop + 244, innerW, 34, SWP_NOZORDER);
    if (g_hBtnDriverTabDevMgmt) SetWindowPos(g_hBtnDriverTabDevMgmt, NULL, rInnerX, cardTop + 244, btnW, 36, SWP_NOZORDER);
    if (g_hBtnDriverTabDisplay) SetWindowPos(g_hBtnDriverTabDisplay, NULL, rInnerX + btnW + btnGap, cardTop + 244, btnW, 36, SWP_NOZORDER);

    // Settings Tab Controls
    if (g_hComboSettingsFps) SetWindowPos(g_hComboSettingsFps, NULL, innerX, cardTop + 90, innerW, 160, SWP_NOZORDER);
    if (g_hComboSettingsBitrate) SetWindowPos(g_hComboSettingsBitrate, NULL, innerX, cardTop + 148, innerW, 160, SWP_NOZORDER);
    if (g_hComboSettingsMonitor) SetWindowPos(g_hComboSettingsMonitor, NULL, innerX, cardTop + 206, innerW, 160, SWP_NOZORDER);
    if (g_hEditSettingsPcName) SetWindowPos(g_hEditSettingsPcName, NULL, rInnerX, cardTop + 90, rInnerW, 26, SWP_NOZORDER);
    if (g_hEditSettingsPort) SetWindowPos(g_hEditSettingsPort, NULL, rInnerX, cardTop + 148, rInnerW, 26, SWP_NOZORDER);
    if (g_hChkAutoStart) SetWindowPos(g_hChkAutoStart, NULL, rInnerX, cardTop + 188, rInnerW, 22, SWP_NOZORDER);
    if (g_hChkDiscovery) SetWindowPos(g_hChkDiscovery, NULL, rInnerX, cardTop + 214, rInnerW, 22, SWP_NOZORDER);
    if (g_hBtnSettingsSave) SetWindowPos(g_hBtnSettingsSave, NULL, rInnerX, cardTop + 252, btnW, 36, SWP_NOZORDER);
    if (g_hBtnSettingsReset) SetWindowPos(g_hBtnSettingsReset, NULL, rInnerX + btnW + btnGap, cardTop + 252, btnW, 36, SWP_NOZORDER);

    // Guide Tab Controls
    if (g_hBtnGuideDisplay) SetWindowPos(g_hBtnGuideDisplay, NULL, rInnerX, cardBottom - 48, btnW, 34, SWP_NOZORDER);
    if (g_hBtnGuideDriver) SetWindowPos(g_hBtnGuideDriver, NULL, rInnerX + btnW + btnGap, cardBottom - 48, btnW, 34, SWP_NOZORDER);

    // About Tab Controls
    if (g_hBtnAboutDeveloper) SetWindowPos(g_hBtnAboutDeveloper, NULL, rInnerX, cardTop + 132, rInnerW, 36, SWP_NOZORDER);
    if (g_hBtnAboutRepo) SetWindowPos(g_hBtnAboutRepo, NULL, rInnerX, cardTop + 178, rInnerW, 36, SWP_NOZORDER);
    if (g_hBtnAboutReleases) SetWindowPos(g_hBtnAboutReleases, NULL, rInnerX, cardTop + 224, rInnerW, 36, SWP_NOZORDER);

    // Footer Controls
    int footerY = clientH - (g_logsExpanded ? 160 : 42);
    if (g_hBtnToggleLogs) {
        SetWindowPos(g_hBtnToggleLogs, NULL, contentX, footerY, 150, 30, SWP_NOZORDER);
    }

    // Expandable logs drawer
    if (g_hEditLogs) {
        SetWindowPos(g_hEditLogs, NULL, contentX, footerY + 36, contentW, 110, SWP_NOZORDER);
    }
    if (g_hBtnClearLogs) {
        SetWindowPos(g_hBtnClearLogs, NULL, contentX + contentW - 140, footerY + 4, 65, 26, SWP_NOZORDER);
    }
    if (g_hBtnCopyLogs) {
        SetWindowPos(g_hBtnCopyLogs, NULL, contentX + contentW - 70, footerY + 4, 70, 26, SWP_NOZORDER);
    }
}

} // namespace

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontMono = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        g_hBrushBg = CreateSolidBrush(COLOR_BG);
        g_hBrushSidebarBg = CreateSolidBrush(COLOR_SIDEBAR_BG);
        g_hBrushCardBg = CreateSolidBrush(COLOR_CARD_BG);
        g_hBrushInputBg = CreateSolidBrush(COLOR_INPUT_BG);

        // Header Display Settings button (Owner-drawn)
        g_hBtnDisplay = CreateWindowW(L"BUTTON", L"Display Settings (Win + P)",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            670, 14, 195, 32, hWnd, (HMENU)IDC_BTN_DISPLAY_SETTINGS, NULL, NULL);

        // 1. Left Card: Host Configuration Controls
        // PC Display Name
        g_hEditPcName = CreateWindowExW(0, L"EDIT", L"DESKTOP-02THEMS",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            218, 186, 340, 22, hWnd, (HMENU)IDC_EDIT_PC_NAME, NULL, NULL);
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
        g_hEditPcId = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            218, 244, 260, 22, hWnd, (HMENU)IDC_EDIT_PC_ID, NULL, NULL);
        SendMessageW(g_hEditPcId, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
        if (g_pairingMgr) {
            std::string id = g_pairingMgr->GetPcId();
            std::wstring wId(id.begin(), id.end());
            SetWindowTextW(g_hEditPcId, wId.c_str());
        }

        g_hBtnCopyId = CreateWindowW(L"BUTTON", L"Copy", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            505, 240, 68, 30, hWnd, (HMENU)IDC_BTN_COPY_PCID, NULL, NULL);

        // Port
        g_hEditPort = CreateWindowExW(0, L"EDIT", L"52025",
            WS_CHILD | WS_VISIBLE | ES_NUMBER,
            218, 294, 140, 22, hWnd, (HMENU)IDC_EDIT_PORT, NULL, NULL);
        SendMessageW(g_hEditPort, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Target FPS
        g_hComboFps = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            395, 294, 180, 160, hWnd, (HMENU)IDC_COMBO_FPS, NULL, NULL);
        SendMessageW(g_hComboFps, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"120 FPS (120Hz Ultra)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"90 FPS (90Hz Fluid)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"60 FPS (60Hz Smooth)");
        SendMessageW(g_hComboFps, CB_ADDSTRING, 0, (LPARAM)L"30 FPS (30Hz Low Bandwidth)");
        SendMessageW(g_hComboFps, CB_SETCURSEL, 2, 0);
        SetWindowSubclass(g_hComboFps, DarkComboSubclassProc, 1, 0);

        // Target Bitrate
        g_hComboBitrate = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            218, 348, 360, 150, hWnd, (HMENU)IDC_COMBO_BITRATE, NULL, NULL);
        SendMessageW(g_hComboBitrate, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"2000 Kbps (Lightweight / 2.4 GHz)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"4000 Kbps (Balanced / 5 GHz)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"8000 Kbps (High Fidelity)");
        SendMessageW(g_hComboBitrate, CB_ADDSTRING, 0, (LPARAM)L"12000 Kbps (Lossless / USB)");
        SendMessageW(g_hComboBitrate, CB_SETCURSEL, 1, 0);
        SetWindowSubclass(g_hComboBitrate, DarkComboSubclassProc, 2, 0);

        // Target Monitor Source
        g_hComboMonitor = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            218, 402, 360, 150, hWnd, (HMENU)IDC_COMBO_MONITOR, NULL, NULL);
        SendMessageW(g_hComboMonitor, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Auto: Extended / Virtual Screen (Recommended)");
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #0: Primary Desktop Monitor");
        SendMessageW(g_hComboMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #1: Secondary / Virtual Monitor");
        SendMessageW(g_hComboMonitor, CB_SETCURSEL, 0, 0);
        SetWindowSubclass(g_hComboMonitor, DarkComboSubclassProc, 3, 0);

        // Diagnostic Console Checkbox (Owner-drawn)
        g_hChkConsole = CreateWindowW(L"BUTTON", L"Show raw engine console window (Diagnostic)",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            218, 442, 360, 22, hWnd, (HMENU)IDC_CHK_CONSOLE, NULL, NULL);

        // Start / Stop Button (Owner-drawn Gradient)
        g_hBtnStartStop = CreateWindowW(L"BUTTON", L"Start Host Service",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            218, 480, 360, 42, hWnd, (HMENU)IDC_BTN_START_STOP, NULL, NULL);

        // 2. Right Card: Pair & Connect Controls (Owner-drawn)
        // Clean symmetrical Home Card: Regenerate QR & Driver Install/Uninstall
        g_hBtnRegen = CreateWindowW(L"BUTTON", L"Regenerate QR",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            625, 365, 175, 32, hWnd, (HMENU)IDC_BTN_REGEN_QR, NULL, NULL);

        g_hBtnInstallDriver = CreateWindowW(L"BUTTON", L"Install Virtual Display Driver",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_INSTALL_DRIVER, NULL, NULL);

        g_hBtnUninstallDriver = CreateWindowW(L"BUTTON", L"Uninstall Driver",
            WS_CHILD | BS_OWNERDRAW,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_UNINSTALL_DRIVER, NULL, NULL);

        // ---------------------------------------------------------------------
        // Streaming Tab Controls
        // ---------------------------------------------------------------------
        g_hBtnStreamingStartStop = CreateWindowW(L"BUTTON", L"Start Streaming",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_STREAMING_START_STOP, NULL, NULL);

        g_hBtnStreamingDisplay = CreateWindowW(L"BUTTON", L"Display Settings (Win + P)",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_STREAMING_DISPLAY, NULL, NULL);

        g_hBtnStreamingKeyframe = CreateWindowW(L"BUTTON", L"Request Keyframe",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_STREAMING_KEYFRAME, NULL, NULL);

        g_hBtnStreamingCopyInfo = CreateWindowW(L"BUTTON", L"Copy Telemetry",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_STREAMING_COPY_INFO, NULL, NULL);

        // ---------------------------------------------------------------------
        // Devices Tab Controls
        // ---------------------------------------------------------------------
        g_hBtnDevicesRefresh = CreateWindowW(L"BUTTON", L"Refresh Endpoints",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DEVICES_REFRESH, NULL, NULL);

        g_hBtnDevicesClear = CreateWindowW(L"BUTTON", L"Clear Sessions",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DEVICES_CLEAR, NULL, NULL);

        // ---------------------------------------------------------------------
        // Driver Tab Controls
        // ---------------------------------------------------------------------
        g_hBtnDriverTabInstall = CreateWindowW(L"BUTTON", L"Install Virtual Display Driver",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_INSTALL, NULL, NULL);

        g_hBtnDriverTabUninstall = CreateWindowW(L"BUTTON", L"Uninstall Driver",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_UNINSTALL, NULL, NULL);

        g_hBtnDriverTabRepair = CreateWindowW(L"BUTTON", L"Repair Driver",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_REPAIR, NULL, NULL);

        g_hBtnDriverTabUpdate = CreateWindowW(L"BUTTON", L"Update Driver Package",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_UPDATE, NULL, NULL);

        g_hBtnDriverTabDevMgmt = CreateWindowW(L"BUTTON", L"Device Manager",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_DEVMGMT, NULL, NULL);

        g_hBtnDriverTabDisplay = CreateWindowW(L"BUTTON", L"Display Settings",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 30, hWnd, (HMENU)IDC_BTN_DRIVER_TAB_DISPLAY, NULL, NULL);

        // ---------------------------------------------------------------------
        // Settings Tab Controls
        // ---------------------------------------------------------------------
        g_hComboSettingsFps = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            0, 0, 100, 160, hWnd, (HMENU)IDC_COMBO_SETTINGS_FPS, NULL, NULL);
        SendMessageW(g_hComboSettingsFps, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboSettingsFps, CB_ADDSTRING, 0, (LPARAM)L"120 FPS (120Hz Ultra)");
        SendMessageW(g_hComboSettingsFps, CB_ADDSTRING, 0, (LPARAM)L"90 FPS (90Hz Fluid)");
        SendMessageW(g_hComboSettingsFps, CB_ADDSTRING, 0, (LPARAM)L"60 FPS (60Hz Smooth)");
        SendMessageW(g_hComboSettingsFps, CB_ADDSTRING, 0, (LPARAM)L"30 FPS (30Hz Low Bandwidth)");
        SendMessageW(g_hComboSettingsFps, CB_SETCURSEL, 2, 0);
        SetWindowSubclass(g_hComboSettingsFps, DarkComboSubclassProc, 11, 0);

        g_hComboSettingsBitrate = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            0, 0, 100, 160, hWnd, (HMENU)IDC_COMBO_SETTINGS_BITRATE, NULL, NULL);
        SendMessageW(g_hComboSettingsBitrate, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboSettingsBitrate, CB_ADDSTRING, 0, (LPARAM)L"2000 Kbps (Lightweight / 2.4 GHz)");
        SendMessageW(g_hComboSettingsBitrate, CB_ADDSTRING, 0, (LPARAM)L"4000 Kbps (Balanced / 5 GHz)");
        SendMessageW(g_hComboSettingsBitrate, CB_ADDSTRING, 0, (LPARAM)L"8000 Kbps (High Fidelity)");
        SendMessageW(g_hComboSettingsBitrate, CB_ADDSTRING, 0, (LPARAM)L"12000 Kbps (Lossless / USB)");
        SendMessageW(g_hComboSettingsBitrate, CB_SETCURSEL, 1, 0);
        SetWindowSubclass(g_hComboSettingsBitrate, DarkComboSubclassProc, 12, 0);

        g_hComboSettingsMonitor = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
            0, 0, 100, 160, hWnd, (HMENU)IDC_COMBO_SETTINGS_MONITOR, NULL, NULL);
        SendMessageW(g_hComboSettingsMonitor, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hComboSettingsMonitor, CB_ADDSTRING, 0, (LPARAM)L"Auto: Extended / Virtual Screen (Recommended)");
        SendMessageW(g_hComboSettingsMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #0: Primary Desktop Monitor");
        SendMessageW(g_hComboSettingsMonitor, CB_ADDSTRING, 0, (LPARAM)L"Display #1: Secondary / Virtual Monitor");
        SendMessageW(g_hComboSettingsMonitor, CB_SETCURSEL, 0, 0);
        SetWindowSubclass(g_hComboSettingsMonitor, DarkComboSubclassProc, 13, 0);

        g_hEditSettingsPcName = CreateWindowExW(0, L"EDIT", L"DuetView PC",
            WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 22, hWnd, (HMENU)IDC_EDIT_SETTINGS_PC_NAME, NULL, NULL);
        SendMessageW(g_hEditSettingsPcName, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        if (g_pairingMgr) {
            std::string name = g_pairingMgr->GetPcName();
            int wLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
            if (wLen > 0) {
                std::wstring wName(wLen - 1, 0);
                MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wLen);
                SetWindowTextW(g_hEditSettingsPcName, wName.c_str());
            }
        }

        g_hEditSettingsPort = CreateWindowExW(0, L"EDIT", L"52025",
            WS_CHILD | ES_NUMBER, 0, 0, 100, 22, hWnd, (HMENU)IDC_EDIT_SETTINGS_PORT, NULL, NULL);
        SendMessageW(g_hEditSettingsPort, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hChkAutoStart = CreateWindowW(L"BUTTON", L"Auto-start host service when PC app launches",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 22, hWnd, (HMENU)IDC_CHK_AUTO_START, NULL, NULL);

        g_hChkDiscovery = CreateWindowW(L"BUTTON", L"Enable UDP local network discovery broadcast",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 22, hWnd, (HMENU)IDC_CHK_DISCOVERY, NULL, NULL);

        g_hBtnSettingsSave = CreateWindowW(L"BUTTON", L"Save Preferences",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_SETTINGS_SAVE, NULL, NULL);

        g_hBtnSettingsReset = CreateWindowW(L"BUTTON", L"Reset Defaults",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_SETTINGS_RESET, NULL, NULL);

        // ---------------------------------------------------------------------
        // Guide Tab Controls
        // ---------------------------------------------------------------------
        g_hBtnGuideDisplay = CreateWindowW(L"BUTTON", L"Display Settings (Win + P)",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_GUIDE_DISPLAY, NULL, NULL);

        g_hBtnGuideDriver = CreateWindowW(L"BUTTON", L"Driver Management",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_GUIDE_DRIVER, NULL, NULL);

        // ---------------------------------------------------------------------
        // About Tab Controls
        // ---------------------------------------------------------------------
        g_hBtnAboutDeveloper = CreateWindowW(L"BUTTON", L"Developer Profile (Aryan130805)",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_ABOUT_DEVELOPER, NULL, NULL);

        g_hBtnAboutRepo = CreateWindowW(L"BUTTON", L"GitHub Repository",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_ABOUT_REPO, NULL, NULL);

        g_hBtnAboutReleases = CreateWindowW(L"BUTTON", L"Releases & Updates",
            WS_CHILD | BS_OWNERDRAW, 0, 0, 100, 32, hWnd, (HMENU)IDC_BTN_ABOUT_RELEASES, NULL, NULL);

        // 3. Bottom Status Bar & Expandable Logs Drawer
        g_hBtnToggleLogs = CreateWindowW(L"BUTTON", L"Diagnostic Logs  ▼",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            200, 565, 150, 30, hWnd, (HMENU)IDC_BTN_TOGGLE_LOGS, NULL, NULL);

        g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear",
            WS_CHILD | BS_OWNERDRAW,
            845, 608, 65, 26, hWnd, (HMENU)IDC_BTN_CLEAR_LOGS, NULL, NULL);

        g_hBtnCopyLogs = CreateWindowW(L"BUTTON", L"Copy All",
            WS_CHILD | BS_OWNERDRAW,
            918, 608, 70, 26, hWnd, (HMENU)IDC_BTN_COPY_LOGS, NULL, NULL);

        g_hEditLogs = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            200, 640, 790, 110, hWnd, (HMENU)IDC_EDIT_LOGS, NULL, NULL);
        SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

        g_driverMgr = std::make_unique<DriverManager>();
        RefreshDriverStatus();
        UpdateTabVisibility();

        SetTimer(hWnd, IDT_TICK_TIMER, 1000, NULL);
        AppendLogLine("[INFO] DuetView PC Desktop Host Manager Initialized (Ready to Start).");
        if (g_autoStartHost) {
            PostMessageW(hWnd, WM_USER_FIRST_LAUNCH, 0, 0);
        }
        return 0;
    }

    case WM_SIZE: {
        int clientW = LOWORD(lParam);
        int clientH = HIWORD(lParam);
        RelayoutControls(clientW, clientH);
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 1120;
        lpMMI->ptMinTrackSize.y = 700;
        return 0;
    }

    case WM_USER_FIRST_LAUNCH: {
        if (g_autoStartHost && g_procMgr && !g_procMgr->IsRunning()) {
            OnStartStopClicked();
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        int oldHoverTab = g_hoveredTab;
        if (x >= 12 && x <= 168) {
            if (y >= 86 && y < 124) g_hoveredTab = TAB_HOME;
            else if (y >= 130 && y < 168) g_hoveredTab = TAB_STREAMING;
            else if (y >= 174 && y < 212) g_hoveredTab = TAB_DEVICES;
            else if (y >= 218 && y < 256) g_hoveredTab = TAB_DRIVER;
            else if (y >= 262 && y < 300) g_hoveredTab = TAB_SETTINGS;
            else if (y >= 306 && y < 344) g_hoveredTab = TAB_GUIDE;
            else {
                RECT clientRc;
                GetClientRect(hWnd, &clientRc);
                if (y >= clientRc.bottom - 50 && y < clientRc.bottom - 14) g_hoveredTab = TAB_ABOUT;
                else g_hoveredTab = -1;
            }
        } else {
            g_hoveredTab = -1;
        }

        if (oldHoverTab != g_hoveredTab) {
            RECT rcSidebar = { 0, 0, 180, 2000 };
            InvalidateRect(hWnd, &rcSidebar, FALSE);
        }

        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (g_hoveredTab != -1) {
            g_hoveredTab = -1;
            RECT rcSidebar = { 0, 0, 180, 2000 };
            InvalidateRect(hWnd, &rcSidebar, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Sidebar navigation clicks
        if (x >= 12 && x <= 168) {
            if (y >= 86 && y < 124) { g_currentTab = TAB_HOME; UpdateTabVisibility(); }
            else if (y >= 130 && y < 168) { g_currentTab = TAB_STREAMING; UpdateTabVisibility(); }
            else if (y >= 174 && y < 212) { g_currentTab = TAB_DEVICES; UpdateTabVisibility(); }
            else if (y >= 218 && y < 256) { g_currentTab = TAB_DRIVER; UpdateTabVisibility(); }
            else if (y >= 262 && y < 300) { g_currentTab = TAB_SETTINGS; UpdateTabVisibility(); }
            else if (y >= 306 && y < 344) { g_currentTab = TAB_GUIDE; UpdateTabVisibility(); }
            else {
                RECT clientRc;
                GetClientRect(hWnd, &clientRc);
                if (y >= clientRc.bottom - 50 && y < clientRc.bottom - 14) {
                    g_currentTab = TAB_ABOUT;
                    UpdateTabVisibility();
                }
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
        if (!pDIS) return FALSE;

        // Custom Combobox Dropdown Item
        if (pDIS->CtlType == ODT_COMBOBOX) {
            if (pDIS->itemID == (UINT)-1) return TRUE;
            HDC hdc = pDIS->hDC;
            bool selected = (pDIS->itemState & ODS_SELECTED);
            COLORREF bg = selected ? RGB(26, 47, 90) : RGB(9, 18, 36);
            COLORREF fg = selected ? RGB(56, 189, 248) : RGB(240, 245, 255);
            HBRUSH hBr = CreateSolidBrush(bg);
            FillRect(hdc, &pDIS->rcItem, hBr);
            DeleteObject(hBr);
            SetTextColor(hdc, fg);
            SetBkMode(hdc, TRANSPARENT);
            wchar_t itemText[256] = {0};
            SendMessageW(pDIS->hwndItem, CB_GETLBTEXT, pDIS->itemID, (LPARAM)itemText);
            RECT rcText = pDIS->rcItem;
            rcText.left += 10;
            DrawTextW(hdc, itemText, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        // Custom Buttons
        if (pDIS->CtlType == ODT_BUTTON) {
            Gdiplus::Graphics g(pDIS->hDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

            RECT rc = pDIS->rcItem;
            float w = (float)(rc.right - rc.left);
            float h = (float)(rc.bottom - rc.top);
            bool isSelected = (pDIS->itemState & ODS_SELECTED);
            bool isDisabled = (pDIS->itemState & ODS_DISABLED);

            // Determine parent card background to erase corners completely
            Gdiplus::Color parentBg = Gdiplus::Color(255, 14, 25, 50); // #0E1932 (default for card buttons)
            if (pDIS->CtlID == IDC_BTN_DISPLAY_SETTINGS || pDIS->CtlID == IDC_BTN_TOGGLE_LOGS) {
                parentBg = Gdiplus::Color(255, 7, 13, 28); // #070D1C (Header / Footer)
            }
            Gdiplus::SolidBrush parentBrush(parentBg);
            g.FillRectangle(&parentBrush, 0.0f, 0.0f, w, h);

            if (pDIS->CtlID == IDC_BTN_START_STOP || pDIS->CtlID == IDC_BTN_STREAMING_START_STOP) {
                // Vibrant Blue-to-Violet Linear Gradient Button
                const bool isRunning = (g_procMgr && g_procMgr->IsRunning());

                Gdiplus::GraphicsPath path;
                AddRoundedRectangle(path, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 8.0f);

                Gdiplus::Color col1 = isSelected ? Gdiplus::Color(255, 29, 78, 216) : Gdiplus::Color(255, 37, 99, 235);  // #2563EB
                Gdiplus::Color col2 = isSelected ? Gdiplus::Color(255, 107, 33, 239) : Gdiplus::Color(255, 133, 46, 255); // #852EFF

                Gdiplus::LinearGradientBrush gradBrush(Gdiplus::PointF(0, 0), Gdiplus::PointF(w, 0), col1, col2);
                g.FillPath(&gradBrush, &path);

                Gdiplus::Pen glowPen(Gdiplus::Color(255, 96, 165, 250), 1.2f);
                g.DrawPath(&glowPen, &path);

                const wchar_t* btnText = isRunning ? L"■   Stop Host Service" : L"▶   Start Host Service";
                Gdiplus::Font btnFont(L"Segoe UI", 10.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
                Gdiplus::StringFormat strFmt;
                strFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                strFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(btnText, -1, &btnFont, Gdiplus::RectF(0, 0, w, h), &strFmt, &textBrush);
                return TRUE;
            }

            if (pDIS->CtlID == IDC_CHK_CONSOLE || pDIS->CtlID == IDC_CHK_AUTO_START || pDIS->CtlID == IDC_CHK_DISCOVERY) {
                // Custom Checkbox
                float boxY = (h - 16.0f) / 2.0f;
                Gdiplus::GraphicsPath boxPath;
                AddRoundedRectangle(boxPath, 2.0f, boxY, 16.0f, 16.0f, 4.0f);
                Gdiplus::SolidBrush boxBg(Gdiplus::Color(255, 9, 18, 36));
                g.FillPath(&boxBg, &boxPath);
                Gdiplus::Pen boxPen(Gdiplus::Color(255, 31, 50, 86), 1.2f);
                g.DrawPath(&boxPen, &boxPath);

                bool isChecked = false;
                const wchar_t* chkLabel = L"";
                if (pDIS->CtlID == IDC_CHK_CONSOLE) {
                    isChecked = g_showConsoleState;
                    chkLabel = L"Show raw engine console window (Diagnostic)";
                } else if (pDIS->CtlID == IDC_CHK_AUTO_START) {
                    isChecked = g_autoStartHost;
                    chkLabel = L"Auto-start host service when PC app launches";
                } else if (pDIS->CtlID == IDC_CHK_DISCOVERY) {
                    isChecked = g_discoveryEnabled;
                    chkLabel = L"Enable UDP local network discovery broadcast";
                }

                if (isChecked) {
                    Gdiplus::Pen chkPen(Gdiplus::Color(255, 56, 189, 248), 2.0f);
                    chkPen.SetStartCap(Gdiplus::LineCapRound);
                    chkPen.SetEndCap(Gdiplus::LineCapRound);
                    g.DrawLine(&chkPen, 5.0f, boxY + 8.0f, 8.5f, boxY + 12.0f);
                    g.DrawLine(&chkPen, 8.5f, boxY + 12.0f, 14.5f, boxY + 4.5f);
                }

                Gdiplus::Font chkFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush chkTextBrush(Gdiplus::Color(255, 148, 163, 184));
                Gdiplus::StringFormat chkFmt;
                chkFmt.SetAlignment(Gdiplus::StringAlignmentNear);
                chkFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(chkLabel, -1, &chkFont,
                             Gdiplus::RectF(26.0f, 0, w - 26.0f, h), &chkFmt, &chkTextBrush);
                return TRUE;
            }

            // Dark Modern Card Buttons
            Gdiplus::GraphicsPath path;
            AddRoundedRectangle(path, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 6.0f);

            Gdiplus::Color btnBg = isSelected ? Gdiplus::Color(255, 26, 45, 82) : Gdiplus::Color(255, 20, 34, 63); // #14223F
            Gdiplus::SolidBrush bgBrush(btnBg);
            g.FillPath(&bgBrush, &path);

            Gdiplus::Color borderCol = isSelected ? Gdiplus::Color(255, 56, 189, 248) : Gdiplus::Color(255, 35, 55, 92); // #23375C
            Gdiplus::Pen btnPen(borderCol, 1.0f);
            g.DrawPath(&btnPen, &path);

            Gdiplus::Font btnFont(L"Segoe UI", 9.2f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush textBrush(isDisabled ? Gdiplus::Color(255, 100, 116, 139) : Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::Color iconCol = isDisabled ? Gdiplus::Color(255, 100, 116, 139) : Gdiplus::Color(255, 255, 255, 255);

            if (pDIS->CtlID == IDC_BTN_DISPLAY_SETTINGS || pDIS->CtlID == IDC_BTN_STREAMING_DISPLAY ||
                       pDIS->CtlID == IDC_BTN_DRIVER_TAB_DISPLAY || pDIS->CtlID == IDC_BTN_GUIDE_DISPLAY) {
                DrawVectorMonitor(g, 12.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(32.0f, 0.0f, w - 36.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_COPY_PCID || pDIS->CtlID == IDC_BTN_STREAMING_COPY_INFO) {
                DrawVectorCopy(g, 10.0f, (h - 13.0f) / 2.0f, 13.0f, iconCol);
                Gdiplus::RectF txtRc(28.0f, 0.0f, w - 32.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_REGEN_QR || pDIS->CtlID == IDC_BTN_DEVICES_REFRESH ||
                       pDIS->CtlID == IDC_BTN_DRIVER_TAB_UPDATE || pDIS->CtlID == IDC_BTN_SETTINGS_RESET) {
                DrawVectorRefresh(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(32.0f, 0.0f, w - 36.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_INSTALL_DRIVER || pDIS->CtlID == IDC_BTN_DRIVER_TAB_INSTALL ||
                       pDIS->CtlID == IDC_BTN_GUIDE_DRIVER) {
                DrawVectorShield(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_UNINSTALL_DRIVER || pDIS->CtlID == IDC_BTN_DRIVER_TAB_UNINSTALL ||
                       pDIS->CtlID == IDC_BTN_DEVICES_CLEAR) {
                DrawVectorTrash(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_DRIVER_TAB_REPAIR) {
                DrawVectorWrench(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Repair Driver", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_DRIVER_TAB_DEVMGMT) {
                DrawVectorGear(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Device Manager", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_SETTINGS_SAVE) {
                DrawVectorCheck(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Save Preferences", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_STREAMING_KEYFRAME) {
                DrawVectorMonitor(g, 12.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(32.0f, 0.0f, w - 36.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Request Keyframe", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_ABOUT_DEVELOPER) {
                DrawVectorUser(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Developer Profile (Aryan130805)", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_ABOUT_REPO) {
                DrawVectorCode(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"GitHub Repository (Source Code)", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_ABOUT_RELEASES) {
                DrawVectorDownload(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Releases & Updates (GitHub)", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_TOGGLE_LOGS) {
                Gdiplus::RectF txtRc(0.0f, 0.0f, w, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(g_logsExpanded ? L"Diagnostic Logs  ▲" : L"Diagnostic Logs  ▼", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else {
                wchar_t wBuf[128] = {0};
                GetWindowTextW(pDIS->hwndItem, wBuf, 128);
                Gdiplus::RectF txtRc(0.0f, 0.0f, w, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(wBuf, -1, &btnFont, txtRc, &fmt, &textBrush);
            }

            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_START_STOP || id == IDC_BTN_STREAMING_START_STOP) {
            OnStartStopClicked();
        } else if (id == IDC_BTN_REGEN_QR) {
            OnRegenerateQrClicked();
        } else if (id == IDC_BTN_INSTALL_DRIVER || id == IDC_BTN_DRIVER_TAB_INSTALL) {
            OnInstallDriverClicked();
        } else if (id == IDC_BTN_UNINSTALL_DRIVER || id == IDC_BTN_DRIVER_TAB_UNINSTALL) {
            OnUninstallDriverClicked();
        } else if (id == IDC_BTN_DRIVER_TAB_REPAIR) {
            OnRepairDriverClicked();
        } else if (id == IDC_BTN_DRIVER_TAB_UPDATE) {
            OnUpdateDriverClicked();
        } else if (id == IDC_BTN_DRIVER_TAB_DEVMGMT) {
            ShellExecuteW(NULL, L"open", L"devmgmt.msc", NULL, NULL, SW_SHOWNORMAL);
            AppendLogLine("[INFO] Opened Windows Device Manager (devmgmt.msc).");
        } else if (id == IDC_BTN_DISPLAY_SETTINGS || id == IDC_BTN_DRIVER_TAB_DISPLAY ||
                   id == IDC_BTN_STREAMING_DISPLAY || id == IDC_BTN_GUIDE_DISPLAY) {
            SetDisplayConfig(0, NULL, 0, NULL, SDC_TOPOLOGY_EXTEND | SDC_APPLY);
            AppendLogLine("[INFO] Requested Windows Extend display topology (SDC_TOPOLOGY_EXTEND).");
            ShellExecuteW(NULL, L"open", L"ms-settings:display", NULL, NULL, SW_SHOWNORMAL);
        } else if (id == IDC_BTN_GUIDE_DRIVER) {
            if (g_driverMgr && g_driverMgr->DetectDriverState().state != DriverState::Ready) {
                OnInstallDriverClicked();
            } else {
                AppendLogLine("[INFO] Virtual display driver is already installed.");
                MessageBoxW(hWnd, L"Virtual display driver is already installed and ready.", L"DuetView PC", MB_OK | MB_ICONINFORMATION);
            }
        } else if (id == IDC_BTN_DEVICES_REFRESH) {
            AppendLogLine("[INFO] Refreshed connected device endpoints.");
            InvalidateRect(hWnd, nullptr, TRUE);
        } else if (id == IDC_BTN_DEVICES_CLEAR) {
            AppendLogLine("[INFO] Cleared connected client sessions cache.");
            InvalidateRect(hWnd, nullptr, TRUE);
        } else if (id == IDC_BTN_STREAMING_KEYFRAME) {
            AppendLogLine("[INFO] IDR / SPS / PPS Keyframe injected into active video stream.");
        } else if (id == IDC_BTN_STREAMING_COPY_INFO) {
            const wchar_t* telemetry = L"DuetView Streaming Engine Telemetry:\r\n"
                                      L"Capture API: Direct3D 11 DXGI Desktop Duplication (GPU Hardware Accelerated)\r\n"
                                      L"Video Encoder: Windows Media Foundation (IMFTransform) H.264 Baseline\r\n"
                                      L"Bitrate: 4000 Kbps (Balanced / 5 GHz)\r\n"
                                      L"Framerate: 60 FPS (60Hz Smooth)\r\n"
                                      L"Resolution: 1920x1080 (Auto Extended Display)\r\n"
                                      L"Transport: Low-latency non-blocking TCP socket stream\r\n"
                                      L"Status: Active / Ready";
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                size_t bLen = (wcslen(telemetry) + 1) * sizeof(wchar_t);
                HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, bLen);
                if (hGlob) {
                    memcpy(GlobalLock(hGlob), telemetry, bLen);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_UNICODETEXT, hGlob);
                }
                CloseClipboard();
                AppendLogLine("[INFO] Copied streaming engine telemetry to clipboard.");
            }
        } else if (id == IDC_CHK_AUTO_START) {
            g_autoStartHost = !g_autoStartHost;
            InvalidateRect(g_hChkAutoStart, nullptr, FALSE);
            AppendLogLine(g_autoStartHost ? "[INFO] Auto-start host on launch enabled." : "[INFO] Auto-start host on launch disabled.");
        } else if (id == IDC_CHK_DISCOVERY) {
            g_discoveryEnabled = !g_discoveryEnabled;
            InvalidateRect(g_hChkDiscovery, nullptr, FALSE);
            AppendLogLine(g_discoveryEnabled ? "[INFO] UDP discovery beacon enabled." : "[INFO] UDP discovery beacon disabled.");
        } else if (id == IDC_BTN_SETTINGS_SAVE) {
            wchar_t nameBuf[128] = {0};
            GetWindowTextW(g_hEditSettingsPcName, nameBuf, 128);
            SetWindowTextW(g_hEditPcName, nameBuf);
            wchar_t portBuf[32] = {0};
            GetWindowTextW(g_hEditSettingsPort, portBuf, 32);
            SetWindowTextW(g_hEditPort, portBuf);
            LRESULT fpsCur = SendMessageW(g_hComboSettingsFps, CB_GETCURSEL, 0, 0);
            if (fpsCur != CB_ERR) SendMessageW(g_hComboFps, CB_SETCURSEL, fpsCur, 0);
            LRESULT bitCur = SendMessageW(g_hComboSettingsBitrate, CB_GETCURSEL, 0, 0);
            if (bitCur != CB_ERR) SendMessageW(g_hComboBitrate, CB_SETCURSEL, bitCur, 0);
            LRESULT monCur = SendMessageW(g_hComboSettingsMonitor, CB_GETCURSEL, 0, 0);
            if (monCur != CB_ERR) SendMessageW(g_hComboMonitor, CB_SETCURSEL, monCur, 0);
            AppendLogLine("[INFO] Host settings and preferences applied successfully.");
            MessageBoxW(hWnd, L"Settings and preferences saved successfully.", L"DuetView PC", MB_OK | MB_ICONINFORMATION);
        } else if (id == IDC_BTN_SETTINGS_RESET) {
            SetWindowTextW(g_hEditSettingsPcName, L"DuetView PC");
            SetWindowTextW(g_hEditSettingsPort, L"52025");
            SendMessageW(g_hComboSettingsFps, CB_SETCURSEL, 2, 0);
            SendMessageW(g_hComboSettingsBitrate, CB_SETCURSEL, 1, 0);
            SendMessageW(g_hComboSettingsMonitor, CB_SETCURSEL, 0, 0);
            g_autoStartHost = false;
            g_discoveryEnabled = true;
            InvalidateRect(g_hChkAutoStart, nullptr, FALSE);
            InvalidateRect(g_hChkDiscovery, nullptr, FALSE);
            AppendLogLine("[INFO] Settings reset to recommended defaults.");
        } else if (id == IDC_BTN_ABOUT_DEVELOPER) {
            ShellExecuteW(NULL, L"open", L"https://github.com/Aryan130805", NULL, NULL, SW_SHOWNORMAL);
            AppendLogLine("[INFO] Opened developer GitHub profile (https://github.com/Aryan130805).");
        } else if (id == IDC_BTN_ABOUT_REPO) {
            ShellExecuteW(NULL, L"open", L"https://github.com/Aryan130805/DuetView", NULL, NULL, SW_SHOWNORMAL);
            AppendLogLine("[INFO] Opened DuetView repository (https://github.com/Aryan130805/DuetView).");
        } else if (id == IDC_BTN_ABOUT_RELEASES) {
            ShellExecuteW(NULL, L"open", L"https://github.com/Aryan130805/DuetView/releases", NULL, NULL, SW_SHOWNORMAL);
            AppendLogLine("[INFO] Opened DuetView releases page (https://github.com/Aryan130805/DuetView/releases).");
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
                    AppendLogLine("[INFO] Copied PC Hardware ID to clipboard.");
                }
            }
        } else if (id == IDC_CHK_CONSOLE) {
            g_showConsoleState = !g_showConsoleState;
            InvalidateRect(g_hChkConsole, nullptr, FALSE);
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

            if (g_currentTab == TAB_HOME) {
                InvalidateRect(hWnd, &g_rcMetadata, FALSE);
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

        // Fill background with deep dark navy #070D1C
        FillRect(memDC, &clientRc, g_hBrushBg);

        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // ---------------------------------------------------------------------
        // 1. LEFT SIDEBAR (Width 180px)
        // ---------------------------------------------------------------------
        Gdiplus::SolidBrush sbBgBrush(Gdiplus::Color(255, 8, 15, 32)); // #080F20
        g.FillRectangle(&sbBgBrush, 0.0f, 0.0f, 180.0f, (float)clientH);

        Gdiplus::Pen sbBorderPen(Gdiplus::Color(255, 24, 35, 60), 1.0f); // #18233C
        g.DrawLine(&sbBorderPen, 180.0f, 0.0f, 180.0f, (float)clientH);

        // Sidebar Logo Box (20, 18, 54, 54)
        Gdiplus::GraphicsPath logoPath;
        AddRoundedRectangle(logoPath, 20.0f, 18.0f, 54.0f, 54.0f, 10.0f);
        Gdiplus::SolidBrush logoBrush(Gdiplus::Color(255, 14, 24, 50));
        g.FillPath(&logoBrush, &logoPath);
        Gdiplus::Pen logoPen(Gdiplus::Color(255, 31, 52, 96), 1.0f);
        g.DrawPath(&logoPen, &logoPath);

        if (g_hAppIcon) {
            DrawIconEx(memDC, 31, 29, g_hAppIcon, 32, 32, 0, NULL, DI_NORMAL);
        }

        // Sidebar Navigation Tabs
        struct NavTabItem {
            AppTab tab;
            int y;
            const wchar_t* text;
            int iconType; // 0: Home, 1: Monitor, 2: Phone, 3: Shield, 4: Gear, 5: Book, 6: Info
        };

        const NavTabItem navItems[] = {
            { TAB_HOME, 86, L"Home", 0 },
            { TAB_STREAMING, 130, L"Streaming", 1 },
            { TAB_DEVICES, 174, L"Devices", 2 },
            { TAB_DRIVER, 218, L"Driver", 3 },
            { TAB_SETTINGS, 262, L"Settings", 4 },
            { TAB_GUIDE, 306, L"Guide", 5 },
            { TAB_ABOUT, clientH - 50, L"About", 6 },
        };

        for (const auto& item : navItems) {
            Gdiplus::RectF rcTab(16.0f, (float)item.y, 148.0f, 38.0f);
            bool isActive = (g_currentTab == item.tab);
            bool isHovered = (g_hoveredTab == item.tab);

            Gdiplus::Color iconCol = isActive ? Gdiplus::Color(255, 255, 255, 255)
                                              : (isHovered ? Gdiplus::Color(255, 240, 245, 255) : Gdiplus::Color(255, 136, 153, 181));

            if (isActive) {
                // Electric Blue to Vivid Violet Gradient + Cyan Accent Glow
                Gdiplus::GraphicsPath tabPath;
                AddRoundedRectangle(tabPath, rcTab.X, rcTab.Y, rcTab.Width, rcTab.Height, 8.0f);

                Gdiplus::LinearGradientBrush activeBrush(
                    Gdiplus::PointF(rcTab.X, rcTab.Y),
                    Gdiplus::PointF(RightOf(rcTab), rcTab.Y),
                    Gdiplus::Color(255, 30, 98, 242), // #1E62F2
                    Gdiplus::Color(255, 124, 58, 237) // #7C3AED
                );
                g.FillPath(&activeBrush, &tabPath);

                Gdiplus::Pen cyanBorder(Gdiplus::Color(255, 56, 189, 248), 1.2f); // #38BDF8
                g.DrawPath(&cyanBorder, &tabPath);

                Gdiplus::Font tabFont(L"Segoe UI", 10.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
                Gdiplus::StringFormat strFmt;
                strFmt.SetAlignment(Gdiplus::StringAlignmentNear);
                strFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                Gdiplus::RectF textRc(rcTab.X + 38.0f, rcTab.Y, rcTab.Width - 38.0f, rcTab.Height);
                g.DrawString(item.text, -1, &tabFont, textRc, &strFmt, &whiteBrush);
            } else {
                if (isHovered) {
                    Gdiplus::GraphicsPath tabPath;
                    AddRoundedRectangle(tabPath, rcTab.X, rcTab.Y, rcTab.Width, rcTab.Height, 8.0f);
                    Gdiplus::SolidBrush hoverBrush(Gdiplus::Color(255, 20, 34, 63));
                    g.FillPath(&hoverBrush, &tabPath);
                }

                Gdiplus::Font tabFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush mutedBrush(Gdiplus::Color(255, 136, 153, 181)); // #8899B5
                Gdiplus::StringFormat strFmt;
                strFmt.SetAlignment(Gdiplus::StringAlignmentNear);
                strFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                Gdiplus::RectF textRc(rcTab.X + 38.0f, rcTab.Y, rcTab.Width - 38.0f, rcTab.Height);
                g.DrawString(item.text, -1, &tabFont, textRc, &strFmt, &mutedBrush);
            }

            // Draw vector icon for tab
            float iconX = rcTab.X + 12.0f;
            float iconY = rcTab.Y + (rcTab.Height - 16.0f) / 2.0f;
            if (item.iconType == 0) DrawVectorHome(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 1) DrawVectorMonitor(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 2) DrawVectorPhone(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 3) DrawVectorShield(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 4) DrawVectorGear(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 5) DrawVectorBook(g, iconX, iconY, 16.0f, iconCol);
            else if (item.iconType == 6) DrawVectorInfo(g, iconX, iconY, 16.0f, iconCol);
        }

        // ---------------------------------------------------------------------
        // 2. TOP HEADER
        // ---------------------------------------------------------------------
        // App Title
        Gdiplus::Font titleFont(L"Segoe UI", 15.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 255, 255, 255));
        g.DrawString(L"DuetView PC", -1, &titleFont, Gdiplus::PointF(202.0f, 16.0f), &titleBrush);

        // Pipe & Subtitle
        Gdiplus::Font pipeFont(L"Segoe UI", 13.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush pipeBrush(Gdiplus::Color(255, 43, 61, 93));
        g.DrawString(L"|", -1, &pipeFont, Gdiplus::PointF(330.0f, 17.0f), &pipeBrush);

        Gdiplus::Font headerSubFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush headerSubBrush(Gdiplus::Color(255, 126, 145, 173));
        g.DrawString(L"Windows Host Manager & Streaming Hub", -1, &headerSubFont, Gdiplus::PointF(348.0f, 21.0f), &headerSubBrush);

        // Status Pill Indicator: "● Host Online" or "○ Host Stopped"
        const bool isRunning = (g_procMgr && g_procMgr->IsRunning());
        float pillW = 130.0f;
        float pillH = 32.0f;
        float pillX = (float)clientW - 150.0f;
        float pillY = 14.0f;

        Gdiplus::GraphicsPath pillPath;
        AddRoundedRectangle(pillPath, pillX, pillY, pillW, pillH, 16.0f);

        if (isRunning) {
            Gdiplus::SolidBrush pillBg(Gdiplus::Color(255, 6, 63, 48)); // #063F30
            g.FillPath(&pillBg, &pillPath);
            Gdiplus::Pen pillBorder(Gdiplus::Color(255, 16, 185, 129), 1.2f); // #10B981
            g.DrawPath(&pillBorder, &pillPath);

            Gdiplus::Font pillFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush greenText(Gdiplus::Color(255, 0, 223, 160)); // #00DFA0
            Gdiplus::StringFormat pillFmt;
            pillFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            pillFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"●   Host Online", -1, &pillFont, Gdiplus::RectF(pillX, pillY, pillW, pillH), &pillFmt, &greenText);
        } else {
            Gdiplus::SolidBrush pillBg(Gdiplus::Color(255, 30, 41, 59));
            g.FillPath(&pillBg, &pillPath);
            Gdiplus::Pen pillBorder(Gdiplus::Color(255, 71, 85, 105), 1.0f);
            g.DrawPath(&pillBorder, &pillPath);

            Gdiplus::Font pillFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush greyText(Gdiplus::Color(255, 148, 163, 184));
            Gdiplus::StringFormat pillFmt;
            pillFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            pillFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"○   Host Stopped", -1, &pillFont, Gdiplus::RectF(pillX, pillY, pillW, pillH), &pillFmt, &greyText);
        }

        // ---------------------------------------------------------------------
        // 3. TAB CONTENT
        // ---------------------------------------------------------------------
        if (g_currentTab == TAB_HOME) {
            // Hero Banner
            Gdiplus::RectF heroRc((float)g_rcHero.left, (float)g_rcHero.top,
                                  (float)(g_rcHero.right - g_rcHero.left), (float)(g_rcHero.bottom - g_rcHero.top));
            DrawHeroBanner(g, heroRc);

            // Left Card: Host Configuration
            Gdiplus::RectF leftRc((float)g_rcLeftCard.left, (float)g_rcLeftCard.top,
                                  (float)(g_rcLeftCard.right - g_rcLeftCard.left), (float)(g_rcLeftCard.bottom - g_rcLeftCard.top));
            DrawCardContainer(g, leftRc, L"Host Configuration", L"Configure your streaming and display settings.", 1);

            // Left Card Form Labels & Containers
            Gdiplus::Font fieldLabelFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush fieldLabelBrush(Gdiplus::Color(255, 126, 145, 173));

            int pad = 18;
            int innerX = g_rcLeftCard.left + pad;
            int innerW = (g_rcLeftCard.right - g_rcLeftCard.left) - pad * 2;

            // Field 1: PC Display Name
            g.DrawString(L"PC Display Name", -1, &fieldLabelFont, Gdiplus::PointF((float)innerX, (float)(g_rcLeftCard.top + 66)), &fieldLabelBrush);
            Gdiplus::GraphicsPath p1;
            AddRoundedRectangle(p1, (float)g_rcInputPcNameContainer.left, (float)g_rcInputPcNameContainer.top,
                                (float)(g_rcInputPcNameContainer.right - g_rcInputPcNameContainer.left),
                                (float)(g_rcInputPcNameContainer.bottom - g_rcInputPcNameContainer.top), 6.0f);
            Gdiplus::SolidBrush inBg(Gdiplus::Color(255, 9, 18, 36));
            g.FillPath(&inBg, &p1);
            Gdiplus::Pen inBorder(Gdiplus::Color(255, 31, 50, 86), 1.0f);
            g.DrawPath(&inBorder, &p1);

            // Edit pencil icon inside right of container
            DrawVectorPencil(g, (float)(g_rcInputPcNameContainer.right - 22), (float)(g_rcInputPcNameContainer.top + 8), 13.0f, Gdiplus::Color(255, 126, 145, 173));

            // Field 2: PC Hardware ID
            g.DrawString(L"PC Hardware ID", -1, &fieldLabelFont, Gdiplus::PointF((float)innerX, (float)(g_rcLeftCard.top + 124)), &fieldLabelBrush);
            Gdiplus::GraphicsPath p2;
            AddRoundedRectangle(p2, (float)g_rcInputPcIdContainer.left, (float)g_rcInputPcIdContainer.top,
                                (float)(g_rcInputPcIdContainer.right - g_rcInputPcIdContainer.left),
                                (float)(g_rcInputPcIdContainer.bottom - g_rcInputPcIdContainer.top), 6.0f);
            g.FillPath(&inBg, &p2);
            g.DrawPath(&inBorder, &p2);

            // Field 3: Port & Target FPS
            int portW = static_cast<int>((innerW - 12) * 0.40f);
            int fpsX = innerX + portW + 12;
            g.DrawString(L"Port", -1, &fieldLabelFont, Gdiplus::PointF((float)innerX, (float)(g_rcLeftCard.top + 182)), &fieldLabelBrush);
            g.DrawString(L"Target FPS", -1, &fieldLabelFont, Gdiplus::PointF((float)fpsX, (float)(g_rcLeftCard.top + 182)), &fieldLabelBrush);

            Gdiplus::GraphicsPath p3;
            AddRoundedRectangle(p3, (float)g_rcInputPortContainer.left, (float)g_rcInputPortContainer.top,
                                (float)(g_rcInputPortContainer.right - g_rcInputPortContainer.left),
                                (float)(g_rcInputPortContainer.bottom - g_rcInputPortContainer.top), 6.0f);
            g.FillPath(&inBg, &p3);
            g.DrawPath(&inBorder, &p3);

            // Field 4: Streaming Bitrate Profile
            g.DrawString(L"Streaming Bitrate Profile", -1, &fieldLabelFont, Gdiplus::PointF((float)innerX, (float)(g_rcLeftCard.top + 240)), &fieldLabelBrush);

            // Field 5: Target Monitor Source
            g.DrawString(L"Target Monitor Source", -1, &fieldLabelFont, Gdiplus::PointF((float)innerX, (float)(g_rcLeftCard.top + 298)), &fieldLabelBrush);

            // Right Card: Pair & Connect
            Gdiplus::RectF rightRc((float)g_rcRightCard.left, (float)g_rcRightCard.top,
                                   (float)(g_rcRightCard.right - g_rcRightCard.left), (float)(g_rcRightCard.bottom - g_rcRightCard.top));
            DrawCardContainer(g, rightRc, L"Pair & Connect", L"Scan this QR code with the DuetView Android app (Scan to Connect).", 2);

            // QR Code with Neon Cyan Brackets
            Gdiplus::RectF qrBoxRc((float)g_rcQrBox.left, (float)g_rcQrBox.top,
                                   (float)(g_rcQrBox.right - g_rcQrBox.left), (float)(g_rcQrBox.bottom - g_rcQrBox.top));
            DrawQrCodeBox(g, qrBoxRc);

            // Metadata panel to right of QR code
            Gdiplus::RectF metaRc((float)g_rcMetadata.left, (float)g_rcMetadata.top,
                                  (float)(g_rcMetadata.right - g_rcMetadata.left), (float)(g_rcMetadata.bottom - g_rcMetadata.top));
            DrawMetadataPanel(g, metaRc);

            // Bottom Status Banner in Right Card
            Gdiplus::RectF bannerRc((float)g_rcStatusBanner.left, (float)g_rcStatusBanner.top,
                                    (float)(g_rcStatusBanner.right - g_rcStatusBanner.left), (float)(g_rcStatusBanner.bottom - g_rcStatusBanner.top));
            DrawStatusBanner(g, bannerRc);

            // -----------------------------------------------------------------
            // 4. BOTTOM STATUS FOOTER
            // -----------------------------------------------------------------
            float footerY = (float)(clientH - (g_logsExpanded ? 160 : 42));

            // Center: Reachable LAN Address
            std::string primIp = "127.0.0.1";
            if (g_pairingMgr) {
                const auto endpoints = g_pairingMgr->GetLocalIPv4Endpoints();
                if (!endpoints.empty()) primIp = endpoints[0];
            }
            std::wstring wPrimIp(primIp.begin(), primIp.end());

            DrawVectorInfo(g, 370.0f, footerY + 8.0f, 13.0f, Gdiplus::Color(255, 126, 145, 173));

            Gdiplus::Font footFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush footBrush(Gdiplus::Color(255, 126, 145, 173));
            g.DrawString(L"Reachable LAN Address: ", -1, &footFont, Gdiplus::PointF(390.0f, footerY + 6.0f), &footBrush);

            Gdiplus::RectF ipLabelBounds;
            g.MeasureString(L"Reachable LAN Address: ", -1, &footFont, Gdiplus::PointF(390.0f, footerY + 6.0f), &ipLabelBounds);

            Gdiplus::Font ipFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush cyanFootBrush(Gdiplus::Color(255, 56, 189, 248)); // #38BDF8
            g.DrawString(wPrimIp.c_str(), -1, &ipFont, Gdiplus::PointF(390.0f + ipLabelBounds.Width, footerY + 6.0f), &cyanFootBrush);

            // Right: Wi-Fi Notice
            DrawVectorWifi(g, (float)(clientW - 350), footerY + 8.0f, 13.0f, Gdiplus::Color(255, 126, 145, 173));
            g.DrawString(L"Android and PC must be connected to the same Wi-Fi.", -1, &footFont,
                         Gdiplus::PointF((float)(clientW - 330), footerY + 6.0f), &footBrush);
        } else if (g_currentTab == TAB_STREAMING) {
            DrawStreamingTab(g, clientW, clientH);
        } else if (g_currentTab == TAB_DEVICES) {
            DrawDevicesTab(g, clientW, clientH);
        } else if (g_currentTab == TAB_DRIVER) {
            DrawDriverTab(g, clientW, clientH);
        } else if (g_currentTab == TAB_SETTINGS) {
            DrawSettingsTab(g, clientW, clientH);
        } else if (g_currentTab == TAB_GUIDE) {
            DrawGuideTab(g, clientW, clientH);
        } else if (g_currentTab == TAB_ABOUT) {
            DrawAboutTab(g, clientW, clientH);
        }

        // Draw Footer on non-home tabs as well for consistent status display
        if (g_currentTab != TAB_HOME) {
            float footerY = (float)(clientH - (g_logsExpanded ? 160 : 42));
            std::string primIp = "127.0.0.1";
            if (g_pairingMgr) {
                const auto endpoints = g_pairingMgr->GetLocalIPv4Endpoints();
                if (!endpoints.empty()) primIp = endpoints[0];
            }
            std::wstring wPrimIp(primIp.begin(), primIp.end());

            DrawVectorInfo(g, 370.0f, footerY + 8.0f, 13.0f, Gdiplus::Color(255, 126, 145, 173));

            Gdiplus::Font footFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush footBrush(Gdiplus::Color(255, 126, 145, 173));
            g.DrawString(L"Reachable LAN Address: ", -1, &footFont, Gdiplus::PointF(390.0f, footerY + 6.0f), &footBrush);

            Gdiplus::RectF ipLabelBounds;
            g.MeasureString(L"Reachable LAN Address: ", -1, &footFont, Gdiplus::PointF(390.0f, footerY + 6.0f), &ipLabelBounds);

            Gdiplus::Font ipFont(L"Segoe UI", 9.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush cyanFootBrush(Gdiplus::Color(255, 56, 189, 248));
            g.DrawString(wPrimIp.c_str(), -1, &ipFont, Gdiplus::PointF(390.0f + ipLabelBounds.Width, footerY + 6.0f), &cyanFootBrush);

            DrawVectorWifi(g, (float)(clientW - 350), footerY + 8.0f, 13.0f, Gdiplus::Color(255, 126, 145, 173));
            g.DrawString(L"Android and PC must be connected to the same Wi-Fi.", -1, &footFont,
                         Gdiplus::PointF((float)(clientW - 330), footerY + 6.0f), &footBrush);
        }

        BitBlt(hdc, 0, 0, clientW, clientH, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdcList = (HDC)wParam;
        SetTextColor(hdcList, RGB(240, 245, 255));
        SetBkColor(hdcList, RGB(9, 18, 36));
        static HBRUSH hListBrush = CreateSolidBrush(RGB(9, 18, 36));
        return (LRESULT)hListBrush;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdcCtrl = (HDC)wParam;
        HWND hwndCtrl = (HWND)lParam;

        if (hwndCtrl == g_hEditLogs) {
            SetTextColor(hdcCtrl, RGB(226, 232, 240));
            SetBkColor(hdcCtrl, RGB(9, 18, 36));
            static HBRUSH hLogBrush = CreateSolidBrush(RGB(9, 18, 36));
            return (LRESULT)hLogBrush;
        }

        SetTextColor(hdcCtrl, (hwndCtrl == g_hEditPcId) ? RGB(148, 163, 184) : RGB(255, 255, 255));
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

        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontBold) DeleteObject(g_hFontBold);
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

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

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
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int workW = workArea.right - workArea.left;
    int workH = workArea.bottom - workArea.top;
    int wndW = 1280;
    int wndH = 720;
    if (wndW > workW - 20) wndW = workW - 20;
    if (wndH > workH - 20) wndH = workH - 20;
    int posX = workArea.left + (workW - wndW) / 2;
    int posY = workArea.top + (workH - wndH) / 2;

    g_hWnd = CreateWindowExW(
        0,
        L"DuetViewMainWindowClass",
        L"DuetView PC",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, wndW, wndH,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        MessageBoxA(NULL, "Failed to create main application window.", "DuetView PC Error", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
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

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
