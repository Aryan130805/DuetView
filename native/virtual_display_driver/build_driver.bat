@echo off
setlocal enabledelayedexpansion
echo ===================================================
echo   DuetView IddCx Virtual Display Driver Build
echo ===================================================

:: Locate vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found.
    exit /b 1
)

:: Find MSVC installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_DIR=%%i"
)

if not defined VS_DIR (
    echo [ERROR] Visual Studio / Build Tools C++ installation not found.
    exit /b 1
)

set "VCVARS=%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found at "%VCVARS%".
    exit /b 1
)

echo [1/3] Initializing MSVC x64 build environment...
call "%VCVARS%" >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Failed to initialize vcvars64.bat.
    exit /b 1
)

cd /d "%~dp0"

echo [2/3] Compiling driver source files (Driver.cpp, Device.cpp, Monitor.cpp)...
set "WDK_INC=C:\Program Files (x86)\Windows Kits\10\Include"
set "WDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib"

cl /c /EHsc /O2 /W3 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_AMD64_ /D_WIN64 /DIDDCX_VERSION_MAJOR=1 /DIDDCX_VERSION_MINOR=4 ^
   /I"%WDK_INC%\wdf\umdf\2.25" ^
   /I"%WDK_INC%\10.0.28000.0\um\iddcx\1.4" ^
   /I"%WDK_INC%\10.0.28000.0\um" ^
   /I"%WDK_INC%\10.0.28000.0\shared" ^
   Driver.cpp Device.cpp Monitor.cpp
if %errorlevel% neq 0 (
    echo [ERROR] Driver compilation failed.
    exit /b 1
)

echo [3/3] Linking SecondScreenIdd.dll...
link /DLL /SUBSYSTEM:WINDOWS /OUT:SecondScreenIdd.dll Driver.obj Device.obj Monitor.obj ^
     "%WDK_LIB%\10.0.28000.0\um\x64\iddcx\1.4\iddcxstub.lib" ^
     "%WDK_LIB%\wdf\umdf\x64\2.25\WdfDriverStubUm.lib" ^
     ntdll.lib kernel32.lib user32.lib ole32.lib uuid.lib ucrt.lib vcruntime.lib msvcrt.lib
if %errorlevel% neq 0 (
    echo [ERROR] Linking SecondScreenIdd.dll failed.
    exit /b 1
)

echo.
echo ===================================================
echo SUCCESS: SecondScreenIdd.dll built successfully!
echo Binary path: %~dp0SecondScreenIdd.dll
echo ===================================================

