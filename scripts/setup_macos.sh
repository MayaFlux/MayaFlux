#!/usr/bin/env zsh

# Quiet setup for macOS: LLVM (via Homebrew) + Vulkan SDK (via LunarG API)
# Only errors printed. Safe for CI and users.
set -euo pipefail

# Silence stdout — only errors go to stderr
exec 3>&1 1>/dev/null

err() { printf '%s\n' "$*" >&2; }

PROFILE="${ZDOTDIR:-$HOME}/.zshenv"

append_if_missing() {
    grep -Fq "$1" "$PROFILE" 2>/dev/null || printf '%s\n' "$1" >>"$PROFILE"
}

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
    if ! grep -Fq "$LLVM_PREFIX/bin" "$PROFILE" 2>/dev/null; then
        append_if_missing "# LLVM setup for CMake / llvm-config"
        append_if_missing "export PATH=\"$LLVM_PREFIX/bin:\$PATH\""
        append_if_missing "export CMAKE_PREFIX_PATH=\"$LLVM_PREFIX/lib/cmake:\$CMAKE_PREFIX_PATH\""
        append_if_missing "export LLVM_DIR=\"$LLVM_PREFIX/lib/cmake/llvm\""
        append_if_missing "export Clang_DIR=\"$LLVM_PREFIX/lib/cmake/clang\""
    fi
fi

# --- 3) Dependencies ------------------------------------------
brew install ffmpeg rtaudio glfw glm eigen fmt magic_enum onedpl googletest vulkan-headers vulkan-loader vulkan-tools vulkan-validationlayers vulkan-utility-libraries spirv-tools spirv-cross shaderc glslang molten-vk

# --- 4) Vulkan environment setup ---------------------------------------------

# Homebrew molten-vk installs to specific location
HOMEBREW_PREFIX=$(brew --prefix)
MOLTENVK_PREFIX="$HOMEBREW_PREFIX/opt/molten-vk"

if [ -d "$MOLTENVK_PREFIX" ]; then
    # Check both possible ICD locations
    if [ -f "$MOLTENVK_PREFIX/share/vulkan/icd.d/MoltenVK_icd.json" ]; then
        MOLTENVK_ICD="$MOLTENVK_PREFIX/share/vulkan/icd.d/MoltenVK_icd.json"
    elif [ -f "$MOLTENVK_PREFIX/etc/vulkan/icd.d/MoltenVK_icd.json" ]; then
        MOLTENVK_ICD="$MOLTENVK_PREFIX/etc/vulkan/icd.d/MoltenVK_icd.json"
    fi
fi

if [ -n "$MOLTENVK_ICD" ]; then
    append_if_missing "export VK_ICD_FILENAMES=\"$MOLTENVK_ICD\""
fi

# --- 5) STB Setup (header-only library) --------------------------------------
printf 'Installing STB headers...\n' >&3

STB_INSTALL_DIR="$HOME/Libraries/stb"
STB_HEADER_CHECK="$STB_INSTALL_DIR/stb_image.h"

if [ ! -f "$STB_HEADER_CHECK" ]; then
    mkdir -p "$STB_INSTALL_DIR"

    STB_HEADERS=(
        "stb_image.h"
        "stb_image_write.h"
        "stb_image_resize2.h"
        "stb_truetype.h"
        "stb_rect_pack.h"
    )

    for header in "${STB_HEADERS[@]}"; do
        header_url="https://raw.githubusercontent.com/nothings/stb/master/$header"
        header_path="$STB_INSTALL_DIR/$header"

        if ! curl -fL "$header_url" -o "$header_path" 2>/dev/null; then
            err "Failed to download STB header: $header"
        fi
    done

    printf 'STB headers installed to %s\n' "$STB_INSTALL_DIR" >&3
else
    printf 'STB already installed at %s\n' "$STB_INSTALL_DIR" >&3
fi

append_if_missing 'export STB_ROOT="$HOME/Libraries"'
append_if_missing 'export CMAKE_PREFIX_PATH="$STB_ROOT:$CMAKE_PREFIX_PATH"'
append_if_missing 'export CPATH="$STB_ROOT/:$CPATH"'

# --- 6) Finish ---------------------------------------------------------------
exec 1>&3 3>&-
printf '✅ LLVM installed at %s\n' "$LLVM_PREFIX"
printf '✅ Vulkan SDK %s installed to %s\n' "$SDK_VERSION" "$DEST"
printf '✅ Environment updated in "$PROFILE"\n'
printf 'Run: source "$PROFILE" or restart your shell.\n'
