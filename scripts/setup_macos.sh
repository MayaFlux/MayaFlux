#!/usr/bin/env zsh

# Quiet setup for macOS: via Homebrew
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
brew install cmake pkg-config git wget curl llvm >/dev/null

# --- 2) LLVM configuration ---------------------------------------
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

# --- 3) Install All Dependencies ---------------------------------
printf 'Installing dependencies via Homebrew...\n' >&3

brew tap mayaflux/mayaflux >/dev/null

brew install \
    ffmpeg rtaudio glfw glm eigen fmt magic_enum onedpl googletest \
    vulkan-headers vulkan-loader vulkan-tools vulkan-validationlayers \
    vulkan-utility-libraries vulkan-extensionlayer spirv-tools spirv-cross \
    spirv-headers shaderc glslang molten-vk mayaflux/mayaflux/stb >/dev/null

# --- 4) Vulkan Environment (Ported from Ruby Formula) ------------------------
MOLTENVK_PREFIX="$(brew --prefix molten-vk)"
VULKAN_LOADER_PREFIX="$(brew --prefix vulkan-loader)"
VULKAN_LAYERS_PREFIX="$(brew --prefix vulkan-validationlayers)"

append_if_missing "# Vulkan & MoltenVK Environment"

# ICD Selection logic
if [ -f "$MOLTENVK_PREFIX/share/vulkan/icd.d/MoltenVK_icd.json" ]; then
    append_if_missing "export VK_ICD_FILENAMES=\"$MOLTENVK_PREFIX/share/vulkan/icd.d/MoltenVK_icd.json\""
elif [ -f "$MOLTENVK_PREFIX/etc/vulkan/icd.d/MoltenVK_icd.json" ]; then
    append_if_missing "export VK_ICD_FILENAMES=\"$MOLTENVK_PREFIX/etc/vulkan/icd.d/MoltenVK_icd.json\""
fi

# Library Paths
append_if_missing "export DYLD_LIBRARY_PATH=\"$MOLTENVK_PREFIX/lib:\$DYLD_LIBRARY_PATH\""
append_if_missing "export DYLD_LIBRARY_PATH=\"$VULKAN_LOADER_PREFIX/lib:\$DYLD_LIBRARY_PATH\""

# Explicit Layers
append_if_missing "export VK_LAYER_PATH=\"$VULKAN_LAYERS_PREFIX/share/vulkan/explicit_layer.d\""

# --- 5) STB Setup (header-only library) --------------------------------------
STB_PATH="$(brew --prefix stb)/include/stb"

append_if_missing "# STB Header-only Library"
append_if_missing "export STB_ROOT=\"$STB_PATH\""
append_if_missing "export CPATH=\"\$STB_ROOT:\$CPATH\""

# --- 6) Finish ---------------------------------------------------------------
exec 1>&3 3>&-
printf '✅ LLVM installed at %s\n' "$LLVM_PREFIX"
printf '✅ Vulkan SDK %s installed to %s\n' "$SDK_VERSION" "$DEST"
printf '✅ Environment updated in "$PROFILE"\n'
printf 'Run: source "$PROFILE" or restart your shell.\n'
