Write-Host "MayaFlux Visual Studio Setup"
Write-Host "==========================="
Write-Host ""

# Admin check
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    Write-Host "Running with administrator privileges."
} else {
    Write-Host "Warning: Not running with administrator privileges."
    Write-Host "vcpkg installations might require elevated rights."
    Start-Sleep -Seconds 2
}

# Robust path handling
$SCRIPT_DIR = $PSScriptRoot
$PROJECT_ROOT = (Get-Item (Join-Path $SCRIPT_DIR "..")).FullName

# Check for Git
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Git is required but not found in PATH."
    Write-Host "Install from: https://git-scm.com/download/win"
    Write-Host "Or using winget: winget install Git.Git"
    exit 1
}

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "Error: CMake is required but not found in PATH."
    Write-Host "Install from: https://cmake.org/download/"
    Write-Host "Or using winget: winget install Kitware.CMake"
    exit 1
}

# Visual Studio detection
$VSWHERE = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $VSWHERE)) {
    Write-Host "Error: Visual Studio Installer not found."
    Write-Host "Download from: https://aka.ms/vs/17/release/vs_community.exe"
    exit 1
}

# Find latest compatible VS installation
$VS_PATH = & $VSWHERE -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VS_PATH) {
    Write-Host "Error: Visual Studio with C++ tools not found."
    Write-Host "Required components:"
    Write-Host "- Desktop development with C++"
    Write-Host "- Windows 10/11 SDK"
    Write-Host "- C++ CMake tools for Windows"
    exit 1
}

Write-Host "Found Visual Studio at: $VS_PATH"

# Get Visual Studio version
$VS_FULL_VERSION = (& $VSWHERE -latest -property catalog.productDisplayVersion)
$VS_VERSION = $VS_FULL_VERSION.Substring(0,2)
Write-Host "Detected Visual Studio version: $VS_FULL_VERSION (using $VS_VERSION)"

# Determine correct CMake generator name
$CMakeGenerator = switch ($VS_VERSION) {
    "17" { "Visual Studio 17 2022" }
    "16" { "Visual Studio 16 2019" }
    "15" { "Visual Studio 15 2017" }
    default { "Visual Studio 17 2022" } # Default to VS 2022 if unknown
}

# vcpkg setup
if ($env:VCPKG_ROOT) {
    Write-Host "Using existing vcpkg from VCPKG_ROOT: $($env:VCPKG_ROOT)"
    $VCPKG_DIR = $env:VCPKG_ROOT
    if (-not (Test-Path (Join-Path $VCPKG_DIR "vcpkg.exe"))) {
        Write-Host "Error: Invalid VCPKG_ROOT - vcpkg.exe missing"
        exit 1
    }
} else {
    $VCPKG_DIR = Join-Path $PROJECT_ROOT "vcpkg"
    if (Test-Path (Join-Path $VCPKG_DIR "vcpkg.exe")) {
        Write-Host "Using project-local vcpkg"
    } else {
        Write-Host "Installing vcpkg to project directory..."
        try {
            git clone https://github.com/microsoft/vcpkg.git $VCPKG_DIR
            if (-not $?) { throw "vcpkg clone failed" }

            Push-Location $VCPKG_DIR
            try {
                & .\bootstrap-vcpkg.bat -disableMetrics
                if (-not $?) { throw "Bootstrap failed" }
            } finally {
                Pop-Location
            }
        } catch {
            Write-Host "FATAL: $_"
            exit 1
        }
    }
}

# Update only project-local installations
if (-not $env:VCPKG_ROOT) {
    Write-Host "Synchronizing project-local vcpkg..."
    Push-Location $VCPKG_DIR
    try {
        git pull
        if (-not $?) {
            Write-Host "Update failed. Potential solutions:"
            Write-Host "1. Run: git reset --hard HEAD"
            Write-Host "2. Delete directory and rerun script"
            exit 1
        }
    } finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "Installing dependencies (x64-windows)..."
& (Join-Path $VCPKG_DIR "vcpkg") install --triplet x64-windows rtaudio ffmpeg gtest eigen3 magic-enum glfw3
if (-not $?) {
    Write-Host "Dependency installation failed"
    exit 1
}

# Verify package installation
Write-Host "Verifying package installation..."
$installed = & (Join-Path $VCPKG_DIR "vcpkg") list
if (-not ($installed -match "rtaudio")) { Write-Host "Warning: rtaudio may not be installed correctly." }
if (-not ($installed -match "ffmpeg")) { Write-Host "Warning: ffmpeg may not be installed correctly." }
if (-not ($installed -match "gtest")) { Write-Host "Warning: gtest may not be installed correctly." }
if (-not ($installed -match "eigen3")) { Write-Host "Warning: eigen3 may not be installed correctly." }
if (-not ($installed -match "magic_enum")) { Write-Host "Warning: magic_enum may not be installed correctly." }
if (-not ($installed -match "glfw3")) { Write-Host "Warning: glfw3 may not be installed correctly." }

# Create build directory
$buildDir = Join-Path $PROJECT_ROOT "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    (Get-Item $buildDir).Attributes += 'Hidden'
}

Write-Host ""
Write-Host "Generating Visual Studio solution using $CMakeGenerator..."
Push-Location $PROJECT_ROOT
try {
    $cmakeArgs = @(
        "-B", "build",
        "-S", ".",
        "-DCMAKE_TOOLCHAIN_FILE=`"$(Join-Path $VCPKG_DIR "scripts\buildsystems\vcpkg.cmake")`"",
        "-G", $CMakeGenerator,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=`"$(Join-Path $PROJECT_ROOT "install")`""
    )

    & cmake @cmakeArgs
    if (-not $?) {
        Write-Host "CMake generation failed. Potential fixes:"
        Write-Host "1. Run from 'Developer Command Prompt for VS'"
        Write-Host "2. Ensure Windows SDK is installed"
        Write-Host "3. Check CMake version (minimum 3.25)"
        exit 1
    }
} finally {
    Pop-Location
}

# Create enhanced build script
@"
@echo off
setlocal
set "VCPKG_ROOT=$($VCPKG_DIR -replace '\\', '\\')"
cmake --build "%~dp0build" --config Release --target ALL_BUILD
if errorlevel 1 exit /b 1
cmake --install "%~dp0build" --config Release
endlocal
"@ | Out-File -FilePath (Join-Path $PROJECT_ROOT "vs_build.bat") -Encoding ASCII

Write-Host ""
Write-Host "Setup complete! Recommended next steps:"
Write-Host ""
Write-Host "1. Visual Studio Extensions to install:"
Write-Host "   - CMake Tools for Visual Studio"
Write-Host "   - ReSharper C++ (optional)"
Write-Host "   - Visual Assist (optional debug help)"
Write-Host ""
Write-Host "2. Open solution:"
Write-Host "   a) Launch Visual Studio"
Write-Host "   b) Open `"$(Join-Path $PROJECT_ROOT "build\MayaFlux.sln")`""
Write-Host "   OR"
Write-Host "   Double-click the .sln file in Explorer"
Write-Host ""
Write-Host "3. Build options:"
Write-Host "   - Debug: F5 (with debugger)"
Write-Host "   - Release: Use vs_build.bat"
Write-Host "   - CMake presets available in CMakeLists.txt"
Write-Host ""
Write-Host "4. Project configuration:"
Write-Host "   - Set startup project in Solution Explorer"
Write-Host "   - Adjust build settings in CMakeSettings.json"
Write-Host ""
Write-Host "Note: Build directory is hidden by default (attrib +h)"
Write-Host "      Use 'dir /ah' to view hidden directories"
Write-Host ""
