@echo off
setlocal
echo ========================================================
echo   SecondScreen Virtual Display Driver Removal           
echo ========================================================
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Administrative privileges required.
    echo Please right-click this script and select 'Run as administrator'.
    pause
    exit /b 1
)

echo [1/3] Removing SecondScreen virtual display device node...
if exist "%~dp0devcon.exe" (
    "%~dp0devcon.exe" remove Root\SecondScreenIdd
) else if exist "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" (
    "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\devcon.exe" remove Root\SecondScreenIdd
)
pnputil /remove-device "Root\SecondScreenIdd" >nul 2>&1

echo.
echo [2/3] Searching for installed SecondScreen driver packages...
for /f "tokens=1,2 delims=:" %%a in ('pnputil /enum-drivers ^| findstr /i "SecondScreenIdd.inf"') do (
    set oem_match=%%b
)

echo [3/3] Removing driver package from Windows Driver Store...
pnputil /delete-driver SecondScreenIdd.inf /uninstall /force >nul 2>&1

echo.
echo ========================================================
echo Uninstallation routine completed.
echo Virtual display removed.
echo ========================================================
pause

