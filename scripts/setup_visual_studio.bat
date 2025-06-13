@echo off
setlocal enabledelayedexpansion

echo MayaFlux Visual Studio Setup
echo ===========================
echo.

:: Improved admin check using whoami
whoami /groups | findstr /b /c:"Mandatory Label\High Mandatory Level" > nul
if %ERRORLEVEL% EQU 0 (
    echo Running with administrator privileges.
) else (
    echo Warning: Not running with administrator privileges.
    echo vcpkg installations might require elevated rights.
    timeout /t 2
)

:: Robust path handling with spaces
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECT_ROOT=%SCRIPT_DIR%\.."
pushd "%PROJECT_ROOT%"
set "PROJECT_ROOT=%cd%"
popd

:: Check for Git with better error guidance
where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: Git is required but not found in PATH.
    echo Install from: https://git-scm.com/download/win
    echo Or using winget: winget install Git.Git
    exit /b 1
)

:: Check for CMake installation
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake is required but not found in PATH.
    echo Install from: https://cmake.org/download/
    echo Or using winget: winget install Kitware.CMake
    exit /b 1
)

:: Enhanced Visual Studio detection
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: Visual Studio Installer not found.
    echo Download from: https://aka.ms/vs/17/release/vs_community.exe
    exit /b 1
)

:: Find latest compatible VS installation
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo Error: Visual Studio with C++ tools not found.
    echo Required components:
    echo - Desktop development with C++
    echo - Windows 10/11 SDK
    echo - C++ CMake tools for Windows
    exit /b 1
)

:: Set up VS environment
echo Found Visual Studio at: %VS_PATH%

:: Get Visual Studio version more reliably
for /f "usebackq tokens=1* delims=: " %%i in (`"%VSWHERE%" -latest -property catalog.productDisplayVersion`) do (
    set "VS_FULL_VERSION=%%j"
)
set "VS_VERSION=%VS_FULL_VERSION:~0,2%"
echo Detected Visual Studio version: %VS_FULL_VERSION% (using %VS_VERSION%)

:: vcpkg setup with priority: VCPKG_ROOT > project-local > install new
if defined VCPKG_ROOT (
    echo Using existing vcpkg from VCPKG_ROOT: %VCPKG_ROOT%
    set "VCPKG_DIR=%VCPKG_ROOT%"
    if not exist "%VCPKG_DIR%\vcpkg.exe" (
        echo Error: Invalid VCPKG_ROOT - vcpkg.exe missing
        exit /b 1
    )
) else (
    set "VCPKG_DIR=%PROJECT_ROOT%\vcpkg"
    if exist "%VCPKG_DIR%\vcpkg.exe" (
        echo Using project-local vcpkg
    ) else (
        echo Installing vcpkg to project directory...
        git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
        if errorlevel 1 (
            echo FATAL: vcpkg clone failed
            exit /b 1
        )
        pushd "%VCPKG_DIR%"
        call bootstrap-vcpkg.bat -disableMetrics
        if errorlevel 1 (
            echo FATAL: Bootstrap failed
            exit /b 1
        )
        popd
    )
)

:: Update only project-local installations
if not defined VCPKG_ROOT (
    echo Synchronizing project-local vcpkg...
    pushd "%VCPKG_DIR%"
    git pull || (
        echo Update failed. Potential solutions:
        echo 1. Run: git reset --hard HEAD
        echo 2. Delete directory and rerun script
        exit /b 1
    )
    popd
)

echo.
echo Installing dependencies (x64-windows)...
"%VCPKG_DIR%\vcpkg" install --triplet x64-windows rtaudio ffmpeg gtest
if errorlevel 1 (
    echo Dependency installation failed
    exit /b 1
)

:: Verify package installation
echo Verifying package installation...
"%VCPKG_DIR%\vcpkg" list | findstr /i "rtaudio" > nul
if errorlevel 1 echo Warning: rtaudio may not be installed correctly.
"%VCPKG_DIR%\vcpkg" list | findstr /i "ffmpeg" > nul
if errorlevel 1 echo Warning: ffmpeg may not be installed correctly.
"%VCPKG_DIR%\vcpkg" list | findstr /i "gtest" > nul
if errorlevel 1 echo Warning: gtest may not be installed correctly.

:: Create build directory with solution folder classification
if not exist "%PROJECT_ROOT%\build" (
    mkdir "%PROJECT_ROOT%\build"
    attrib +h "%PROJECT_ROOT%\build"
)

echo.
echo Generating Visual Studio %VS_VERSION% solution...
pushd "%PROJECT_ROOT%"
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" ^
    -G "Visual Studio %VS_VERSION% %VS_VERSION%" ^
    -A x64 ^
    -DCMAKE_INSTALL_PREFIX="%PROJECT_ROOT%\install"

if errorlevel 1 (
    echo CMake generation failed. Potential fixes:
    echo 1. Run from "Developer Command Prompt for VS"
    echo 2. Ensure Windows SDK is installed
    echo 3. Check CMake version (minimum 3.25)
    exit /b 1
)
popd

:: Create enhanced build script
(
echo @echo off
echo setlocal
echo set "VCPKG_ROOT=%VCPKG_DIR:\=\\%"
echo cmake --build "%~dp0build" --config Release --target ALL_BUILD
echo if errorlevel 1 exit /b 1
echo cmake --install "%~dp0build" --config Release
echo endlocal
) > "%PROJECT_ROOT%\vs_build.bat"

echo.
echo Setup complete! Recommended next steps:
echo.
echo 1. Visual Studio Extensions to install:
echo    - CMake Tools for Visual Studio
echo    - ReSharper C++ (optional)
echo    - Visual Assist (optional debug help)
echo.
echo 2. Open solution:
echo    a) Launch Visual Studio
echo    b) Open "%PROJECT_ROOT%\build\MayaFlux.sln"
echo    OR
echo    Double-click the .sln file in Explorer
echo.
echo 3. Build options:
echo    - Debug: F5 (with debugger)
echo    - Release: Use vs_build.bat
echo    - CMake presets available in CMakeLists.txt
echo.
echo 4. Project configuration:
echo    - Set startup project in Solution Explorer
echo    - Adjust build settings in CMakeSettings.json
echo.
echo Note: Build directory is hidden by default (attrib +h)
echo       Use "dir /ah" to view hidden directories
echo.

endlocal
