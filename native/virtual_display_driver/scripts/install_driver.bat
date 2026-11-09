@echo off
setlocal
echo ========================================================
echo   DuetView Virtual Display Driver Installation      
echo ========================================================
echo.

:: Check for administrative privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Administrative privileges required.
    echo Please right-click this script and select 'Run as administrator'.
    if not "%~1"=="--silent" if not "%~1"=="/silent" pause
    exit /b 1
)

:: Locate SecondScreenIdd.inf robustly
set "DRIVER_DIR=%~dp0"
if exist "%DRIVER_DIR%SecondScreenIdd.inf" goto FoundInf

set "DRIVER_DIR=%~dp0.."
if exist "%DRIVER_DIR%\SecondScreenIdd.inf" goto FoundInf

set "DRIVER_DIR=%~dp0driver"
if exist "%DRIVER_DIR%\SecondScreenIdd.inf" goto FoundInf

set "DRIVER_DIR=%~dp0..\driver"
if exist "%DRIVER_DIR%\SecondScreenIdd.inf" goto FoundInf

set "DRIVER_DIR=D:\Second Display\native\virtual_display_driver"
if exist "%DRIVER_DIR%\SecondScreenIdd.inf" goto FoundInf

echo [ERROR] SecondScreenIdd.inf could not be found.
echo Searched in: %~dp0
if not "%~1"=="--silent" if not "%~1"=="/silent" pause
exit /b 1

:FoundInf
cd /d "%DRIVER_DIR%"
echo [1/3] Checking driver package in "%cd%"...
if not exist "SecondScreenIdd.dll" (
    echo [ERROR] SecondScreenIdd.dll not found in "%cd%".
    if not "%~1"=="--silent" if not "%~1"=="/silent" pause
    exit /b 1
)
echo Found SecondScreenIdd.inf and SecondScreenIdd.dll

echo [2/4] Installing driver package to Windows Driver Store...
pnputil /add-driver SecondScreenIdd.inf /install
if %errorlevel% neq 0 (
    echo.
    echo [WARNING] Driver installation returned code %errorlevel%.
    echo Note: If test-signing mode was just turned on, ensure you restarted your PC.
) else (
    echo.
    echo [SUCCESS] Driver package added to Driver Store!
)

echo.
echo [3/4] Creating and activating Virtual Display device node (Root\SecondScreenIdd)...
if exist "devcon.exe" (
    devcon.exe install SecondScreenIdd.inf Root\SecondScreenIdd
) else if exist "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" (
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" install SecondScreenIdd.inf Root\SecondScreenIdd
) else (
    echo [INFO] devcon.exe not found. Windows will enumerate device on restart.
)

echo.
echo [4/4] Checking Windows PnP display devices...
pnputil /enum-devices /class Display

echo.
echo ========================================================
echo Installation routine completed.
echo Open Windows Display Settings (ms-settings:display) or press
echo Win + P and select 'Extend' to activate the virtual monitor.
echo ========================================================
if /i "%~1"=="--silent" exit /b 0
if /i "%~1"=="/silent" exit /b 0
if /i "%~1"=="-s" exit /b 0
pause
