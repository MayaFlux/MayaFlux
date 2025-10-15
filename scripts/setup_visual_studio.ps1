Write-Host "MayaFlux Visual Studio Setup (Conan Edition)"
Write-Host "==========================================="
Write-Host ""

# Admin check (less critical with Conan)
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) {
    Write-Host "Running with administrator privileges."
} else {
    Write-Host "Note: Running without admin privileges (usually fine with Conan)"
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

# Check for Python (required for Conan)
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Python not found. Conan requires Python 3.8+" -ForegroundColor Red
    Write-Host "Download from: https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "Or install via: winget install Python.Python.3.11" -ForegroundColor Yellow
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

# Conan setup
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

# Add Conan remotes
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
    # Install dependencies - Conan will handle everything
    conan install . --build=missing -s build_type=Release -s compiler.runtime=dynamic
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Conan dependency installation failed" -ForegroundColor Red
        Write-Host "This might be due to:" -ForegroundColor Yellow
        Write-Host "1. Network issues (check your connection)"
        Write-Host "2. Missing Visual Studio C++ tools"
        Write-Host "3. Python environment issues"
        exit 1
    }
    
    Write-Host "✓ Dependencies installed successfully" -ForegroundColor Green
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Generating Visual Studio solution using $CMakeGenerator..."
Push-Location $PROJECT_ROOT
try {
    # Generate Visual Studio solution - Conan's toolchain is automatically used
    $cmakeArgs = @(
        "-B", "build",
        "-S", ".",
        "-G", $CMakeGenerator,
        "-A", "x64",
        "-DCMAKE_INSTALL_PREFIX=`"$(Join-Path $PROJECT_ROOT "install")`""
    )

    & cmake @cmakeArgs
    if (-not $?) {
        Write-Host "CMake generation failed. Potential fixes:" -ForegroundColor Red
        Write-Host "1. Run from 'Developer Command Prompt for VS'" -ForegroundColor Yellow
        Write-Host "2. Ensure Windows SDK is installed" -ForegroundColor Yellow
        Write-Host "3. Check CMake version (minimum 3.25)" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "✓ Visual Studio solution generated" -ForegroundColor Green
} finally {
    Pop-Location
}

# Create user_project.hpp if missing
$USER_PROJECT_FILE = Join-Path $PROJECT_ROOT "src\user_project.hpp"
$TEMPLATE_FILE = Join-Path $PROJECT_ROOT "cmake\user_project.hpp.in"
if (-not (Test-Path $USER_PROJECT_FILE)) {
    Write-Host "Creating user_project.hpp from template..." -ForegroundColor Yellow
    Copy-Item -Path $TEMPLATE_FILE -Destination $USER_PROJECT_FILE
}

# Create enhanced build scripts for Visual Studio
Write-Host "Creating build scripts..." -ForegroundColor Yellow

# Visual Studio build script
@"
@echo off
echo Building MayaFlux with Visual Studio and Conan...
cmake --build "%~dp0build" --config Release --target ALL_BUILD
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)
echo Build successful!
echo You can now run: build\Release\MayaFlux.exe
pause
"@ | Out-File -FilePath (Join-Path $PROJECT_ROOT "vs_build.bat") -Encoding ASCII

# Conan rebuild script (if dependencies change)
@"
@echo off
echo Reinstalling dependencies with Conan...
conan install . --build=missing -s build_type=Release
if errorlevel 1 (
    echo Conan install failed!
    pause
    exit /b 1
)
echo Dependencies updated successfully!
echo Now regenerate Visual Studio solution...
cmake -B build -G "$CMakeGenerator" -A x64
echo Regeneration complete. Use vs_build.bat to build.
pause
"@ | Out-File -FilePath (Join-Path $PROJECT_ROOT "conan_rebuild.bat") -Encoding ASCII

Write-Host ""
Write-Host "✅ Visual Studio setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "What was installed:" -ForegroundColor Cyan
Write-Host "  ✓ LLVM 17.0.6 (for Lila live coding)"
Write-Host "  ✓ RtAudio 5.2.0 (audio I/O)" 
Write-Host "  ✓ FFmpeg 5.1.2 (media processing)"
Write-Host "  ✓ Vulkan 1.3.250 (graphics)"
Write-Host "  ✓ GLFW 3.3.8 (window management)"
Write-Host "  ✓ Eigen 3.4.0 (math library)"
Write-Host "  ✓ Google Test 1.14.0 (unit testing)"
Write-Host "  ✓ Magic Enum 0.9.3 (reflection)"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Open solution: build\MayaFlux.sln"
Write-Host "  2. Build: Run vs_build.bat OR F5 in Visual Studio"
Write-Host "  3. Test: Run build\Release\MayaFlux.exe"
Write-Host ""
Write-Host "Visual Studio integration:" -ForegroundColor Cyan
Write-Host "  - Solution file: build\MayaFlux.sln"
Write-Host "  - Build script: vs_build.bat (for command line)"
Write-Host "  - Dependency update: conan_rebuild.bat"
Write-Host ""
Write-Host "Project structure in Solution Explorer:" -ForegroundColor Cyan
Write-Host "  - MayaFlux (main application)"
Write-Host "  - MayaFluxLib (core library)"
Write-Host "  - Lila (live coding library)"
Write-Host "  - Tests (unit tests)"
Write-Host ""
Write-Host "Benefits over vcpkg:" -ForegroundColor Yellow
Write-Host "  ✓ Much faster setup (precompiled binaries)"
Write-Host "  ✓ No 4-hour LLVM compilation"
Write-Host "  ✓ More reliable dependency resolution"
Write-Host "  ✓ Better version management"
Write-Host ""
