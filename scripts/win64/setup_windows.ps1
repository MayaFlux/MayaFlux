#Requires -RunAsAdministrator

param(
    [Parameter(Mandatory = $false)]
    [string[]]$Only
)

$ErrorActionPreference = "Stop"

$SCRIPT_DIR = $PSScriptRoot
$PROJECT_ROOT = Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent

Write-Host @"
╔═══════════════════════════════════════════════════════════╗
║            MayaFlux Windows Setup                         ║
║  Next-generation multimedia DSP framework                 ║
╚═══════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

Write-Host ""

# Helper function to append to environment variables without duplicates
function Add-EnvPath {
    param(
        [string]$Name,
        [string]$Value,
        [string]$Scope = 'Machine'
    )
    
    if (-not (Test-Path $Value)) {
        Write-Warning "Path does not exist: $Value"
        return
    }
    
    $currentValue = [Environment]::GetEnvironmentVariable($Name, $Scope)
    if ($currentValue) {
        $paths = $currentValue -split ';'
        if ($paths -notcontains $Value) {
            $newValue = "$Value;$currentValue"
            [Environment]::SetEnvironmentVariable($Name, $newValue, $Scope)
            Write-Host "[ENV] Added $Value to $Name" -ForegroundColor Green
        } else {
            Write-Host "[ENV] $Value already in $Name" -ForegroundColor Yellow
        }
    } else {
        [Environment]::SetEnvironmentVariable($Name, $Value, $Scope)
        Write-Host "[ENV] Set $Name = $Value" -ForegroundColor Green
    }
}

# Helper function to add include directories
function Add-IncludeDirectory {
    param(
        [string]$Path,
        [string]$Scope = 'Machine'
    )
    
    Add-EnvPath -Name "CPATH" -Value $Path -Scope $Scope
}

# Helper function to add library directories
function Add-LibraryDirectory {
    param(
        [string]$Path,
        [string]$Scope = 'Machine'
    )
    
    Add-EnvPath -Name "LIBRARY_PATH" -Value $Path -Scope $Scope
}

# Load files
$packagesFile = Join-Path $SCRIPT_DIR "packages.psd1"
$installerScript = Join-Path $SCRIPT_DIR "install_package.ps1"

if (-not (Test-Path $packagesFile) -or -not (Test-Path $installerScript)) {
    Write-Error "Required files not found"
    exit 1
}

# Run generic installer
$installerArgs = @{ PackagesFile = $packagesFile }
if ($Only) { $installerArgs.Only = $Only }

try {
    & $installerScript @installerArgs
}
catch {
    Write-Error "Package installation failed: $($_.Exception.Message)"
    exit 1
}

# ========================================
# SPECIAL HANDLING for complex packages
# ========================================

Write-Host "`n=== Configuring Environment Variables ===" -ForegroundColor Magenta

# DIA SDK (required for LLVM on Windows)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstalls = & $vswhere -all -format json | ConvertFrom-Json
    $diaFound = $false

    foreach ($vs in $vsInstalls) {
        $diaPath = Join-Path $vs.installationPath "DIA SDK"
        if (Test-Path "$diaPath\lib\amd64\diaguids.lib") {
            [Environment]::SetEnvironmentVariable("DIA_SDK_PATH", $diaPath, "Machine")
            Write-Host "[DIA SDK] Found: $diaPath" -ForegroundColor Green
            $diaFound = $true
            break
        }
    }

    if (-not $diaFound) {
        Write-Warning "DIA SDK not found - LLVM may have linking issues"
    }
}

# LLVM/Clang
$llvmRoot = "C:\Program Files\LLVM_Libs"
if (Test-Path $llvmRoot) {
    [Environment]::SetEnvironmentVariable("LLVM_ROOT", $llvmRoot, "Machine")
    [Environment]::SetEnvironmentVariable("LLVM_DIR", "$llvmRoot\lib\cmake\llvm", "Machine")
    [Environment]::SetEnvironmentVariable("Clang_DIR", "$llvmRoot\lib\cmake\clang", "Machine")
    Write-Host "[LLVM] Environment configured" -ForegroundColor Green
}

# Vulkan - find actual version directory
$vulkanBase = "C:\VulkanSDK"
if (Test-Path $vulkanBase) {
    $vulkanVersion = Get-ChildItem $vulkanBase -Directory | Select-Object -First 1
    if ($vulkanVersion) {
        $vulkanPath = $vulkanVersion.FullName
        [Environment]::SetEnvironmentVariable("VULKAN_SDK", $vulkanPath, "Machine")
        [Environment]::SetEnvironmentVariable("VK_SDK_PATH", $vulkanPath, "Machine")
        Write-Host "[Vulkan] SDK: $vulkanPath" -ForegroundColor Green
        
        # Add include directory
        $vulkanInclude = Join-Path $vulkanPath "Include"
        if (Test-Path $vulkanInclude) {
            Add-IncludeDirectory -Path $vulkanInclude
        }
        
        # Add library directory
        $vulkanLib = Join-Path $vulkanPath "Lib"
        if (Test-Path $vulkanLib) {
            Add-LibraryDirectory -Path $vulkanLib
        }
        
        # Add bin directory to PATH
        $vulkanBin = Join-Path $vulkanPath "Bin"
        if (Test-Path $vulkanBin) {
            Add-EnvPath -Name "PATH" -Value $vulkanBin
        }
    } else {
        Write-Warning "No Vulkan SDK version found in $vulkanBase"
    }
} else {
    Write-Warning "Vulkan SDK not found at $vulkanBase"
}

# GLFW - detect lib directory
Write-Host "`n[Configuring GLFW]" -ForegroundColor Cyan
$glfwRoot = "C:\Program Files\GLFW"
if (Test-Path $glfwRoot) {
    $glfwLibDir = $null
    foreach ($candidate in @("lib-vc2022", "lib-vc2019", "lib-vc2017", "lib-vc2015")) {
        $testPath = Join-Path $glfwRoot $candidate
        if (Test-Path "$testPath\glfw3dll.lib") {
            $glfwLibDir = $testPath
            break
        }
    }

    if ($glfwLibDir) {
        [Environment]::SetEnvironmentVariable("GLFW_ROOT", $glfwRoot, "Machine")
        [Environment]::SetEnvironmentVariable("GLFW_LIB_DIR", $glfwLibDir, "Machine")
        Write-Host "[GLFW] Library: $glfwLibDir" -ForegroundColor Green
        
        # Add include directory
        $glfwInclude = Join-Path $glfwRoot "include"
        if (Test-Path $glfwInclude) {
            Add-IncludeDirectory -Path $glfwInclude
        }
    } else {
        Write-Warning "GLFW library directory not found"
    }
} else {
    Write-Warning "GLFW not found at $glfwRoot"
}

# FFmpeg
$ffmpegRoot = "C:\Program Files\FFmpeg"
if (Test-Path $ffmpegRoot) {
    [Environment]::SetEnvironmentVariable("FFMPEG_ROOT", $ffmpegRoot, "Machine")
    Write-Host "[FFmpeg] Root: $ffmpegRoot" -ForegroundColor Green
}

# RtAudio
$rtaudioRoot = "C:\Program Files\RtAudio"
if (Test-Path $rtaudioRoot) {
    [Environment]::SetEnvironmentVariable("RTAUDIO_ROOT", $rtaudioRoot, "Machine")
    Write-Host "[RtAudio] Root: $rtaudioRoot" -ForegroundColor Green
}

# Header-only libraries
$headerLibs = @{
    "EIGEN3_INCLUDE_DIR"     = "C:\Program Files\Eigen3"
    "GLM_INCLUDE_DIR"        = "C:\Program Files\glm\include"
    "STB_INCLUDE_DIR"        = "C:\Program Files\stb\include"
    "MAGIC_ENUM_INCLUDE_DIR" = "C:\Program Files\magic_enum\include"
    "LIBXML2_INCLUDE_DIR"    = "C:\Program Files\LibXml2\include"
}

$foundCount = 0
$totalCount = $headerLibs.Count

foreach ($envVar in $headerLibs.GetEnumerator()) {
    if (Test-Path $envVar.Value) {
        [Environment]::SetEnvironmentVariable($envVar.Key, $envVar.Value, "Machine")
        Add-IncludeDirectory -Path $envVar.Value
        Write-Host "[$($envVar.Key)] $($envVar.Value)" -ForegroundColor Green
        $foundCount++
    }
}

Write-Host "Header libraries configured: $foundCount/$totalCount" -ForegroundColor Green
if ($foundCount -lt $totalCount) {
    Write-Warning "Some header libraries not found - builds may fail"
}

Write-Host "`n=== Setup Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Environment variables configured for:" -ForegroundColor Yellow
Write-Host "  • INCLUDE, CPATH: Header search paths" -ForegroundColor Yellow
Write-Host "  • LIB, LIBRARY_PATH: Library search paths" -ForegroundColor Yellow
Write-Host "  • PATH: Executable search paths" -ForegroundColor Yellow
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. RESTART your terminal/PowerShell/VS Code" -ForegroundColor Red
Write-Host "  2. Verify: `$env:MAYAFLUX_ROOT" -ForegroundColor Yellow
Write-Host "  3. cmake -B build -S ." -ForegroundColor Yellow
Write-Host "  4. cmake --build build --config Release" -ForegroundColor Yellow
Write-Host ""