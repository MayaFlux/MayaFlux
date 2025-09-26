# MayaFlux Windows Setup
Write-Host "MayaFlux Windows Setup"
Write-Host "====================="
Write-Host ""

# Get the project root directory
$SCRIPT_DIR = $PSScriptRoot
$PROJECT_ROOT = (Get-Item (Join-Path $SCRIPT_DIR "..")).FullName

# Check for admin privileges
if (-not (New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Warning: Not running with administrator privileges."
    Write-Host "Some operations might fail (like vcpkg install)"
    Start-Sleep -Seconds 2
}

# Check for Visual Studio environment
if (-not $env:VisualStudioVersion) {
    Write-Host "Warning: Visual Studio environment not detected."
    Write-Host "Ensure you have:"
    Write-Host "- Visual Studio 2022 with C++ workload installed"
    Write-Host "- Run this script from 'Developer Command Prompt for VS'"
    Write-Host "- Or add VS to PATH using 'vcvarsall.bat'"
    Start-Sleep -Seconds 5
}

# Check for vcpkg
if ($env:VCPKG_ROOT) {
    Write-Host "Using existing vcpkg from VCPKG_ROOT: $env:VCPKG_ROOT"
    $vcpkgDir = $env:VCPKG_ROOT
    if (-not (Test-Path -Path "$vcpkgDir\vcpkg.exe")) {
        Write-Host "Error: vcpkg.exe not found in VCPKG_ROOT directory"
        exit 1
    }
} else {
    $vcpkgDir = Join-Path -Path $PROJECT_ROOT -ChildPath "vcpkg"
    if (Test-Path -Path "$vcpkgDir\vcpkg.exe") {
        Write-Host "Using project-local vcpkg"
    } else {
        Write-Host "Installing vcpkg to project directory..."
        git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Failed to clone vcpkg"
            exit 1
        }
        Push-Location $vcpkgDir
        .\bootstrap-vcpkg.bat -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Failed to bootstrap vcpkg"
            exit 1
        }
        Pop-Location
    }
}

# Install required packages
Write-Host "Installing required packages (x64-windows)..."
& "$vcpkgDir\vcpkg.exe" install --triplet x64-windows rtaudio ffmpeg gtest eigen3 magic-enum glfw3
if ($LASTEXITCODE -ne 0) {
    Write-Host "Package installation failed"
    exit 1
}

# Verify package installation
Write-Host "Verifying package installation..."
& "$vcpkgDir\vcpkg.exe" list | Where-Object { $_ -match "rtaudio|ffmpeg|gtest|eigen3|magic_enum|glfw3" }
if ($LASTEXITCODE -ne 0) {
    Write-Host "Warning: Some packages may not have installed correctly."
    Write-Host "Please check the output above for errors."
    Start-Sleep -Seconds 2
} else {
    Write-Host "All required packages verified."
}

# Set environment variables
$env:VCPKG_ROOT = $vcpkgDir
$env:PATH = "$vcpkgDir;$env:PATH"

if (-not (Test-Path -Path "$PROJECT_ROOT\build")) {
    New-Item -Path "$PROJECT_ROOT\build" -ItemType Directory
}

$userProjectPath = Join-Path $PROJECT_ROOT "src/user_project.hpp"
$templatePath = Join-Path $PROJECT_ROOT "cmake/user_project.hpp.in"
if (-not (Test-Path $userProjectPath)) {
    Write-Host "Copying user project template..."
    Copy-Item -Path $templatePath -Destination $userProjectPath
    Write-Host "Created src/user_project.hpp from template"
} else {
    Write-Host "src/user_project.hpp already exists, skipping creation"
}

# Create build script
$buildPath = Join-Path $PROJECT_ROOT "build.bat"
"@echo off" | Out-File -FilePath $buildPath -Encoding ASCII
"setlocal" | Out-File -FilePath $buildPath -Append -Encoding ASCII
"set VCPKG_ROOT=$vcpkgDir" | Out-File -FilePath $buildPath -Append -Encoding ASCII
"cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" | Out-File -FilePath $buildPath -Append -Encoding ASCII
"cmake --build build --config Release" | Out-File -FilePath $buildPath -Append -Encoding ASCII
"endlocal" | Out-File -FilePath $buildPath -Append -Encoding ASCII

Write-Host "Setup complete!"
