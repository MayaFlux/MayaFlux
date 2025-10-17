#Requires -RunAsAdministrator

$ErrorActionPreference = "Stop"

function Test-Command($cmd) { 
    return [bool](Get-Command $cmd -ErrorAction SilentlyContinue) 
}

function Add-ToSystemPath($path) {
    $current = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if ($current -notmatch [Regex]::Escape($path)) {
        [Environment]::SetEnvironmentVariable("Path", "$current;$path", "Machine")
    }
}

function Add-ToSystemLibPath($path) {
    $currentLib = [Environment]::GetEnvironmentVariable("LIB", "Machine")
    if ($currentLib -notmatch [Regex]::Escape($path)) {
        [Environment]::SetEnvironmentVariable("LIB", "$currentLib;$path", "Machine")
    }
}

function Refresh-PathInSession {
    $env:Path = [Environment]::GetEnvironmentVariable("Path","Machine") + ";" + 
                [Environment]::GetEnvironmentVariable("Path","User")
}

if (-not (Test-Command "winget")) {
    Write-Error "Winget required"
    exit 1
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "Visual Studio not found"
    exit 1
}

if (-not (Test-Command "cmake")) {
    winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements --silent
    Refresh-PathInSession
}

if (-not (Test-Command "git")) {
    winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements --silent
    Refresh-PathInSession
}

if (-not (Test-Command "ninja")) {
    winget install --id Ninja-build.Ninja -e --accept-package-agreements --accept-source-agreements --silent
    Refresh-PathInSession
}

# --- Configuration Variables ---
$LLVM_VERSION = "21.1.3"
$LLVM_ARCH = "x86_64-pc-windows-msvc"
$LLVM_TARBALL_NAME = "clang+llvm-$LLVM_VERSION-$LLVM_ARCH.tar.xz"
$LLVM_URL = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/$LLVM_TARBALL_NAME"
$PSScriptRoot = (Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)

# FIX 1A: Target the required system path for final installation
$LLVM_SYSTEM_ROOT = "C:\Program Files\LLVM_Libs"
$LLVM_INSTALL_ROOT = $LLVM_SYSTEM_ROOT 

# Use a temporary folder for downloading and staging
$LLVM_TEMP_DIR = Join-Path $PSScriptRoot "temp\llvm"
$LLVM_DOWNLOAD_PATH = Join-Path $LLVM_TEMP_DIR $LLVM_TARBALL_NAME

# The path to LLVMConfig.cmake relative to the installation root
$LLVM_CONFIG_PATH_SUFFIX = "lib\cmake\llvm" 

# --- Execution Logic - LLVM Setup ---

Write-Host "Starting LLVM $LLVM_VERSION system dependency setup..."

# 1. Define the final check path
$LLVM_DIR_FULL = Join-Path $LLVM_SYSTEM_ROOT $LLVM_CONFIG_PATH_SUFFIX

# FIX 1B: Primary check is against the INSTALLATION, not the temporary download
if (-not (Test-Path $LLVM_DIR_FULL)) {
    
    Write-Host "LLVM not found at $LLVM_SYSTEM_ROOT. Starting download and installation..."
    
    # Ensure the final system directory exists
    if (-not (Test-Path $LLVM_SYSTEM_ROOT)) {
        Write-Host "Creating system LLVM installation directory: $LLVM_SYSTEM_ROOT"
        mkdir $LLVM_SYSTEM_ROOT | Out-Null
    }

    # Ensure the temporary download directory exists
    if (-not (Test-Path $LLVM_TEMP_DIR)) {
        mkdir $LLVM_TEMP_DIR | Out-Null
    }

    # 2. Download the artifact only if needed
    if (-not (Test-Path $LLVM_DOWNLOAD_PATH)) {
        Write-Host "Downloading $LLVM_TARBALL_NAME from $LLVM_URL..."
        try {
            Invoke-WebRequest -Uri $LLVM_URL -OutFile $LLVM_DOWNLOAD_PATH -MaximumRedirection 5
            Write-Host "Download complete."
        } catch {
            Write-Error "Failed to download LLVM artifact: $($_.Exception.Message)"
            exit 1
        }
    } else {
        Write-Host "LLVM artifact already downloaded to temporary folder."
    }

    # 3. Extract the artifact and move it to the system path
    Write-Host "Extracting $LLVM_TARBALL_NAME into temporary staging area..."
    try {
        # Use native tar.exe to extract to temp directory 
        tar -xf $LLVM_DOWNLOAD_PATH -C $LLVM_TEMP_DIR
        Write-Host "Extraction complete."
        
        # Move contents from staging subfolder into the final system path
        $ExtractedContentDir = (Get-ChildItem -Path $LLVM_TEMP_DIR -Directory | Where-Object { $_.Name -like "clang+llvm-*" }).FullName
        
        if ($ExtractedContentDir) {
            Write-Host "Moving extracted content to $LLVM_SYSTEM_ROOT..."
            # Move all contents (* not the directory itself) 
            Move-Item -Path "$ExtractedContentDir\*" -Destination $LLVM_SYSTEM_ROOT -Force
            # Clean up temporary staging directory
            Remove-Item $ExtractedContentDir -Recurse -Force
        } else {
             Write-Error "Failed to find extracted content directory in staging area."
             exit 1
        }
    } catch {
        Write-Error "Failed to extract or move LLVM artifact: $($_.Exception.Message)"
        exit 1
    }
} else {
    Write-Host "LLVM installation already found at $LLVM_SYSTEM_ROOT."
}


# 4. Configure Environment Variables for CMake
# This sets LLVM_DIR based on the final, system installation path.
$LLVM_DIR_FULL = Join-Path $LLVM_SYSTEM_ROOT $LLVM_CONFIG_PATH_SUFFIX
$env:LLVM_DIR = $LLVM_DIR_FULL
Write-Host "Environment variable LLVM_DIR set to: $env:LLVM_DIR" [1, 2]

# (Optional but Recommended) Add LLVM tools to PATH
$LLVM_BIN_DIR = Join-Path $LLVM_SYSTEM_ROOT "bin"
$env:Path = "$LLVM_BIN_DIR;$env:Path"
Write-Host "LLVM bin directory added to PATH."

# ----------------------------------------------------------------------
#  DIA SDK Setup (REQUIRED for Lila)
# ----------------------------------------------------------------------

# --- DIA SDK Setup (Required for LLVM linking on Windows) ---
Write-Host -ForegroundColor Cyan "--- Locating DIA SDK for LLVM compatibility ---"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

# Query all Visual Studio installations
$vsInstallations = & $vswhere -all -format json | ConvertFrom-Json

if (-not $vsInstallations) {
    Write-Error "No Visual Studio installations found"
    exit 1
}

# Find the first installation with DIA SDK
$diaSdkFound = $false
$diaSdkPath = ""

foreach ($vs in $vsInstallations) {
    $installationPath = $vs.installationPath
    $diaCandidatePath = Join-Path $installationPath "DIA SDK"
    
    if (Test-Path "$diaCandidatePath\lib\amd64\diaguids.lib") {
        $diaSdkPath = $diaCandidatePath
        $diaSdkFound = $true
        Write-Host "Found DIA SDK at: $diaSdkPath"
        break
    }
}

# If not found in standard location, try alternative paths
if (-not $diaSdkFound) {
    $alternativePaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\DIA SDK",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\DIA SDK",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\DIA SDK",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\DIA SDK",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\DIA SDK",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\DIA SDK"
    )
    
    foreach ($path in $alternativePaths) {
        if (Test-Path "$path\lib\amd64\diaguids.lib") {
            $diaSdkPath = $path
            $diaSdkFound = $true
            Write-Host "Found DIA SDK at: $diaSdkPath"
            break
        }
    }
}

if (-not $diaSdkFound) {
    Write-Error "DIA SDK not found. LLVM requires this for linking."
    Write-Error "Ensure Visual Studio with C++ development tools is installed."
    exit 1
}

# Create LLVM lib symlink structure to handle hardcoded paths
$diaTargetDir = "C:\Program Files\LLVM_Libs\dia_sdk_compat"
if (-not (Test-Path "$diaTargetDir\lib\amd64")) {
    Write-Host "Setting up DIA SDK compatibility layer for LLVM..."
    New-Item -ItemType Directory -Path "$diaTargetDir\lib\amd64" -Force | Out-Null
    
    # Copy DIA SDK files to compatibility location
    Copy-Item -Path "$diaSdkPath\lib\amd64\diaguids.lib" -Destination "$diaTargetDir\lib\amd64\" -Force
    Write-Host "DIA SDK compatibility layer created at: $diaTargetDir"
}

# Set environment variable for CMake (optional, but helpful)
[Environment]::SetEnvironmentVariable("DIA_SDK_PATH", $diaSdkPath, "Machine")
Write-Host "Environment variable DIA_SDK_PATH set to: $diaSdkPath"

# ----------------------------------------------------------------------
#  ADD LibXml2 SETUP (REQUIRED for Lila)
# ----------------------------------------------------------------------

Write-Host -ForegroundColor Cyan "--- LibXml2 Dependency Setup ---"

# These paths are set to system conventions.
$LIBXML2_INSTALL_ROOT = "C:\Program Files\LibXml2" 
$LIBXML2_TEMP_DIR = Join-Path $PSScriptRoot "temp\libxml2"

# FIX 2A: Using the latest official source release URL and filename
$LIBXML2_VERSION = "2.15.0" 
$LIBXML2_TARBALL_NAME = "libxml2-$LIBXML2_VERSION.tar.xz" 
$LIBXML2_URL = "https://download.gnome.org/sources/libxml2/2.15/$LIBXML2_TARBALL_NAME" 

$LIBXML2_DOWNLOAD_PATH = Join-Path $LIBXML2_TEMP_DIR $LIBXML2_TARBALL_NAME

# Calculate final required header file path
$LIBXML2_INCLUDE_DIR_TOOLCHAIN = "C:/Program Files/LibXml2/include"
$LIBXML2_LIBRARY_DUMMY_FILE = "C:/Program Files/LibXml2/include/libxml/xmlversion.h" 

# FIX: Check for the existence of the critical header needed for the dummy path
if (-not (Test-Path $LIBXML2_LIBRARY_DUMMY_FILE -PathType Leaf)) {

    Write-Host "LibXml2 header file 'xmlversion.h' not found. Starting installation/repair..."

    # Clean the installation root just in case a failed previous extraction left debris
    if (Test-Path $LIBXML2_INSTALL_ROOT) {
        Write-Host "Cleaning up old LibXml2 installation debris..."
        Remove-Item $LIBXML2_INSTALL_ROOT -Recurse -Force
    }
    
    # Ensure target directories exist
    if (-not (Test-Path $LIBXML2_TEMP_DIR)) {
        mkdir $LIBXML2_TEMP_DIR | Out-Null
    }
    if (-not (Test-Path $LIBXML2_INSTALL_ROOT)) {
        mkdir $LIBXML2_INSTALL_ROOT | Out-Null
    }

    # Download source file
    if (-not (Test-Path $LIBXML2_DOWNLOAD_PATH)) {
        Write-Host "Downloading LibXml2 source $LIBXML2_VERSION from official site..."
        try {
            Invoke-WebRequest -Uri $LIBXML2_URL -OutFile $LIBXML2_DOWNLOAD_PATH -MaximumRedirection 5
            Write-Host "Download complete."
        } catch {
            Write-Error "Failed to download LibXml2 artifact: $($_.Exception.Message)"
            exit 1
        }
    } else {
        Write-Host "LibXml2 artifact already downloaded to temporary folder."
    }

    # Extract the tar.xz source file to the install root (C:\Program Files\LibXml2)
    Write-Host "Extracting LibXml2 source to $LIBXML2_INSTALL_ROOT..."
    try {
        # tar extracts into a folder named libxml2-2.15.0 inside $LIBXML2_INSTALL_ROOT
        tar -xf $LIBXML2_DOWNLOAD_PATH -C $LIBXML2_INSTALL_ROOT
        
        # Move contents from the extraction subfolder to the root
        $ExtractedRootName = $LIBXML2_TARBALL_NAME -replace '\.tar\.xz$'
        $ExtractedContentDir = Join-Path $LIBXML2_INSTALL_ROOT $ExtractedRootName
        
        if (Test-Path $ExtractedContentDir) {
            Write-Host "Moving extracted content to the final root..."
            # Move all contents (*.*, include, doc, etc.) to C:\Program Files\LibXml2\
            Get-ChildItem -Path "$ExtractedContentDir\*" -Force | Move-Item -Destination $LIBXML2_INSTALL_ROOT -Force
            
            # Remove the now-empty extraction subdirectory
            Remove-Item $ExtractedContentDir -Recurse -Force
            Write-Host "Extraction and flattening complete."
        } else {
            Write-Error "Extraction failed: Cannot find expected subdirectory '$ExtractedRootName'."
            exit 1
        }
    } catch {
        Write-Error "Failed to extract LibXml2 artifact using tar.exe."
        exit 1
    }

} else {
    Write-Host "LibXml2 installation already found at $LIBXML2_INSTALL_ROOT."
}

$env:LIBXML2_INCLUDE_DIR = $LIBXML2_INCLUDE_DIR_TOOLCHAIN 
$env:LIBXML2_LIBRARY = $LIBXML2_LIBRARY_DUMMY_FILE
Write-Host "Environment variables for LibXml2 set (for immediate session use)."

$LLVM_INCLUDE_PATH_TOOLCHAIN = "C:/Program Files/LLVM_Libs/include"
$LLVM_CORE_LIBRARY_PATH = "C:/Program Files/LLVM_Libs/lib/LLVMCore.lib" 

if (-not (Test-Path "$($LLVM_CORE_LIBRARY_PATH -replace '/', '\')" -PathType Leaf)) {
    Write-Error "CRITICAL: The core LLVM library file (LLVMCore.lib) was not found in C:\Program Files\LLVM_Libs\lib."
    Write-Error "The pre-built binaries you downloaded may be incomplete or lack the required static libraries for linking."
    exit 1
}

# ----------------------------------------------------------------------
#  VULKAN SDK SETUP
# ----------------------------------------------------------------------

# Vulkan SDK (full SDK, not just runtime)
$vulkanSdkPath = "C:\VulkanSDK"
if (-not (Test-Path "$vulkanSdkPath\*\Include\vulkan\vulkan.h")) {
    Write-Output "Installing Vulkan SDK..."
    $vulkanInstaller = Join-Path $env:TEMP "VulkanSDK-Installer.exe"
    Invoke-WebRequest -Uri "https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe" -OutFile $vulkanInstaller -UseBasicParsing
    
    # Install with specific components (unattended)
    $installArgs = @(
        "/S",
        "/D=$vulkanSdkPath"
    )
    Start-Process -FilePath $vulkanInstaller -ArgumentList $installArgs -Wait
    Remove-Item $vulkanInstaller -ErrorAction SilentlyContinue
    
    # Find installed version directory
    $vulkanVersionDir = Get-ChildItem "$vulkanSdkPath" -Directory | Select-Object -First 1
    if ($vulkanVersionDir) {
        $vulkanSdkFull = $vulkanVersionDir.FullName
        [Environment]::SetEnvironmentVariable("VULKAN_SDK", $vulkanSdkFull, "Machine")
        [Environment]::SetEnvironmentVariable("VK_SDK_PATH", $vulkanSdkFull, "Machine")
        Add-ToSystemPath "$vulkanSdkFull\Bin"
        Refresh-PathInSession
        Write-Output "Vulkan SDK installed at: $vulkanSdkFull"
    } else {
        Write-Error "Vulkan SDK installation failed"
        exit 1
    }
}

# FFmpeg
$ffmpegDir = "C:\Program Files\FFmpeg"
if (-not (Test-Path "$ffmpegDir\bin\ffmpeg.exe")) {
    Write-Output "Installing FFmpeg..."
    
    $sevenZipPath = "${env:ProgramFiles}\7-Zip\7z.exe"
    if (-not (Test-Path $sevenZipPath)) {
        winget install --id 7zip.7zip -e --accept-package-agreements --accept-source-agreements --silent
        Refresh-PathInSession
    }
    if (-not (Test-Path $sevenZipPath)) {
        Write-Error "7-Zip not found at $sevenZipPath"
        exit 1
    }
    
    $archive = Join-Path $env:TEMP "ffmpeg.7z"
    $extractTemp = "$env:TEMP\ffmpeg-extract"
    
    # Clean up any previous attempts
    if (Test-Path $extractTemp) {
        Remove-Item $extractTemp -Recurse -Force
    }
    if (Test-Path $ffmpegDir) {
        Remove-Item $ffmpegDir -Recurse -Force
    }
    
    Invoke-WebRequest -Uri "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z" -OutFile $archive -UseBasicParsing
    
    # Extract
    New-Item -ItemType Directory -Path $extractTemp -Force | Out-Null
    & $sevenZipPath x $archive -o"$extractTemp" -y | Out-Null
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "7z extraction failed with exit code $LASTEXITCODE"
        exit 1
    }
    
    # Find the extracted directory (has version in name like ffmpeg-7.1-full_build-shared)
    $extractedDir = Get-ChildItem $extractTemp -Directory | Select-Object -First 1
    if (-not $extractedDir) {
        Write-Error "No directory found in extracted FFmpeg archive"
        exit 1
    }
    
    # Move to final location
    New-Item -ItemType Directory -Path $ffmpegDir -Force | Out-Null
    Get-ChildItem $extractedDir.FullName | Move-Item -Destination $ffmpegDir -Force
    
    # Verify
    if (-not (Test-Path "$ffmpegDir\bin\ffmpeg.exe")) {
        Write-Error "FFmpeg installation failed - ffmpeg.exe not found"
        exit 1
    }
    
    # Cleanup
    Remove-Item $archive -ErrorAction SilentlyContinue
    Remove-Item $extractTemp -Recurse -Force -ErrorAction SilentlyContinue
    
    [Environment]::SetEnvironmentVariable("FFMPEG_ROOT", $ffmpegDir, "Machine")
    Add-ToSystemPath "$ffmpegDir\bin"
    Write-Output "FFmpeg installed at: $ffmpegDir"
}

# GLFW - Use prebuilt binaries (already has /MT)
$glfwDir = "C:\Program Files\GLFW"
if (-not (Test-Path "$glfwDir\include\GLFW\glfw3.h")) {
    Write-Output "Installing GLFW prebuilt binaries..."
    
    $archive = Join-Path $env:TEMP "glfw.zip"
    $extractTemp = "$env:TEMP\glfw-extract"
    
    if (Test-Path $extractTemp) {
        Remove-Item $extractTemp -Recurse -Force
    }
    
    Invoke-WebRequest -Uri "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip" -OutFile $archive -UseBasicParsing
    Expand-Archive -Path $archive -DestinationPath $extractTemp -Force
    
    $glfwExtracted = Get-ChildItem "$extractTemp\glfw-*" -Directory | Select-Object -First 1
    if (-not $glfwExtracted) {
        Write-Error "GLFW extraction failed"
        exit 1
    }
    
    New-Item -ItemType Directory -Path $glfwDir -Force | Out-Null
    Copy-Item -Recurse "$glfwExtracted\*" $glfwDir -Force
    
    Remove-Item $archive -ErrorAction SilentlyContinue
    Remove-Item $extractTemp -Recurse -Force -ErrorAction SilentlyContinue
    
    [Environment]::SetEnvironmentVariable("GLFW_ROOT", $glfwDir, "Machine")
    Write-Output "GLFW installed at: $glfwDir"
} else {
    Write-Host "GLFW installation already found at $glfwDir."
}

# Dynamically find the latest available Visual Studio version for GLFW libs
Write-Host "Detecting available Visual Studio versions for GLFW..."

$glfwLibCandidates = @(
    "lib-vc2022",  # VS 2022 (preferred, most recent)
    "lib-vc2019",  # VS 2019
    "lib-vc2017",  # VS 2017
    "lib-vc2015"   # VS 2015
)

$foundLibDir = $null
foreach ($candidate in $glfwLibCandidates) {
    $candidatePath = Join-Path $glfwDir $candidate
    if (Test-Path "$candidatePath\glfw3dll.lib") {
        $foundLibDir = $candidatePath
        Write-Host "Found GLFW libs for Visual Studio version: $candidate"
        break
    }
}

if (-not $foundLibDir) {
    Write-Error "No compatible GLFW library directory found for any Visual Studio version"
    Write-Error "Expected one of: $($glfwLibCandidates -join ', ')"
    exit 1
}

# Set CMake toolchain variables using the detected lib directory
$glfwLibRelativeName = Split-Path -Leaf $foundLibDir

[Environment]::SetEnvironmentVariable("GLFW_ROOT", $glfwDir, "Machine")
[Environment]::SetEnvironmentVariable("GLFW_LIB_DIR", $foundLibDir, "Machine")

Write-Host "GLFW configuration complete:"
Write-Host "  Root: $glfwDir"
Write-Host "  Libs: $foundLibDir"

# RtAudio
$rtaudioDir = "C:\Program Files\RtAudio"
if (-not (Test-Path "$rtaudioDir\include\rtaudio\RtAudio.h")) {
    Write-Output "Building RtAudio..."
    
    $rtaudioSrc = "$env:TEMP\rtaudio-src"
    $rtaudioBuild = "$env:TEMP\rtaudio-build"
    
    if (Test-Path $rtaudioSrc) {
        Remove-Item $rtaudioSrc -Recurse -Force
    }
    if (Test-Path $rtaudioBuild) {
        Remove-Item $rtaudioBuild -Recurse -Force
    }
    
    $env:GIT_REDIRECT_STDERR = '2>&1'
    git clone --depth 1 --branch 6.0.1 --quiet "https://github.com/thestk/rtaudio.git" $rtaudioSrc 2>$null
    cmake -S $rtaudioSrc -B $rtaudioBuild -DCMAKE_INSTALL_PREFIX="$rtaudioDir" -DCMAKE_BUILD_TYPE=Release -DRTAUDIO_BUILD_TESTING=OFF -DRTAUDIO_API_WASAPI=ON -DRTAUDIO_API_DS=ON -G "Visual Studio 17 2022" 2>$null | Out-Null
    cmake --build $rtaudioBuild --config Release 2>$null | Out-Null
    cmake --install $rtaudioBuild --config Release 2>$null | Out-Null
    
    Remove-Item $rtaudioSrc -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $rtaudioBuild -Recurse -Force -ErrorAction SilentlyContinue
    
    [Environment]::SetEnvironmentVariable("RTAUDIO_ROOT", $rtaudioDir, "Machine")
    Add-ToSystemPath "$rtaudioDir\bin"
    Write-Output "RtAudio installed at: $rtaudioDir"
}

# Eigen3 (header-only, just extract)
$eigenDir = "C:\Program Files\Eigen3"
if (-not (Test-Path "$eigenDir\Eigen\Dense")) {
    Write-Output "Installing Eigen3..."
    
    $archive = Join-Path $env:TEMP "eigen.zip"
    $extractTemp = "$env:TEMP\eigen-extract"
    
    if (Test-Path $extractTemp) {
        Remove-Item $extractTemp -Recurse -Force
    }
    
    # Download Eigen 3.4.0
    Invoke-WebRequest -Uri "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip" -OutFile $archive -UseBasicParsing
    Expand-Archive -Path $archive -DestinationPath $extractTemp -Force
    
    $eigenExtracted = Get-ChildItem "$extractTemp\eigen-*" -Directory | Select-Object -First 1
    if (-not $eigenExtracted) {
        Write-Error "Eigen extraction failed"
        exit 1
    }
    
    New-Item -ItemType Directory -Path $eigenDir -Force | Out-Null
    Copy-Item -Recurse "$eigenExtracted\*" $eigenDir -Force
    
    Remove-Item $archive -ErrorAction SilentlyContinue
    Remove-Item $extractTemp -Recurse -Force -ErrorAction SilentlyContinue
    
    [Environment]::SetEnvironmentVariable("EIGEN3_ROOT", $eigenDir, "Machine")
    Write-Output "Eigen3 installed at: $eigenDir"
}

# ----------------------------------------------------------------------
#  MAGIC_ENUM SETUP (Header-only library)
# ----------------------------------------------------------------------

Write-Host -ForegroundColor Cyan "--- Magic Enum Dependency Setup ---"

$MAGIC_ENUM_VERSION = "v0.9.5"
$MAGIC_ENUM_INSTALL_ROOT = "C:\Program Files\magic_enum"
$MAGIC_ENUM_TEMP_DIR = Join-Path $PSScriptRoot "temp\magic_enum"
$MAGIC_ENUM_HEADER_FILE = "$MAGIC_ENUM_INSTALL_ROOT\include\magic_enum.hpp"

if (-not (Test-Path $MAGIC_ENUM_HEADER_FILE -PathType Leaf)) {
    Write-Host "Magic Enum not found. Installing..."
    
    # Create directories
    if (-not (Test-Path $MAGIC_ENUM_TEMP_DIR)) {
        mkdir $MAGIC_ENUM_TEMP_DIR | Out-Null
    }
    if (-not (Test-Path $MAGIC_ENUM_INSTALL_ROOT)) {
        mkdir "$MAGIC_ENUM_INSTALL_ROOT\include" -Force | Out-Null
    }

    # Download and extract
    $MAGIC_ENUM_URL = "https://github.com/Neargye/magic_enum/archive/refs/tags/$MAGIC_ENUM_VERSION.zip"
    $MAGIC_ENUM_ZIP_PATH = Join-Path $MAGIC_ENUM_TEMP_DIR "magic_enum.zip"

    if (-not (Test-Path $MAGIC_ENUM_ZIP_PATH)) {
        Write-Host "Downloading Magic Enum..."
        try {
            Invoke-WebRequest -Uri $MAGIC_ENUM_URL -OutFile $MAGIC_ENUM_ZIP_PATH -MaximumRedirection 5
            Write-Host "Download complete."
        } catch {
            Write-Error "Failed to download Magic Enum: $($_.Exception.Message)"
            exit 1
        }
    }

    # Extract and copy headers
    Write-Host "Extracting Magic Enum..."
    try {
        Expand-Archive -Path $MAGIC_ENUM_ZIP_PATH -DestinationPath $MAGIC_ENUM_TEMP_DIR -Force
        
        $ExtractedDir = Join-Path $MAGIC_ENUM_TEMP_DIR "magic_enum-$($MAGIC_ENUM_VERSION.TrimStart('v'))"
        $SrcIncludeDir = Join-Path $ExtractedDir "include"
        
        if (Test-Path $SrcIncludeDir) {
            Copy-Item -Recurse -Path "$SrcIncludeDir\*" -Destination "$MAGIC_ENUM_INSTALL_ROOT\include\" -Force
            Write-Host "Magic Enum headers installed to $MAGIC_ENUM_INSTALL_ROOT\include"
        } else {
            Write-Error "Magic Enum extraction failed: include directory not found"
            exit 1
        }
    } catch {
        Write-Error "Failed to extract Magic Enum: $($_.Exception.Message)"
        exit 1
    }
} else {
    Write-Host "Magic Enum installation already found at $MAGIC_ENUM_INSTALL_ROOT."
}

# Generate toolchain
$configDir = "C:\MayaFlux"
New-Item -ItemType Directory -Path $configDir -Force | Out-Null

# In setup_windows_binaries.ps1, replace toolchain generation with:

# Get actual Vulkan SDK path
$vulkanSdkActual = if ($env:VULKAN_SDK) { 
    $env:VULKAN_SDK 
} else { 
    $vulkanVersionDir = Get-ChildItem "C:\VulkanSDK" -Directory | Select-Object -First 1
    if ($vulkanVersionDir) { $vulkanVersionDir.FullName } else { "" }
}

# Define environment functions BEFORE using them
function Update-SystemIncludePath {
    param([string[]]$IncludePaths)
    
    $currentInclude = [Environment]::GetEnvironmentVariable("INCLUDE", "Machine")
    if (-not $currentInclude) {
        $currentInclude = ""
    }
    
    $updated = $false
    foreach ($path in $IncludePaths) {
        if ($currentInclude -notmatch [Regex]::Escape($path)) {
            if ($currentInclude -eq "") {
                $currentInclude = $path
            } else {
                $currentInclude = "$currentInclude;$path"
            }
            $updated = $true
            Write-Host "Added to INCLUDE: $path"
        }
    }
    
    if ($updated) {
        [Environment]::SetEnvironmentVariable("INCLUDE", $currentInclude, "Machine")
        Write-Host "System INCLUDE environment variable updated"
    }
}

function Refresh-Environment {
    $env:INCLUDE = [Environment]::GetEnvironmentVariable("INCLUDE", "Machine")
    $env:LIB = [Environment]::GetEnvironmentVariable("LIB", "Machine")
    Refresh-PathInSession
}

# ----------------------------------------------------------------------
#  COLLECT ALL INCLUDE PATHS (Dependencies + MayaFlux)
# ----------------------------------------------------------------------

Write-Host -ForegroundColor Cyan "--- Collecting All Include Paths ---"

$includePaths = @()

# Dependency includes
$llvmIncludePath = Join-Path $LLVM_SYSTEM_ROOT "include"
if (Test-Path $llvmIncludePath) {
    $includePaths += $llvmIncludePath
}

$libxml2IncludePath = Join-Path $LIBXML2_INSTALL_ROOT "include"
if (Test-Path $libxml2IncludePath) {
    $includePaths += $libxml2IncludePath
}

$vulkanSdkActual = if ($env:VULKAN_SDK) { 
    $env:VULKAN_SDK 
} else { 
    $vulkanVersionDir = Get-ChildItem "C:\VulkanSDK" -Directory | Select-Object -First 1
    if ($vulkanVersionDir) { $vulkanVersionDir.FullName } else { "" }
}
if ($vulkanSdkActual -and (Test-Path "$vulkanSdkActual\Include")) {
    $includePaths += "$vulkanSdkActual\Include"
}

$ffmpegIncludePath = Join-Path $ffmpegDir "include"
if (Test-Path $ffmpegIncludePath) {
    $includePaths += $ffmpegIncludePath
}

$glfwIncludePath = Join-Path $glfwDir "include"
if (Test-Path $glfwIncludePath) {
    $includePaths += $glfwIncludePath
}

$rtaudioIncludePath = Join-Path $rtaudioDir "include"
if (Test-Path $rtaudioIncludePath) {
    $includePaths += $rtaudioIncludePath
}

$eigenIncludePath = $eigenDir
if (Test-Path $eigenIncludePath) {
    $includePaths += $eigenIncludePath
}

$magicEnumIncludePath = Join-Path $MAGIC_ENUM_INSTALL_ROOT "include"
if (Test-Path $magicEnumIncludePath) {
    $includePaths += $magicEnumIncludePath
}

if ($diaSdkFound -and $diaSdkPath) {
    $diaIncludePath = Join-Path $diaSdkPath "include"
    if (Test-Path $diaIncludePath) {
        $includePaths += $diaIncludePath
    }
}

# ----------------------------------------------------------------------
#  MAYAFLUX PATHS SETUP
# ----------------------------------------------------------------------

Write-Host -ForegroundColor Cyan "--- Registering MayaFlux Installation Paths ---"

$MAYAFLUX_INSTALL_ROOT = "C:\MayaFlux"
$MAYAFLUX_INCLUDE_PATH = Join-Path $MAYAFLUX_INSTALL_ROOT "include"
$MAYAFLUX_LIB_PATH = Join-Path $MAYAFLUX_INSTALL_ROOT "lib" 
$MAYAFLUX_BIN_PATH = Join-Path $MAYAFLUX_INSTALL_ROOT "bin"

# Create the installation directory structure
if (-not (Test-Path $MAYAFLUX_INSTALL_ROOT)) {
    Write-Host "Creating MayaFlux installation directory: $MAYAFLUX_INSTALL_ROOT"
    New-Item -ItemType Directory -Path $MAYAFLUX_INSTALL_ROOT -Force | Out-Null
    New-Item -ItemType Directory -Path $MAYAFLUX_INCLUDE_PATH -Force | Out-Null
    New-Item -ItemType Directory -Path $MAYAFLUX_LIB_PATH -Force | Out-Null
    New-Item -ItemType Directory -Path $MAYAFLUX_BIN_PATH -Force | Out-Null
}

# Add MayaFlux to include paths collection
$includePaths += $MAYAFLUX_INCLUDE_PATH

# : Add the MayaFlux lib directory to the system LIB variable
Add-ToSystemLibPath "C:\MayaFlux\lib"
Write-Host "Added MayaFlux lib to system LIB: C:\MayaFlux\lib"

# ----------------------------------------------------------------------
#  UPDATE ALL ENVIRONMENT VARIABLES
# ----------------------------------------------------------------------

Write-Host -ForegroundColor Cyan "--- Updating System Environment Variables ---"

# Update INCLUDE with ALL paths (dependencies + MayaFlux)
Update-SystemIncludePath -IncludePaths $includePaths

# Add MayaFlux lib path to system LIB
$currentLib = [Environment]::GetEnvironmentVariable("LIB", "Machine")
if ($currentLib -notmatch [Regex]::Escape($MAYAFLUX_LIB_PATH)) {
    if ($currentLib -eq "") {
        [Environment]::SetEnvironmentVariable("LIB", $MAYAFLUX_LIB_PATH, "Machine")
    } else {
        [Environment]::SetEnvironmentVariable("LIB", "$MAYAFLUX_LIB_PATH;$currentLib", "Machine")
    }
    Write-Host "Added to LIB: $MAYAFLUX_LIB_PATH"
}

# Add MayaFlux bin path to system PATH
Add-ToSystemPath $MAYAFLUX_BIN_PATH

# Refresh current session once
Refresh-Environment

Write-Host "MayaFlux environment paths registered:"
Write-Host "  INCLUDE: $MAYAFLUX_INCLUDE_PATH"
Write-Host "  LIB: $MAYAFLUX_LIB_PATH"
Write-Host "  PATH: $MAYAFLUX_BIN_PATH"
Write-Host ""

Write-Host "Current INCLUDE paths:"
$env:INCLUDE -split ';' | ForEach-Object { Write-Host "  - $_" }

$toolchainContent = @"
# MayaFlux Windows Initial Cache

set(LLVM_ROOT "C:/Program Files/LLVM_Libs" CACHE PATH "" FORCE)
set(LLVM_DIR "$LLVM_DIR_TOOLCHAIN" CACHE PATH "" FORCE)
set(Clang_DIR "$LLVM_CLANG_DIR_TOOLCHAIN" CACHE PATH "" FORCE)

set(LLVM_INCLUDE_DIR "$LLVM_INCLUDE_PATH_TOOLCHAIN" CACHE PATH "" FORCE)
set(LLVM_LIBRARY "$LLVM_CORE_LIBRARY_PATH" CACHE FILEPATH "" FORCE)

list(APPEND CMAKE_PREFIX_PATH "$LLVM_DIR_TOOLCHAIN")
list(APPEND CMAKE_PREFIX_PATH "$LLVM_CLANG_DIR_TOOLCHAIN")

set(LIBXML2_INCLUDE_DIR "$LIBXML2_INCLUDE_DIR_TOOLCHAIN" CACHE PATH "" FORCE)
set(LIBXML2_LIBRARY "$LIBXML2_LIBRARY_DUMMY_FILE" CACHE FILEPATH "" FORCE)

set(VULKAN_SDK "$($vulkanSdkActual -replace '\\','/')" CACHE PATH "" FORCE)
set(Vulkan_INCLUDE_DIR "$($vulkanSdkActual -replace '\\','/')/Include" CACHE PATH "" FORCE)
set(Vulkan_LIBRARY "$($vulkanSdkActual -replace '\\','/')/Lib/vulkan-1.lib" CACHE FILEPATH "" FORCE)

set(FFMPEG_ROOT "C:/Program Files/FFmpeg" CACHE PATH "" FORCE)
set(GLFW_ROOT "C:/Program Files/GLFW" CACHE PATH "" FORCE)
set(RTAUDIO_ROOT "C:/Program Files/RtAudio" CACHE PATH "" FORCE)

set(CMAKE_PREFIX_PATH "C:/Program Files/RtAudio/share/rtaudio;C:/Program Files/LLVM_Libs/lib/cmake/llvm;C:/Program Files/LLVM/lib/cmake/clang" CACHE STRING "" FORCE)

set(GLFW_INCLUDE_DIR "C:/Program Files/GLFW/include" CACHE PATH "" FORCE)
set(GLFW_LIBRARY "$($glfwLibDir -replace '\\','/')/glfw3dll.lib" CACHE FILEPATH "" FORCE)
set(GLFW_DLL "$($glfwLibDir -replace '\\','/')/glfw3.dll" CACHE FILEPATH "" FORCE)

set(AVCODEC_INCLUDE_DIR "C:/Program Files/FFmpeg/include" CACHE PATH "" FORCE)
set(AVFORMAT_INCLUDE_DIR "C:/Program Files/FFmpeg/include" CACHE PATH "" FORCE)
set(AVUTIL_INCLUDE_DIR "C:/Program Files/FFmpeg/include" CACHE PATH "" FORCE)
set(SWRESAMPLE_INCLUDE_DIR "C:/Program Files/FFmpeg/include" CACHE PATH "" FORCE)
set(SWSCALE_INCLUDE_DIR "C:/Program Files/FFmpeg/include" CACHE PATH "" FORCE)

set(AVCODEC_LIBRARY "C:/Program Files/FFmpeg/lib/avcodec.lib" CACHE FILEPATH "" FORCE)
set(AVFORMAT_LIBRARY "C:/Program Files/FFmpeg/lib/avformat.lib" CACHE FILEPATH "" FORCE)
set(AVUTIL_LIBRARY "C:/Program Files/FFmpeg/lib/avutil.lib" CACHE FILEPATH "" FORCE)
set(SWRESAMPLE_LIBRARY "C:/Program Files/FFmpeg/lib/swresample.lib" CACHE FILEPATH "" FORCE)
set(SWSCALE_LIBRARY "C:/Program Files/FFmpeg/lib/swscale.lib" CACHE FILEPATH "" FORCE)

set(EIGEN3_ROOT "C:/Program Files/Eigen3" CACHE PATH "" FORCE)
set(EIGEN3_INCLUDE_DIR "C:/Program Files/Eigen3" CACHE PATH "" FORCE)

set(MAGIC_ENUM_INCLUDE_DIR "C:/Program Files/magic_enum/include" CACHE PATH "" FORCE)
"@

Set-Content -Path "C:\MayaFlux\MayaFluxToolchain.cmake" -Value $toolchainContent

Write-Output ""
Write-Output "=== Installation Complete ==="
Write-Output "LLVM: $LLVM_SYSTEM_ROOT"
Write-Output "LibXml2: $LIBXML2_INSTALL_ROOT"
Write-Output "Vulkan SDK: $env:VULKAN_SDK"
Write-Output "FFmpeg: $ffmpegDir"
Write-Output "GLFW: $glfwDir"
Write-Output "RtAudio: $rtaudioDir"
Write-Output ""
Write-Output "Toolchain: C:\MayaFlux\MayaFluxToolchain.cmake"
Write-Output ""
Write-Output "Restart your terminal, then build with:"
Write-Output '  cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:\MayaFlux\MayaFluxToolchain.cmake"'
Write-Output "  cmake --build build --config Release"
