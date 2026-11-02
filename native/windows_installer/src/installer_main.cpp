#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Control Identifiers
#define IDC_EDIT_TARGET_DIR     2001
#define IDC_BTN_BROWSE          2002
#define IDC_CHK_DESKTOP_SHORTCUT 2003
#define IDC_CHK_START_MENU      2004
#define IDC_CHK_LAUNCH_NOW      2005
#define IDC_BTN_INSTALL         2006
#define IDC_BTN_CANCEL          2007
#define IDC_PROGRESS_BAR        2008
#define IDC_STATIC_STATUS       2009

namespace {

// Slate Dark Theme matching DuetView PC UI
const COLORREF COLOR_BG          = RGB(15, 23, 42);    // #0f172a
const COLORREF COLOR_CARD_BG     = RGB(30, 41, 59);    // #1e293b
const COLORREF COLOR_CARD_BORDER = RGB(51, 65, 85);    // #334155
const COLORREF COLOR_TEXT_MAIN   = RGB(248, 250, 252); // #f8fafc
const COLORREF COLOR_TEXT_MUTED  = RGB(148, 163, 184); // #94a3b8
const COLORREF COLOR_ACCENT      = RGB(56, 189, 248);  // #38bdf8
const COLORREF COLOR_SUCCESS     = RGB(16, 185, 129);  // #10b981

HWND g_hWnd = NULL;
HWND g_hEditTargetDir = NULL;
HWND g_hBtnBrowse = NULL;
HWND g_hChkDesktop = NULL;
HWND g_hChkStartMenu = NULL;
HWND g_hChkLaunch = NULL;
HWND g_hBtnInstall = NULL;
HWND g_hBtnCancel = NULL;
HWND g_hProgressBar = NULL;
HWND g_hStaticStatus = NULL;

HFONT g_hFontTitle = NULL;
HFONT g_hFontSub = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontNormal = NULL;

HBRUSH g_hBrushBg = NULL;
HBRUSH g_hBrushCardBg = NULL;

bool g_silentMode = false;

std::wstring GetDefaultInstallDirectory() {
    wchar_t localAppData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        return std::wstring(localAppData) + L"\\Programs\\DuetView";
    }
    return L"C:\\Program Files\\DuetView";
}

std::wstring GetInstallerSourceDir() {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    return std::wstring(exePath);
}

bool CreateShellShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath,
                         const std::wstring& iconPath, const std::wstring& description,
                         const std::wstring& workingDir) {
    HRESULT hr = CoInitialize(NULL);
    bool coInitialized = SUCCEEDED(hr);

    IShellLinkW* psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        psl->SetDescription(description.c_str());
        if (!workingDir.empty()) {
            psl->SetWorkingDirectory(workingDir.c_str());
        }
        if (!iconPath.empty()) {
            psl->SetIconLocation(iconPath.c_str(), 0);
        }

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }

    if (coInitialized) {
        CoUninitialize();
    }
    return SUCCEEDED(hr);
}

void RegisterInAddRemovePrograms(const std::wstring& installDir) {
    HKEY hKey = NULL;
    const wchar_t* regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DuetViewPC";

    if (RegCreateKeyExW(HKEY_CURRENT_USER, regPath, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

        const wchar_t* displayName = L"DuetView PC";
        const wchar_t* displayVer = L"0.4.0";
        const wchar_t* publisher = L"DuetView Project";
        std::wstring exePath = installDir + L"\\DuetView-PC.exe";
        std::wstring icoPath = installDir + L"\\duetview.ico";
        std::wstring uninstPath = L"cmd.exe /c \"" + installDir + L"\\uninstall.bat\"";

        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (const BYTE*)displayName, (DWORD)((wcslen(displayName) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)displayVer, (DWORD)((wcslen(displayVer) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (const BYTE*)publisher, (DWORD)((wcslen(publisher) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)installDir.c_str(), (DWORD)((installDir.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)icoPath.c_str(), (DWORD)((icoPath.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninstPath.c_str(), (DWORD)((uninstPath.length() + 1) * sizeof(wchar_t)));

        DWORD sizeKb = 15360; // ~15 MB
        RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&sizeKb, sizeof(sizeKb));

        RegCloseKey(hKey);
    }
}

void GenerateUninstallerScript(const std::wstring& installDir) {
    std::wstring uninstBat = installDir + L"\\uninstall.bat";
    FILE* fp = _wfopen(uninstBat.c_str(), L"w");
    if (!fp) return;

    fwprintf(fp, L"@echo off\n");
    fwprintf(fp, L"echo Uninstalling DuetView PC...\n");
    fwprintf(fp, L"taskkill /f /im DuetView-PC.exe >nul 2>&1\n");
    fwprintf(fp, L"taskkill /f /im secondscreen_engine.exe >nul 2>&1\n");
    fwprintf(fp, L"reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DuetViewPC\" /f >nul 2>&1\n");
    fwprintf(fp, L"reg delete \"HKCU\\Software\\DuetView\" /f >nul 2>&1\n");
    
    // Remove shortcuts
    wchar_t desktop[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop))) {
        fwprintf(fp, L"del /f /q \"%s\\DuetView PC.lnk\" >nul 2>&1\n", desktop);
    }
    wchar_t programs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programs))) {
        fwprintf(fp, L"del /f /q \"%s\\DuetView PC.lnk\" >nul 2>&1\n", programs);
    }

    fwprintf(fp, L"echo DuetView PC uninstalled successfully.\n");
    fwprintf(fp, L"timeout /t 2 >nul\n");
    fwprintf(fp, L"rmdir /s /q \"%s\" >nul 2>&1\n", installDir.c_str());
    fclose(fp);
}

bool PerformInstallation(const std::wstring& targetDir, bool createDesktopShortcut,
                         bool createStartMenuShortcut, bool launchAfter,
                         std::wstring& outErrorMessage) {
    try {
        std::error_code ec;
        fs::create_directories(targetDir, ec);
        if (ec) {
            outErrorMessage = L"Failed to create target directory: " + std::wstring(ec.message().begin(), ec.message().end());
            return false;
        }

        fs::path targetDriverDir = fs::path(targetDir) / "driver";
        fs::create_directories(targetDriverDir, ec);

        std::wstring srcDir = GetInstallerSourceDir();

        // 1. Copy Application Binary (DuetView-PC.exe - Unified UI & Streaming Engine)
        std::vector<std::pair<std::wstring, std::wstring>> filesToCopy;

        std::wstring duetViewSrc = srcDir + L"\\DuetView-PC.exe";
        if (fs::exists(duetViewSrc)) {
            filesToCopy.push_back({ duetViewSrc, targetDir + L"\\DuetView-PC.exe" });
        }

        // Icon check
        std::wstring icoSrc = srcDir + L"\\duetview.ico";
        if (!fs::exists(icoSrc)) {
            icoSrc = srcDir + L"\\..\\..\\resources\\duetview.ico";
        }
        if (fs::exists(icoSrc)) {
            filesToCopy.push_back({ icoSrc, targetDir + L"\\duetview.ico" });
        }

        // Driver files check
        std::vector<std::pair<std::wstring, std::wstring>> driverFiles = {
            { srcDir + L"\\driver\\SecondScreenIdd.inf", targetDriverDir.wstring() + L"\\SecondScreenIdd.inf" },
            { srcDir + L"\\driver\\install_driver.bat", targetDriverDir.wstring() + L"\\install_driver.bat" },
            { srcDir + L"\\driver\\uninstall_driver.bat", targetDriverDir.wstring() + L"\\uninstall_driver.bat" },
            { srcDir + L"\\driver\\SecondScreenIdd.dll", targetDriverDir.wstring() + L"\\SecondScreenIdd.dll" },
            { srcDir + L"\\driver\\SecondScreenIdd.cat", targetDriverDir.wstring() + L"\\SecondScreenIdd.cat" },
            { srcDir + L"\\driver\\SecondScreenIdd.cer", targetDriverDir.wstring() + L"\\SecondScreenIdd.cer" },
            { srcDir + L"\\driver\\devcon.exe", targetDriverDir.wstring() + L"\\devcon.exe" },
        };

        // Fallback check source directory for INF/DLL if not in build/driver
        for (auto& df : driverFiles) {
            if (fs::exists(df.first)) {
                filesToCopy.push_back(df);
            } else {
                // Try source project folder fallback
                fs::path filename = fs::path(df.first).filename();
                std::wstring fallbackSrc = srcDir + L"\\..\\virtual_display_driver\\" + filename.wstring();
                if (fs::exists(fallbackSrc)) {
                    filesToCopy.push_back({ fallbackSrc, df.second });
                } else {
                    std::wstring relReleaseSrc = srcDir + L"\\..\\virtual_display_driver\\x64\\Release\\" + filename.wstring();
                    if (fs::exists(relReleaseSrc)) {
                        filesToCopy.push_back({ relReleaseSrc, df.second });
                    }
                }
            }
        }

        // Perform copy operations
        for (const auto& pair : filesToCopy) {
            if (fs::exists(pair.first)) {
                fs::copy_file(pair.first, pair.second, fs::copy_options::overwrite_existing, ec);
            }
        }

        // Auto-register driver in Windows PnP if INF and DLL are deployed
        std::wstring installedInf = targetDriverDir.wstring() + L"\\SecondScreenIdd.inf";
        std::wstring installedDll = targetDriverDir.wstring() + L"\\SecondScreenIdd.dll";
        std::wstring installedBat = targetDriverDir.wstring() + L"\\install_driver.bat";
        if (fs::exists(installedInf) && fs::exists(installedDll)) {
            if (fs::exists(installedBat)) {
                std::wstring batParams = L"/c \"" + installedBat + L"\"";
                ShellExecuteW(NULL, L"runas", L"cmd.exe", batParams.c_str(), targetDriverDir.c_str(), SW_SHOWNORMAL);
            } else {
                std::wstring pnpCmd = L"/add-driver \"" + installedInf + L"\" /install";
                ShellExecuteW(NULL, L"runas", L"pnputil.exe", pnpCmd.c_str(), NULL, SW_SHOWNORMAL);
            }
        }

        // Generate uninstaller batch script
        GenerateUninstallerScript(targetDir);

        // Register in Windows Add/Remove Programs
        RegisterInAddRemovePrograms(targetDir);

        // Create Start Menu shortcut
        std::wstring exeTarget = targetDir + L"\\DuetView-PC.exe";
        std::wstring icoTarget = targetDir + L"\\duetview.ico";

        if (createStartMenuShortcut) {
            wchar_t startMenuPath[MAX_PATH] = {0};
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath))) {
                std::wstring lnkPath = std::wstring(startMenuPath) + L"\\DuetView PC.lnk";
                CreateShellShortcut(exeTarget, lnkPath, icoTarget,
                                    L"DuetView PC - Virtual Display & Screen Mirroring Host", targetDir);
            }
        }

        // Create Desktop shortcut
        if (createDesktopShortcut) {
            wchar_t desktopPath[MAX_PATH] = {0};
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath))) {
                std::wstring lnkPath = std::wstring(desktopPath) + L"\\DuetView PC.lnk";
                CreateShellShortcut(exeTarget, lnkPath, icoTarget,
                                    L"DuetView PC - Virtual Display & Screen Mirroring Host", targetDir);
            }
        }

        // Launch application if requested
        if (launchAfter) {
            ShellExecuteW(NULL, L"open", exeTarget.c_str(), NULL, targetDir.c_str(), SW_SHOWNORMAL);
        }

        return true;
    } catch (const std::exception& ex) {
        outErrorMessage = L"Installation exception: " + std::wstring(ex.what(), ex.what() + strlen(ex.what()));
        return false;
    }
}

void OnBrowseClicked(HWND hWnd) {
    BROWSEINFOW bi{};
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"Select Installation Directory for DuetView PC:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        wchar_t path[MAX_PATH] = {0};
        if (SHGetPathFromIDListW(pidl, path)) {
            SetWindowTextW(g_hEditTargetDir, path);
        }
        CoTaskMemFree(pidl);
    }
}

void OnInstallClicked(HWND hWnd) {
    wchar_t targetDirBuf[MAX_PATH] = {0};
    GetWindowTextW(g_hEditTargetDir, targetDirBuf, MAX_PATH);

    std::wstring targetDir = targetDirBuf;
    if (targetDir.empty()) {
        MessageBoxW(hWnd, L"Please enter a valid installation directory.", L"DuetView Setup", MB_ICONWARNING | MB_OK);
        return;
    }

    bool chkDesktop = (SendMessageW(g_hChkDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool chkStartMenu = (SendMessageW(g_hChkStartMenu, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool chkLaunch = (SendMessageW(g_hChkLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);

    EnableWindow(g_hBtnInstall, FALSE);
    EnableWindow(g_hBtnCancel, FALSE);
    SendMessageW(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g_hProgressBar, PBM_SETPOS, 30, 0);
    SetWindowTextW(g_hStaticStatus, L"Installing files and bundling driver package...");

    std::wstring errMsg;
    bool ok = PerformInstallation(targetDir, chkDesktop, chkStartMenu, chkLaunch, errMsg);

    SendMessageW(g_hProgressBar, PBM_SETPOS, 100, 0);

    if (ok) {
        SetWindowTextW(g_hStaticStatus, L"Installation completed successfully!");
        MessageBoxW(hWnd,
            L"DuetView PC has been successfully installed!\n\n"
            L"The application and virtual display driver bundle have been deployed.\n"
            L"Shortcuts have been created and the application is ready to use.",
            L"DuetView PC Setup",
            MB_ICONINFORMATION | MB_OK);
        PostQuitMessage(0);
    } else {
        SetWindowTextW(g_hStaticStatus, L"Installation failed.");
        MessageBoxW(hWnd, (L"Installation failed:\n" + errMsg).c_str(), L"DuetView Setup Error", MB_ICONERROR | MB_OK);
        EnableWindow(g_hBtnInstall, TRUE);
        EnableWindow(g_hBtnCancel, TRUE);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hWnd;

        INITCOMMONCONTROLSEX icex{};
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);

        // Fonts
        g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontSub = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hBrushBg = CreateSolidBrush(COLOR_BG);
        g_hBrushCardBg = CreateSolidBrush(COLOR_CARD_BG);

        // Target Directory Controls
        CreateWindowW(L"STATIC", L"Installation Directory:", WS_CHILD | WS_VISIBLE,
                      30, 95, 300, 20, hWnd, NULL, NULL, NULL);

        std::wstring defDir = GetDefaultInstallDirectory();
        g_hEditTargetDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defDir.c_str(),
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           30, 120, 390, 26, hWnd, (HMENU)IDC_EDIT_TARGET_DIR, NULL, NULL);
        SendMessageW(g_hEditTargetDir, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     430, 120, 90, 26, hWnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);
        SendMessageW(g_hBtnBrowse, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Checkboxes
        g_hChkDesktop = CreateWindowW(L"BUTTON", L"Create Desktop Shortcut",
                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      30, 165, 300, 22, hWnd, (HMENU)IDC_CHK_DESKTOP_SHORTCUT, NULL, NULL);
        SendMessageW(g_hChkDesktop, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hChkDesktop, BM_SETCHECK, BST_CHECKED, 0);

        g_hChkStartMenu = CreateWindowW(L"BUTTON", L"Create Start Menu Shortcut",
                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                        30, 195, 300, 22, hWnd, (HMENU)IDC_CHK_START_MENU, NULL, NULL);
        SendMessageW(g_hChkStartMenu, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hChkStartMenu, BM_SETCHECK, BST_CHECKED, 0);

        g_hChkLaunch = CreateWindowW(L"BUTTON", L"Launch DuetView PC after installation",
                                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     30, 225, 300, 22, hWnd, (HMENU)IDC_CHK_LAUNCH_NOW, NULL, NULL);
        SendMessageW(g_hChkLaunch, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(g_hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);

        // Progress & Status
        g_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                                         WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                         30, 265, 490, 16, hWnd, (HMENU)IDC_PROGRESS_BAR, NULL, NULL);

        g_hStaticStatus = CreateWindowW(L"STATIC", L"Ready to install DuetView PC with bundled driver package.",
                                        WS_CHILD | WS_VISIBLE,
                                        30, 288, 490, 20, hWnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL);
        SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Install & Cancel Buttons
        g_hBtnInstall = CreateWindowW(L"BUTTON", L"Install",
                                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                      320, 325, 95, 32, hWnd, (HMENU)IDC_BTN_INSTALL, NULL, NULL);
        SendMessageW(g_hBtnInstall, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        g_hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     425, 325, 95, 32, hWnd, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
        SendMessageW(g_hBtnCancel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_BROWSE) {
            OnBrowseClicked(hWnd);
        } else if (id == IDC_BTN_INSTALL) {
            OnInstallClicked(hWnd);
        } else if (id == IDC_BTN_CANCEL) {
            PostQuitMessage(0);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        FillRect(hdc, &rcClient, g_hBrushBg);

        // Top Banner Area
        RECT rcBanner = { 0, 0, rcClient.right, 75 };
        FillRect(hdc, &rcBanner, g_hBrushCardBg);

        HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_CARD_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, 75, NULL);
        LineTo(hdc, rcClient.right, 75);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT_MAIN);
        SelectObject(hdc, g_hFontTitle);
        TextOutW(hdc, 30, 15, L"DuetView PC Setup", 17);

        SetTextColor(hdc, COLOR_TEXT_MUTED);
        SelectObject(hdc, g_hFontSub);
        TextOutW(hdc, 30, 45, L"Install DuetView PC and bundle the Virtual Display Driver package", 65);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, COLOR_TEXT_MAIN);
        SetBkColor(hdcStatic, COLOR_BG);
        return (INT_PTR)g_hBrushBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, COLOR_TEXT_MAIN);
        SetBkColor(hdcEdit, COLOR_CARD_BG);
        return (INT_PTR)g_hBrushCardBg;
    }

    case WM_DESTROY: {
        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hFontSub) DeleteObject(g_hFontSub);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hBrushBg) DeleteObject(g_hBrushBg);
        if (g_hBrushCardBg) DeleteObject(g_hBrushCardBg);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    std::wstring cmdLine = GetCommandLineW();

    // Handle Silent / Automated installation (supports /S or /SILENT with optional /D=<dir>)
    if (cmdLine.find(L"/S") != std::wstring::npos || cmdLine.find(L"/SILENT") != std::wstring::npos) {
        std::wstring targetDir = GetDefaultInstallDirectory();
        size_t dPos = cmdLine.find(L"/D=");
        if (dPos != std::wstring::npos) {
            targetDir = cmdLine.substr(dPos + 3);
            // Trim any surrounding quotes or trailing spaces
            while (!targetDir.empty() && (targetDir.front() == L' ' || targetDir.front() == L'"')) {
                targetDir.erase(0, 1);
            }
            while (!targetDir.empty() && (targetDir.back() == L' ' || targetDir.back() == L'"')) {
                targetDir.pop_back();
            }
        }
        std::wstring errMsg;
        bool ok = PerformInstallation(targetDir, true, true, false, errMsg);
        return ok ? 0 : 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DuetViewPCSetupClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    RegisterClassExW(&wc);

    int width = 565;
    int height = 410;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"DuetView PC Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
