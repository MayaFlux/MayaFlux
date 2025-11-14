# scripts/win64/setup_visual_studio.ps1

Write-Host "MayaFlux Visual Studio Solution Generator" -ForegroundColor Cyan
Write-Host ""

$SCRIPT_DIR = $PSScriptRoot
$PROJECT_ROOT = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent

# Check prerequisites
$missing = @()

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { $missing += "Git" }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $missing += "CMake" }

$VSWHERE = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $VSWHERE)) { $missing += "Visual Studio" }

if ($missing.Count -gt 0) {
    Write-Host "ERROR: Missing required tools: $($missing -join ', ')" -ForegroundColor Red
    Write-Host ""
    Write-Host "Install missing tools:" -ForegroundColor Yellow
    if ($missing -contains "Git") { Write-Host "  winget install Git.Git" }
    if ($missing -contains "CMake") { Write-Host "  winget install Kitware.CMake" }
    if ($missing -contains "Visual Studio") { Write-Host "  Download: https://visualstudio.microsoft.com/downloads/" }
    exit 1
}

# Check for C++ workload
$VS_PATH = & $VSWHERE -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VS_PATH) {
    Write-Host "ERROR: Visual Studio with C++ tools not found" -ForegroundColor Red
    Write-Host "Install 'Desktop development with C++' workload" -ForegroundColor Yellow
    exit 1
}

# Detect VS version
$VS_VERSION = (& $VSWHERE -latest -property catalog.productLineVersion)
$CMakeGenerator = switch ($VS_VERSION) {
    "2022" { "Visual Studio 17 2022" }
    "2019" { "Visual Studio 16 2019" }
    default { "Visual Studio 17 2022" }
}

Write-Host "Visual Studio: $VS_PATH" -ForegroundColor Green
Write-Host "Generator: $CMakeGenerator" -ForegroundColor Green
Write-Host ""

# Check if dependencies installed by verifying key environment variables
$depsOk = $true
$requiredEnvVars = @("LLVM_DIR", "VULKAN_SDK", "FFMPEG_ROOT", "GLFW_ROOT")

foreach ($envVar in $requiredEnvVars) {
    if (-not (Test-Path "env:$envVar")) {
        Write-Host "Missing environment variable: $envVar" -ForegroundColor Red
        $depsOk = $false
    }
}

if (-not $depsOk) {
    Write-Host ""
    Write-Host "ERROR: Dependencies not installed" -ForegroundColor Red
    Write-Host "Run setup_windows.ps1 first (requires admin), then restart terminal" -ForegroundColor Yellow
    exit 1
}

Write-Host "Dependencies found" -ForegroundColor Green
Write-Host ""

# Generate solution
$BUILD_DIR = Join-Path $PROJECT_ROOT "build"
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

Write-Host "Generating Visual Studio solution..." -ForegroundColor Yellow

try {
    Push-Location $PROJECT_ROOT

    $cmakeArgs = @(
        "-B", "build",
        "-S", ".",
        "-G", $CMakeGenerator,
        "-A", "x64",
        "-DCMAKE_BUILD_TYPE=Release"
    )

    # Execute CMake
    & cmake @cmakeArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ERROR: CMake generation failed" -ForegroundColor Red
        exit 1
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Solution Generated ===" -ForegroundColor Green
Write-Host ""
Write-Host "Solution: build\MayaFlux.sln" -ForegroundColor Cyan
Write-Host ""
Write-Host "Build from Visual Studio:" -ForegroundColor Yellow
Write-Host "  1. Open build\MayaFlux.sln"
Write-Host "  2. Set startup project"
Write-Host "  3. Build -> Build Solution (or press F7)"
Write-Host ""
Write-Host "Or build from command line:" -ForegroundColor Yellow
Write-Host "  cmake --build build --config Release"
Write-Host ""
