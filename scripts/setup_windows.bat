@echo off
setlocal enabledelayedexpansion

echo MayaFlux Windows Setup
echo =====================
echo.

:: Improved admin check (works on non-English systems)
whoami /groups | findstr /b /c:"Mandatory Label\High Mandatory Level" > nul
if %ERRORLEVEL% EQU 0 (
    echo Running with administrator privileges.
) else (
    echo Warning: Not running with administrator privileges.
    echo Some operations (like vcpkg install) might fail.
    timeout /t 2
)

:: Get absolute project root path (handles spaces correctly)
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECT_ROOT=%SCRIPT_DIR%\.."
pushd "%PROJECT_ROOT%"
set "PROJECT_ROOT=%cd%"
popd

:: Check for Git
where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: Git is required but not found in PATH.
    echo Download from: https://git-scm.com/download/win
    echo Or install via winget: winget install Git.Git
    exit /b 1
)

:: Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake is required but not found in PATH.
    echo Download from: https://cmake.org/download/
    echo Or install via winget: winget install Kitware.CMake
    exit /b 1
)

:: Check for Visual Studio environment
if defined VisualStudioVersion (
    echo Visual Studio %VisualStudioVersion% detected.
) else (
    echo Warning: Visual Studio environment not detected.
    echo Ensure you have:
    echo - Visual Studio 2022 with C++ workload installed
    echo - Run this script from "Developer Command Prompt for VS"
    echo or
    echo - Add VS to PATH using "vcvarsall.bat"
    timeout /t 5
)

:: vcpkg setup with priority: VCPKG_ROOT > project-local > install new
if defined VCPKG_ROOT (
    echo Using existing vcpkg from VCPKG_ROOT: %VCPKG_ROOT%
    set "VCPKG_DIR=%VCPKG_ROOT%"
    if not exist "%VCPKG_DIR%\vcpkg.exe" (
        echo Error: vcpkg.exe not found in VCPKG_ROOT directory
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
            echo Failed to clone vcpkg
            exit /b 1
        )
        pushd "%VCPKG_DIR%"
        call bootstrap-vcpkg.bat -disableMetrics
        if errorlevel 1 (
            echo Failed to bootstrap vcpkg
            exit /b 1
        )
        popd
    )
)

:: Only update project-local vcpkg (never touch VCPKG_ROOT)
if not defined VCPKG_ROOT (
    echo Updating project-local vcpkg...
    pushd "%VCPKG_DIR%"
    git pull || (
        echo Failed to update. Conflicts detected - try:
        echo 1. Delete %VCPKG_DIR%
        echo 2. Rerun this script
        exit /b 1
    )
    popd
)

echo.
echo Installing required packages (x64-windows)...
"%VCPKG_DIR%\vcpkg" install --triplet x64-windows rtaudio ffmpeg gtest
if errorlevel 1 (
    echo Package installation failed
    exit /b 1
)

:: Add this after the vcpkg install command
echo Verifying package installation...
"%VCPKG_DIR%\vcpkg" list | findstr "rtaudio ffmpeg gtest" > nul
if errorlevel 1 (
    echo Warning: Some packages may not have installed correctly.
    echo Please check the output above for errors.
    timeout /t 2
) else (
    echo All required packages verified.
)

:: Set environment variables for current session
set "VCPKG_ROOT=%VCPKG_DIR%"
set "PATH=%VCPKG_DIR%;%PATH%"

echo.
echo Creating build directory...
if not exist "%PROJECT_ROOT%\build" mkdir "%PROJECT_ROOT%\build"

:: Generate better build script using absolute paths
echo Generating build script...
(
echo @echo off
echo setlocal
echo set "VCPKG_ROOT=%VCPKG_DIR:\=\\%"
echo cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="%%VCPKG_ROOT%%\scripts\buildsystems\vcpkg.cmake"
echo cmake --build build --config Release
echo endlocal
) > "%PROJECT_ROOT%\build.bat"

echo.
echo Setup complete!
echo.
echo Recommended next steps:
echo 1. Open Developer Command Prompt for VS
echo 2. Navigate to project directory
echo 3. Run either:
echo    - build.bat
echo    - Or manually:
echo      cd build
echo      cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"
echo      cmake --build . --config Release
echo.
