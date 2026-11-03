@echo off
setlocal EnableDelayedExpansion
echo ========================================================
echo   DuetView Virtual Display Driver Removal           
echo ========================================================
echo.

:: Check for administrative privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Administrative privileges required.
    echo Please right-click this script and select 'Run as administrator'.
    pause
    exit /b 1
)

echo [1/3] Removing virtual display device node...
if exist "%~dp0devcon.exe" (
    "%~dp0devcon.exe" remove Root\SecondScreenIdd
    "%~dp0devcon.exe" remove "Root\SecondScreenIdd*"
) else if exist "%~dp0..\devcon.exe" (
    "%~dp0..\devcon.exe" remove Root\SecondScreenIdd
    "%~dp0..\devcon.exe" remove "Root\SecondScreenIdd*"
)
pnputil /remove-device "Root\SecondScreenIdd" >nul 2>&1

echo.
echo [2/3] Searching and removing driver packages from Windows Driver Store...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$drivers = pnputil /enum-drivers; for ($i=0; $i -lt $drivers.Count; $i++) { if ($drivers[$i] -match 'secondscreenidd') { for ($j=$i; $j -ge 0; $j--) { if ($drivers[$j] -match '(oem\d+\.inf)') { Write-Host ('Deleting package: ' + $matches[1]); pnputil /delete-driver $matches[1] /uninstall /force; break } } } }"

echo.
echo [3/3] Checking remaining display devices...
pnputil /enum-devices /class Display

echo.
echo ========================================================
echo Uninstallation completed. Virtual display removed.
echo ========================================================
pause
