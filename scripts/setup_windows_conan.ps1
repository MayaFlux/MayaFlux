# MayaFlux Windows Setup (Conan Edition)
Write-Host "MayaFlux Windows Setup (Conan)" -ForegroundColor Green
Write-Host "=============================="
Write-Host ""

# Get the project root directory
$SCRIPT_DIR = $PSScriptRoot
$PROJECT_ROOT = (Get-Item (Join-Path $SCRIPT_DIR "..")).FullName

# Admin check (less critical with Conan)
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    Write-Host "Running with administrator privileges."
} else {
    Write-Host "Note: Running without admin privileges (usually fine with Conan)"
}

# Check for Visual Studio (still needed for compiler)
if (-not $env:VisualStudioVersion) {
    Write-Host "Warning: Visual Studio environment not detected." -ForegroundColor Yellow
    Write-Host "This script works best from 'Developer Command Prompt for VS'"
    Write-Host "Alternatively, ensure Visual Studio 2022 with C++ tools is installed"
    Write-Host ""
    Write-Host "Trying to detect Visual Studio anyway..."
    
    $VSWHERE = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $VSWHERE) {
        $VS_PATH = & $VSWHERE -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($VS_PATH) {
            Write-Host "Found Visual Studio at: $VS_PATH" -ForegroundColor Green
            
            # Try to set up environment
            $VCVARS = Join-Path $VS_PATH "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $VCVARS) {
                Write-Host "Setting up Visual Studio environment..."
                cmd /c "`"$VCVARS`" && set" | ForEach-Object {
                    if ($_ -match "^(.*?)=(.*)$") {
                        Set-Item -Path "env:\$($matches[1])" -Value $matches[2]
                    }
                }
            }
        }
    }
}

# Check for Python (required for Conan)
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Python not found. Conan requires Python 3.8+" -ForegroundColor Red
    Write-Host "Download from: https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "Or install via: winget install Python.Python.3.11" -ForegroundColor Yellow
    exit 1
}

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "Error: CMake is required but not found in PATH." -ForegroundColor Red
    Write-Host "Install from: https://cmake.org/download/" -ForegroundColor Yellow
    Write-Host "Or using winget: winget install Kitware.CMake" -ForegroundColor Yellow
    exit 1
}

# Install/Update Conan
Write-Host "`nSetting up Conan..." -ForegroundColor Yellow
$CONAN_VERSION = & { python -c "import conan; print(conan.__version__)" } 2>$null
if ($LASTEXITCODE -ne 0 -or $CONAN_VERSION -lt "2.0.0") {
    Write-Host "Installing/upgrading Conan..."
    pip install --upgrade conan
} else {
    Write-Host "Conan $CONAN_VERSION already installed" -ForegroundColor Green
}

# Configure Conan (if first time)
if (-not (Test-Path "$env:USERPROFILE\.conan2\profiles\default")) {
    Write-Host "Initializing Conan configuration..." -ForegroundColor Yellow
    conan profile detect --force
}

# Add Conan remotes (ensure we have access to all packages)
Write-Host "Configuring Conan remotes..." -ForegroundColor Yellow
conan remote add conancenter https://center.conan.io true 2>$null
Write-Host "✓ ConanCenter configured" -ForegroundColor Green

# Create build directory
$BUILD_DIR = Join-Path $PROJECT_ROOT "build"
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

# Install dependencies with Conan
Write-Host "`nInstalling dependencies with Conan..." -ForegroundColor Yellow
Write-Host "This will download precompiled binaries (much faster than vcpkg!)" -ForegroundColor Cyan

Push-Location $PROJECT_ROOT
try {
    # First, install dependencies
    conan install . --build=missing -s build_type=Release -s compiler.runtime=dynamic
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Conan dependency installation failed" -ForegroundColor Red
        Write-Host "This might be due to:" -ForegroundColor Yellow
        Write-Host "1. Network issues (check your connection)"
        Write-Host "2. Missing Visual Studio C++ tools"
        Write-Host "3. Python environment issues"
        exit 1
    }
    
    # Then build the project
    Write-Host "`nBuilding MayaFlux..." -ForegroundColor Yellow
    conan build .
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Build failed" -ForegroundColor Red
        exit 1
    }
}
finally {
    Pop-Location
}

# Create user_project.hpp if missing
$USER_PROJECT_FILE = Join-Path $PROJECT_ROOT "src\user_project.hpp"
$TEMPLATE_FILE = Join-Path $PROJECT_ROOT "cmake\user_project.hpp.in"
if (-not (Test-Path $USER_PROJECT_FILE)) {
    Write-Host "Creating user_project.hpp from template..." -ForegroundColor Yellow
    Copy-Item -Path $TEMPLATE_FILE -Destination $USER_PROJECT_FILE
}

# Create convenient build scripts
Write-Host "`nCreating build scripts..." -ForegroundColor Yellow

# Simple build script
@"
@echo off
echo Building MayaFlux with Conan...
conan install . --build=missing -s build_type=Release
conan build .
pause
"@ | Out-File -FilePath (Join-Path $PROJECT_ROOT "build_conan.bat") -Encoding ASCII

# Development build script (incremental)
@"
@echo off
echo Building MayaFlux (Development)...
conan build .
if %errorlevel% equ 0 (
    echo Build successful!
    echo You can now run: build\Release\MayaFlux.exe
) else (
    echo Build failed!
)
pause
"@ | Out-File -FilePath (Join-Path $PROJECT_ROOT "dev_build.bat") -Encoding ASCII

Write-Host "`n✅ MayaFlux setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "What was installed:" -ForegroundColor Cyan
Write-Host "  ✓ LLVM (for Lila live coding)"
Write-Host "  ✓ RtAudio (audio I/O)"
Write-Host "  ✓ FFmpeg (media processing)"
Write-Host "  ✓ Vulkan (graphics)"
Write-Host "  ✓ GLFW (window management)"
Write-Host "  ✓ Eigen (math library)"
Write-Host "  ✓ Google Test (unit testing)"
Write-Host "  ✓ Magic Enum (reflection)"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Run 'build\Release\MayaFlux.exe' to test"
Write-Host "  2. Use 'dev_build.bat' for incremental builds"
Write-Host "  3. Use 'build_conan.bat' for clean builds"
Write-Host ""
Write-Host "Project structure:" -ForegroundColor Cyan
Write-Host "  build\Release\          - Your built executables"
Write-Host "  build\Release\*.dll     - Required DLLs (already bundled)"
Write-Host "  ~\.conan2\data\         - Conan cache (dependencies)"
Write-Host ""
Write-Host "To update dependencies later:" -ForegroundColor Yellow
Write-Host "  conan install . --build=missing -s build_type=Release"
Write-Host ""
