# MayaFlux Windows Setup
Write-Host "MayaFlux Windows Setup"
Write-Host "====================="
Write-Host ""

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
    $vcpkgDir = Join-Path -Path $PSScriptRoot -ChildPath "vcpkg"
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
& "$vcpkgDir\vcpkg.exe" install --triplet x64-windows rtaudio ffmpeg gtest eigen3 magic-enum
if ($LASTEXITCODE -ne 0) {
    Write-Host "Package installation failed"
    exit 1
}

# Verify package installation
Write-Host "Verifying package installation..."
& "$vcpkgDir\vcpkg.exe" list | Where-Object { $_ -match "rtaudio|ffmpeg|gtest|eigen3|magic_enum" }
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

# Create build directory
if (-not (Test-Path -Path "$PSScriptRoot\build")) {
    New-Item -Path "$PSScriptRoot\build" -ItemType Directory
}

# Generate build script
$buildScript = @"
@echo off
setlocal
set "VCPKG_ROOT=$vcpkgDir"
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
endlocal
"@

Set-Content -Path "$PSScriptRoot\build.bat" -Value $buildScript

Write-Host "Setup complete!"
Write-Host "Recommended next steps:"
Write-Host "1. Open Developer Command Prompt for VS"
Write-Host "2. Navigate to project directory"
Write-Host "3. Run either:"
Write-Host "   - build.bat"
Write-Host "   - Or manually:"
Write-Host "     cd build"
Write-Host "     cmake .. -DCMAKE_TOOLCHAIN_FILE=""$vcpkgDir\scripts\buildsystems\vcpkg.cmake"""
Write-Host "     cmake --build . --config Release"
