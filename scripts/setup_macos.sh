#!/bin/zsh

echo "MayaFlux macOS System Setup"
echo "==========================="
echo

# Get the project root directory (one level up from the script location)
SCRIPT_DIR="${0:a:h}"
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

# Check if script has execute permissions
if [ ! -x "$0" ]; then
    echo "Warning: This script doesn't have execute permissions."
    echo "You may need to run: chmod +x $0"
    echo
fi

# Check macOS version requirement
MACOS_VERSION=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo $MACOS_VERSION | cut -d. -f1)
MACOS_MINOR=$(echo $MACOS_VERSION | cut -d. -f2)

echo "Detected macOS version: $MACOS_VERSION"

if [ "$MACOS_MAJOR" -lt 14 ]; then
    echo "❌ Error: MayaFlux requires macOS 14 (Sonoma) or later for C++23 support."
    echo "Current macOS version: $MACOS_VERSION"
    echo "Please upgrade to macOS 14+ to use MayaFlux."
    echo
    echo "Academic users: macOS 14 provides excellent compatibility for institutional machines."
    exit 1
elif [ "$MACOS_MAJOR" -eq 14 ]; then
    echo "✅ macOS 14 detected - excellent compatibility for academic/institutional use"
else
    echo "✅ macOS $MACOS_MAJOR detected - latest C++23 features available"
fi

# Check macOS architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    echo "Detected Apple Silicon (M1/M2/M3) Mac"
    HOMEBREW_PREFIX="/opt/homebrew"
else
    echo "Detected Intel Mac"
    HOMEBREW_PREFIX="/usr/local"
fi

# Check if Git is installed
if ! command -v git &>/dev/null; then
    echo "Error: Git is required but not found."
    echo "Please install Git from https://git-scm.com/download/mac"
    echo "Or install via Homebrew: brew install git"
    exit 1
fi

# Check if Xcode is installed
if ! xcode-select -p &>/dev/null; then
    echo "Error: Xcode command line tools are not installed."
    echo "Please install by running: xcode-select --install"
    exit 1
fi

# Check if Homebrew is installed
if ! command -v brew &>/dev/null; then
    echo "Homebrew is not installed. Would you like to install it? (y/n)"
    read -r install_brew
    if [[ $install_brew =~ ^[Yy]$ ]]; then
        echo "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

        # Add Homebrew to PATH for this session
        if [[ -f "$HOMEBREW_PREFIX/bin/brew" ]]; then
            eval "$($HOMEBREW_PREFIX/bin/brew shellenv)"
        elif [[ -f /usr/local/bin/brew ]]; then
            eval "$(/usr/local/bin/brew shellenv)"
        else
            echo "Error: Homebrew installation path not found."
            exit 1
        fi
    else
        echo "Homebrew is required to install dependencies. Exiting."
        exit 1
    fi
fi

echo "Installing required packages..."
brew install cmake rtaudio ffmpeg googletest pkg-config eigen onedpl magic_enum fmt glfw

if [ $? -ne 0 ]; then
    echo "Error: Failed to install one or more packages."
    echo "Please check the output above for details."
    exit 1
fi

# Verify package installation
echo "Verifying package installation..."
MISSING_PACKAGES=""
for pkg in cmake rtaudio glfw ffmpeg googletest pkg-config eigen onedpl magic_enum fmt; do
    if ! brew list --formula | grep -q "^${pkg}$"; then
        MISSING_PACKAGES="$MISSING_PACKAGES $pkg"
    fi
done

if [ -n "$MISSING_PACKAGES" ]; then
    echo "Warning: The following packages may not be installed correctly:$MISSING_PACKAGES"
    echo "You may need to install them manually."
fi

# ------------------------------------------------------------
# Vulkan SDK (LunarG) Installation - Always Latest Version
# ------------------------------------------------------------

echo "🔽 Fetching latest Vulkan SDK version for macOS..."
VULKAN_VERSION=$(curl -s https://vulkan.lunarg.com/sdk/latest/mac.txt)

if [ -z "$VULKAN_VERSION" ]; then
    echo "❌ Could not fetch latest Vulkan SDK version."
    exit 1
fi

echo "🔁 Using latest Vulkan SDK $VULKAN_VERSION"

VULKAN_SDK_DIR="$HOME/VulkanSDK/$VULKAN_VERSION/macOS"
TMP_DMG="/tmp/vulkan-sdk-$VULKAN_VERSION.dmg"
URL="https://sdk.lunarg.com/sdk/download/$VULKAN_VERSION/mac/vulkan_sdk.dmg"

if [ -d "$VULKAN_SDK_DIR" ]; then
    echo "✅ Vulkan SDK $VULKAN_VERSION already installed at $VULKAN_SDK_DIR"
else
    echo "⬇️  Downloading from: $URL"
    curl -L "$URL" -o "$TMP_DMG" --progress-bar || {
        echo "❌ Download failed."
        exit 1
    }

    echo "📦 Mounting DMG..."
    MOUNT_POINT=$(hdiutil attach "$TMP_DMG" -nobrowse -quiet | grep Volumes | awk '{print $3}')

    echo "📁 Installing Vulkan SDK..."
    INSTALLER_PKG=$(find "$MOUNT_POINT" -name "*.pkg" | head -n1)
    if [ -z "$INSTALLER_PKG" ]; then
        echo "❌ No .pkg found in mounted image."
        hdiutil detach "$MOUNT_POINT" -quiet || true
        rm -f "$TMP_DMG"
        exit 1
    fi

    sudo installer -pkg "$INSTALLER_PKG" -target / >/dev/null

    echo "💿 Unmounting DMG..."
    hdiutil detach "$MOUNT_POINT" -quiet || true
    rm -f "$TMP_DMG"

    echo "✅ Vulkan SDK $VULKAN_VERSION installed at $VULKAN_SDK_DIR"
fi

export VULKAN_SDK="$VULKAN_SDK_DIR"
export PATH="$VULKAN_SDK/bin:$PATH"
export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH"
export VK_ICD_FILENAMES="$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json"

echo "VULKAN_SDK set to $VULKAN_SDK"

# Check system Clang version
echo "Checking system Clang..."
if command -v clang++ &>/dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo "✓ System Clang found: $CLANG_VERSION"

    if [ "$MACOS_MAJOR" -eq 14 ]; then
        echo "Note: Using system Apple Clang with compatibility workarounds for macOS 14"
    else
        echo "Note: Using system Apple Clang with modern C++23 support"
    fi
else
    echo "❌ Error: Clang not found. Please ensure Xcode command line tools are installed."
    exit 1
fi

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)

if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 15 ]); then
    echo "Warning: CMake version $CMAKE_VERSION may be too old."
    echo "MayaFlux recommends CMake 3.15 or newer."
    echo "Consider upgrading: brew upgrade cmake"
    echo
fi

# Create user project file if it doesn't exist
USER_PROJECT_FILE="$PROJECT_ROOT/src/user_project.hpp"
TEMPLATE_FILE="$PROJECT_ROOT/cmake/user_project.hpp.in"
if [ ! -f "$USER_PROJECT_FILE" ]; then
    echo "Copying user project template..."
    cp "$TEMPLATE_FILE" "$USER_PROJECT_FILE"
    echo "✓ Created user_project.hpp from template"
else
    echo "src/user_project.hpp already exists, skipping creation"
fi

echo
echo "✅ System setup complete!"
echo
echo "System Summary:"
echo "  ✓ macOS $MACOS_VERSION (C++23 compatible)"
echo "  ✓ System Apple Clang (no Homebrew LLVM needed)"
echo "  ✓ All dependencies installed via Homebrew"
echo
echo "Next steps:"
echo "1. Run './scripts/setup_xcode.sh' to generate Xcode project"
echo "2. Or configure your preferred build environment"
echo
echo "Dependencies installed:"
echo "  ✓ CMake, RtAudio, FFmpeg, GoogleTest, GLFW"
echo "  ✓ Eigen, OneDPL, Magic Enum, fmt"
echo
