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
HWND g_hBtnCopyInfo = NULL;
HWND g_hBtnInstallDriver = NULL;
HWND g_hBtnUninstallDriver = NULL;
HWND g_hBtnGuide = NULL;
HWND g_hBtnDisplay = NULL;

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

void DrawVectorGlobe(Gdiplus::Graphics& g, float x, float y, float s, const Gdiplus::Color& c) {
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
        if (g_hBtnInstallDriver && g_hBtnUninstallDriver) {
            if (g_driverInfo.state == DriverState::Ready ||
                g_driverInfo.state == DriverState::InstalledButMonitorNotAvailable) {
                ShowWindow(g_hBtnInstallDriver, SW_HIDE);
                ShowWindow(g_hBtnUninstallDriver, (g_currentTab == TAB_HOME) ? SW_SHOW : SW_HIDE);
            } else {
                ShowWindow(g_hBtnInstallDriver, (g_currentTab == TAB_HOME) ? SW_SHOW : SW_HIDE);
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

void OnOpenWebPageClicked() {
    std::string primIp = "127.0.0.1";
    if (g_pairingMgr) {
        const auto endpoints = g_pairingMgr->GetLocalIPv4Endpoints();
        if (!endpoints.empty()) primIp = endpoints[0];
    }
    std::string url = "http://" + primIp + ":52027/";
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    OnCopyPairingInfoClicked();
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

    // 4 Action Buttons in 2x2 Grid
    int btnGap = 12;
    int btnW = (rInnerW - btnGap) / 2;
    int btnH = 32;
    int row1Y = cardTop + 244;
    int row2Y = cardTop + 284;

    if (g_hBtnRegen) SetWindowPos(g_hBtnRegen, NULL, rInnerX, row1Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnCopyInfo) SetWindowPos(g_hBtnCopyInfo, NULL, rInnerX + btnW + btnGap, row1Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnInstallDriver) SetWindowPos(g_hBtnInstallDriver, NULL, rInnerX, row2Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnUninstallDriver) SetWindowPos(g_hBtnUninstallDriver, NULL, rInnerX, row2Y, btnW, btnH, SWP_NOZORDER);
    if (g_hBtnGuide) SetWindowPos(g_hBtnGuide, NULL, rInnerX + btnW + btnGap, row2Y, btnW, btnH, SWP_NOZORDER);

    // Status Banner
    g_rcStatusBanner = { rInnerX, cardTop + 328, rInnerX + rInnerW, cardTop + 382 };

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
        g_hBtnRegen = CreateWindowW(L"BUTTON", L"Regenerate QR",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            625, 365, 175, 32, hWnd, (HMENU)IDC_BTN_REGEN_QR, NULL, NULL);

        g_hBtnCopyInfo = CreateWindowW(L"BUTTON", L"Open Web Page  ↗",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            810, 365, 175, 32, hWnd, (HMENU)IDC_BTN_COPY_INFO, NULL, NULL);

        g_hBtnInstallDriver = CreateWindowW(L"BUTTON", L"Install Virtual Display Driver",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_INSTALL_DRIVER, NULL, NULL);

        g_hBtnUninstallDriver = CreateWindowW(L"BUTTON", L"Uninstall Driver",
            WS_CHILD | BS_OWNERDRAW,
            625, 405, 240, 32, hWnd, (HMENU)IDC_BTN_UNINSTALL_DRIVER, NULL, NULL);

        g_hBtnGuide = CreateWindowW(L"BUTTON", L"Guide",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            875, 405, 110, 32, hWnd, (HMENU)IDC_BTN_DRIVER_GUIDE, NULL, NULL);

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

        SetTimer(hWnd, IDT_TICK_TIMER, 1000, NULL);
        AppendLogLine("[INFO] DuetView PC Desktop Host Manager Initialized.");
        PostMessageW(hWnd, WM_USER_FIRST_LAUNCH, 0, 0);
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
        if (g_procMgr && !g_procMgr->IsRunning()) {
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

            if (pDIS->CtlID == IDC_BTN_START_STOP) {
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

            if (pDIS->CtlID == IDC_CHK_CONSOLE) {
                // Custom Checkbox
                float boxY = (h - 16.0f) / 2.0f;
                Gdiplus::GraphicsPath boxPath;
                AddRoundedRectangle(boxPath, 2.0f, boxY, 16.0f, 16.0f, 4.0f);
                Gdiplus::SolidBrush boxBg(Gdiplus::Color(255, 9, 18, 36));
                g.FillPath(&boxBg, &boxPath);
                Gdiplus::Pen boxPen(Gdiplus::Color(255, 31, 50, 86), 1.2f);
                g.DrawPath(&boxPen, &boxPath);

                if (g_showConsoleState) {
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
                g.DrawString(L"Show raw engine console window (Diagnostic)", -1, &chkFont,
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

            if (pDIS->CtlID == IDC_BTN_DISPLAY_SETTINGS) {
                DrawVectorMonitor(g, 12.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(32.0f, 0.0f, w - 36.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Display Settings (Win + P)", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_COPY_PCID) {
                DrawVectorCopy(g, 10.0f, (h - 13.0f) / 2.0f, 13.0f, iconCol);
                Gdiplus::RectF txtRc(27.0f, 0.0f, w - 30.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Copy", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_REGEN_QR) {
                DrawVectorRefresh(g, 16.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(36.0f, 0.0f, w - 40.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Regenerate QR", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_COPY_INFO) {
                DrawVectorGlobe(g, 16.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(36.0f, 0.0f, w - 40.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Open Web Page  ↗", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_INSTALL_DRIVER) {
                DrawVectorShield(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Install Virtual Display Driver", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_UNINSTALL_DRIVER) {
                DrawVectorShield(g, 14.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(34.0f, 0.0f, w - 38.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Uninstall Driver", -1, &btnFont, txtRc, &fmt, &textBrush);
            } else if (pDIS->CtlID == IDC_BTN_DRIVER_GUIDE) {
                DrawVectorBook(g, 16.0f, (h - 14.0f) / 2.0f, 14.0f, iconCol);
                Gdiplus::RectF txtRc(36.0f, 0.0f, w - 40.0f, h);
                Gdiplus::StringFormat fmt;
                fmt.SetAlignment(Gdiplus::StringAlignmentNear);
                fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(L"Guide", -1, &btnFont, txtRc, &fmt, &textBrush);
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
        if (id == IDC_BTN_START_STOP) {
            OnStartStopClicked();
        } else if (id == IDC_BTN_REGEN_QR) {
            OnRegenerateQrClicked();
        } else if (id == IDC_BTN_COPY_INFO) {
            OnOpenWebPageClicked();
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
        } else {
            // Other Tab Views
            Gdiplus::RectF otherRc(200.0f, 60.0f, (float)(clientW - 220), (float)(clientH - 120));
            const wchar_t* tabTitle = L"DuetView PC";
            const wchar_t* tabSub = L"";

            if (g_currentTab == TAB_STREAMING) {
                tabTitle = L"Streaming Pipeline & Engine Telemetry";
                tabSub = L"Real-time DXGI capture and Media Foundation H.264 hardware encoder metrics.";
            } else if (g_currentTab == TAB_DEVICES) {
                tabTitle = L"Connected & Paired Android Devices";
                tabSub = L"Manage devices permitted to stream and control this PC.";
            } else if (g_currentTab == TAB_DRIVER) {
                tabTitle = L"Windows Virtual Display Driver (IddCx)";
                tabSub = L"Configure the optional indirect display driver for extended second monitor mode.";
            } else if (g_currentTab == TAB_SETTINGS) {
                tabTitle = L"Host Settings & Preferences";
                tabSub = L"Configure application behavior, autostart, and network interface bindings.";
            } else if (g_currentTab == TAB_GUIDE) {
                tabTitle = L"Quick Connection Guide";
                tabSub = L"Step-by-step instructions for connecting your Android device.";
            } else if (g_currentTab == TAB_ABOUT) {
                tabTitle = L"About DuetView PC";
                tabSub = L"Free, high-performance second display software for Windows and Android.";
            }

            DrawCardContainer(g, otherRc, tabTitle, tabSub, 1);

            Gdiplus::Font bodyBold(L"Segoe UI", 10.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::Font bodyNormal(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush cyanTxt(Gdiplus::Color(255, 56, 189, 248));
            Gdiplus::SolidBrush whiteTxt(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::SolidBrush mutedTxt(Gdiplus::Color(255, 126, 145, 173));

            float curY = 130.0f;
            if (g_currentTab == TAB_STREAMING) {
                g.DrawString(L"Capture & Pipeline Status", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"• Screen Capture API: Direct3D 11 & DXGI Desktop Duplication (Hardware Accelerated)", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Video Encoder: Windows Media Foundation (IMFTransform) H.264 Baseline/High Profile", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Packet Framing: SecondScreen SS02 with SPS/PPS Configuration Insertion", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Transport Layer: Low-latency non-blocking TCP socket stream", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
            } else if (g_currentTab == TAB_DEVICES) {
                g.DrawString(L"Network Endpoints & Ports", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"• Automatic Discovery Port: 52026 (UDP)", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Pairing Authentication Port: 52027 (TCP)", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Primary Video Streaming Port: 52025 (TCP)", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
            } else if (g_currentTab == TAB_DRIVER) {
                g.DrawString(L"Virtual Display Driver Details", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"• Architecture: Microsoft Indirect Display Driver (IddCx / UMDF 2.x)", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Hardware ID: Root\\SecondScreenIdd", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Virtual Modes: 1920x1080 @ 60Hz, 1280x720 @ 60Hz", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
            } else if (g_currentTab == TAB_SETTINGS) {
                g.DrawString(L"Host Settings & Preferences", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"• Auto-Start Streaming: Host automatically starts on PC application launch.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• LAN Discovery Broadcasts: Broadcasts UDP beacon every 2 seconds for Android discovery.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"• Touch Input Injection: Translates normalized touch points to absolute Windows screen coordinates.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
            } else if (g_currentTab == TAB_GUIDE) {
                g.DrawString(L"How to Connect", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"1. Connect PC and Android to the same Wi-Fi network (5 GHz recommended).", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"2. Open DuetView Android app and tap 'Scan to Connect'.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"3. Point your camera at the QR code on the Home screen to begin streaming instantly.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
            } else if (g_currentTab == TAB_ABOUT) {
                g.DrawString(L"DuetView PC — Version 0.4.0 (Unified Release)", -1, &bodyBold, Gdiplus::PointF(225.0f, curY), &cyanTxt);
                curY += 26.0f;
                g.DrawString(L"Turn your Android device into a high-performance second monitor for your Windows computer.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &whiteTxt);
                curY += 24.0f;
                g.DrawString(L"Powered by Direct3D 11, DXGI Desktop Duplication, and Windows Media Foundation H.264.", -1, &bodyNormal, Gdiplus::PointF(225.0f, curY), &mutedTxt);
            }
        }

        // BitBlt memory backbuffer to primary window surface
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
