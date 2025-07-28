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

# Check macOS architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    echo "Detected Apple Silicon (M1/M2) Mac"
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
brew install cmake rtaudio ffmpeg googletest pkg-config eigen onedpl magic_enum
if [ $? -ne 0 ]; then
    echo "Error: Failed to install one or more packages."
    echo "Please check the output above for details."
    exit 1
fi

# Verify package installation
echo "Verifying package installation..."
MISSING_PACKAGES=""
for pkg in cmake rtaudio ffmpeg googletest pkg-config eigen onedpl magic_enum; do
    if ! brew list --formula | grep -q "^${pkg}$"; then
        MISSING_PACKAGES="$MISSING_PACKAGES $pkg"
    fi
done

if [ -n "$MISSING_PACKAGES" ]; then
    echo "Warning: The following packages may not be installed correctly:$MISSING_PACKAGES"
    echo "You may need to install them manually."
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
if [ ! -f "$USER_PROJECT_FILE" ]; then
    echo "Creating user project file..."
    cat >"$USER_PROJECT_FILE" <<'EOF'
#pragma once
#define MAYASIMPLE ;
#include "MayaFlux/MayaFlux.hpp"

void compose() {
    // Your MayaFlux code goes here!
}
EOF
    echo "✓ Created user_project.hpp"
else
    echo "src/user_project.hpp already exists, skipping creation"
fi

echo
echo "System setup complete!"
echo
echo "Next steps:"
echo "1. Run './setup_xcode.sh' to generate Xcode project"
echo "2. Or manually configure your build environment"
echo
echo "Dependencies installed:"
echo "  ✓ CMake, RTAudio, FFmpeg, GoogleTest"
echo "  ✓ Eigen, OneDPL, Magic Enum"
echo
