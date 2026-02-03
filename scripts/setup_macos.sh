#!/usr/bin/env zsh

# MayaFlux Dev Setup
set -euo pipefail

# Silence stdout — only errors go to stderr
exec 3>&1 1>/dev/null

err() { printf '%s\n' "$*" >&2; }

ENV_FILE="${ZDOTDIR:-$HOME}/.mayaflux_env.sh"
PROFILE="${ZDOTDIR:-$HOME}/.zshenv"

# --- 1) Homebrew setup --------------------------------------------------------
if ! command -v brew >/dev/null 2>&1; then
    printf 'Installing Homebrew...\n' >&3
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" >/dev/null
fi

brew update >/dev/null
# --- 3) Install All Dependencies ---------------------------------
printf 'Installing dependencies via Homebrew...\n' >&3

brew tap mayaflux/mayaflux >/dev/null

brew install \
    cmake pkg-config git wget curl llvm \
    ffmpeg rtaudio glfw glm eigen fmt magic_enum onedpl googletest \
    vulkan-headers vulkan-loader vulkan-tools vulkan-validationlayers \
    vulkan-utility-libraries vulkan-extensionlayer spirv-tools spirv-cross \
    spirv-headers shaderc glslang molten-vk hidapi rtmidi mayaflux/mayaflux/stb >/dev/null

# --- 3) Gather Paths ----------------------------------------------------------
LLVM_PREFIX="$(brew --prefix llvm)"
MOLTENVK_PREFIX="$(brew --prefix molten-vk)"
VULKAN_LOADER_PREFIX="$(brew --prefix vulkan-loader)"
VULKAN_LAYERS_PREFIX="$(brew --prefix vulkan-validationlayers)"
STB_ROOT="$(brew --prefix stb)/include/stb"

# ICD Selection
MOLTENVK_ICD="$MOLTENVK_PREFIX/share/vulkan/icd.d/MoltenVK_icd.json"
[[ ! -f "$MOLTENVK_ICD" ]] && MOLTENVK_ICD="$MOLTENVK_PREFIX/etc/vulkan/icd.d/MoltenVK_icd.json"

# --- 4) Generate Single Environment File --------------------------------------
printf 'Generating environment file: %s\n' "$ENV_FILE" >&3

cat <<EOF >"$ENV_FILE"
# MayaFlux Generated Environment - DO NOT EDIT MANUALLY
# Generated on $(date)

# LLVM
export PATH="$LLVM_PREFIX/bin:\$PATH"
export LLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm"
export Clang_DIR="$LLVM_PREFIX/lib/cmake/clang"
export CMAKE_PREFIX_PATH="$LLVM_PREFIX/lib/cmake:\$CMAKE_PREFIX_PATH"

# Vulkan & MoltenVK
export VK_ICD_FILENAMES="$MOLTENVK_ICD"
export VK_LAYER_PATH="$VULKAN_LAYERS_PREFIX/share/vulkan/explicit_layer.d"
export DYLD_LIBRARY_PATH="$MOLTENVK_PREFIX/lib:$VULKAN_LOADER_PREFIX/lib:\$DYLD_LIBRARY_PATH"

# STB
export STB_ROOT="$STB_ROOT"
export CPATH="\$STB_ROOT:\$CPATH"
EOF

# --- 4) Prune and Link to Shell Profile ---------------------------------------
printf 'Cleaning old MayaFlux entries from %s...\n' "$PROFILE" >&3

if [ -f "$PROFILE" ]; then
    # Delete any lines containing MAYAFLUX_ROOT, MAYAFLUX_PREFIX, or sourcing the mayaflux_env/env.sh
    # This handles both the old homebrew style and the previous manual script style
    sed -i '' '/MAYAFLUX_ROOT/d' "$PROFILE"
    sed -i '' '/MAYAFLUX_PREFIX/d' "$PROFILE"
    sed -i '' '/mayaflux_env\.sh/d' "$PROFILE"
    sed -i '' '/mayaflux-dev\/env\.sh/d' "$PROFILE"
    sed -i '' '/mayaflux\/env\.sh/d' "$PROFILE"
fi

printf 'Adding fresh environment link to %s\n' "$PROFILE" >&3
printf '\n[[ -f "%s" ]] && source "%s"\n' "$ENV_FILE" "$ENV_FILE" >>"$PROFILE"

# --- 5) Finish ----------------------------------------------------------------
exec 1>&3 3>&-
printf '✅ Dependencies synced via Homebrew\n'
printf '✅ Profile %s sanitized and updated\n' "$PROFILE"
printf 'Run: source %s\n' "$PROFILE"
