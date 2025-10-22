#!/usr/bin/env zsh

# Quiet setup for macOS: LLVM (via Homebrew) + Vulkan SDK (via LunarG API)
# Only errors printed. Safe for CI and users.
set -euo pipefail

# Silence stdout — only errors go to stderr
exec 3>&1 1>/dev/null

err() { printf '%s\n' "$*" >&2; }

# --- 1) Homebrew setup --------------------------------------------------------
if ! command -v brew >/dev/null 2>&1; then
    printf 'Installing Homebrew...\n' >&3
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" >/dev/null
fi

brew update >/dev/null
brew install cmake pkg-config git wget curl >/dev/null
brew install llvm >/dev/null

# --- 2) LLVM environment configuration ---------------------------------------
LLVM_PREFIX="$(brew --prefix llvm 2>/dev/null || true)"
if [ -n "$LLVM_PREFIX" ]; then
    if ! grep -Fq "$LLVM_PREFIX/bin" ~/.zprofile 2>/dev/null; then
        {
            echo ''
            echo '# LLVM setup for CMake / llvm-config'
            echo "export PATH=\"$LLVM_PREFIX/bin:\$PATH\""
            echo "export CMAKE_PREFIX_PATH=\"$LLVM_PREFIX/lib/cmake:\$CMAKE_PREFIX_PATH\""
        } >>~/.zprofile
    fi
fi

# --- 3) Vulkan SDK setup (official LunarG API) --------------------------------
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

SDK_VERSION="$(curl -fsSL https://vulkan.lunarg.com/sdk/latest/mac.txt || true)"
if [ -z "$SDK_VERSION" ]; then
    err "Failed to fetch latest Vulkan SDK version for macOS"
    exit 1
fi

DEST="$HOME/VulkanSDK/${SDK_VERSION}"

# Check if Vulkan SDK already exists
if [ -d "$DEST" ]; then
    printf 'Vulkan SDK %s already exists. Skipping download.\n' "$SDK_VERSION" >&3
else
    printf 'Downloading Vulkan SDK %s...\n' "$SDK_VERSION" >&3
    SDK_URL="https://sdk.lunarg.com/sdk/download/${SDK_VERSION}/mac/vulkan_sdk.zip"
    SDK_ZIP="$TMPDIR/vulkan_sdk_${SDK_VERSION}.zip"

    if ! curl -fL "$SDK_URL" -o "$SDK_ZIP"; then
        err "Failed to download Vulkan SDK from $SDK_URL"
        exit 1
    fi

    mkdir -p "$DEST"
    unzip -q "$SDK_ZIP" -d "$TMPDIR"

    SRC_DIR="$(find "$TMPDIR" -type d -name "$SDK_VERSION" | head -n1 || true)"
    if [ -z "$SRC_DIR" ]; then
        err "Could not find extracted SDK folder in $TMPDIR"
        exit 1
    fi

    rm -rf "$DEST"
    mv "$SRC_DIR" "$DEST"
fi

# --- 4) Vulkan environment setup ---------------------------------------------
PROFILE="$HOME/.zprofile"
append_if_missing() {
    grep -Fq "$1" "$PROFILE" 2>/dev/null || printf '%s\n' "$1" >>"$PROFILE"
}

append_if_missing "export VULKAN_SDK=\"$DEST/macOS\""
append_if_missing 'export PATH="$VULKAN_SDK/bin:$PATH"'
append_if_missing 'export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH"'
append_if_missing 'export VK_ICD_FILENAMES="$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json"'
append_if_missing 'export VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layer.d"'

# --- 5) CMake dependencies ------------------------------------------
brew install ffmpeg rtaudio glfw glm eigen fmt magic-enum onedpl >/dev/null || true

# --- 6) Finish ---------------------------------------------------------------
exec 1>&3 3>&-
printf '✅ LLVM installed at %s\n' "$LLVM_PREFIX"
printf '✅ Vulkan SDK %s installed to %s\n' "$SDK_VERSION" "$DEST"
printf '✅ Environment updated in ~/.zprofile\n'
printf 'Run: source ~/.zprofile or restart your shell.\n'
