#!/bin/zsh

set -e

echo "MayaFlux macOS Development Environment Installer"
echo "================================================"
echo

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

if [[ "$(uname)" != "Darwin" ]]; then
    print_error "This installer is for macOS only"
    exit 1
fi

MACOS_VERSION=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo $MACOS_VERSION | cut -d. -f1)

if [[ $MACOS_MAJOR -lt 14 ]]; then
    print_error "MayaFlux requires macOS 14.0 (Sonoma) or later"
    print_error "Current version: $MACOS_VERSION"
    exit 1
fi

print_status "Detected macOS $MACOS_VERSION"

ARCH=$(uname -m)
if [[ "$ARCH" == "arm64" ]]; then
    HOMEBREW_PREFIX="/opt/homebrew"
    print_status "Apple Silicon (M1/M2/M3) detected"
else
    HOMEBREW_PREFIX="/usr/local"
    print_status "Intel Mac detected"
fi

install_homebrew() {
    if ! command -v brew &>/dev/null; then
        print_status "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

        if [[ "$ARCH" == "arm64" ]]; then
            eval "$(/opt/homebrew/bin/brew shellenv)"
        else
            eval "$(/usr/local/bin/brew shellenv)"
        fi
    else
        print_status "Homebrew already installed"
    fi
}

install_build_dependencies() {
    print_status "Installing build tools..."

    if ! xcode-select -p &>/dev/null; then
        print_status "Installing Xcode command line tools..."
        xcode-select --install
        until xcode-select -p &>/dev/null; do
            sleep 5
        done
    fi

    brew update

    local tools=(cmake git pkg-config ninja)
    for tool in "${tools[@]}"; do
        if ! brew list --formula | grep -q "^${tool}\$"; then
            print_status "Installing $tool..."
            brew install $tool
        else
            print_status "$tool already installed"
        fi
    done
}

install_llvm() {
    print_status "Installing LLVM for JIT/runtime compilation..."

    local found_llvm=false
    local llvm_versions=("llvm@21" "llvm@20" "llvm")

    for version in "${llvm_versions[@]}"; do
        if brew list --formula | grep -q "^${version}\$"; then
            print_status "Found $version"
            LLVM_PREFIX=$(brew --prefix $version)
            if [[ -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
                found_llvm=true
                break
            fi
        fi
    done

    if ! $found_llvm; then
        if brew info llvm@21 &>/dev/null; then
            print_status "Installing LLVM 21 (recommended for in-place lambdas)..."
            brew install llvm@21
            LLVM_PREFIX=$(brew --prefix llvm@21)
        else
            print_status "Installing latest LLVM..."
            brew install llvm
            LLVM_PREFIX=$(brew --prefix llvm)
        fi
    fi

    if [[ ! -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
        print_error "LLVM installation incomplete - missing CMake configuration"
        exit 1
    fi

    print_success "LLVM installed at $LLVM_PREFIX"
}

install_vulkan_sdk() {
    print_status "Installing Vulkan SDK..."

    if [[ -d "$HOME/VulkanSDK" ]] && find "$HOME/VulkanSDK" -name "vulkan.framework" -type d | read; then
        print_status "Vulkan SDK already installed"
        return 0
    fi

    local vulkan_json=$(curl -fsSL "https://vulkan.lunarg.com/sdk/latest/mac.json")
    local latest_version=$(echo "$vulkan_json" | grep -o '"mac":"[^"]*' | cut -d'"' -f3)

    if [[ -z "$latest_version" ]]; then
        print_error "Failed to fetch latest Vulkan SDK version"
        return 1
    fi

    local download_url="https://sdk.lunarg.com/sdk/download/${latest_version}/mac/vulkan_sdk.dmg"

    print_status "Downloading Vulkan SDK $latest_version..."

    local temp_dmg="/tmp/vulkan-sdk-${latest_version}.dmg"

    if ! curl -L "$download_url" -o "$temp_dmg" --progress-bar; then
        print_error "Failed to download Vulkan SDK"
        return 1
    fi

    print_status "Installing Vulkan SDK..."
    local mount_point=$(hdiutil attach "$temp_dmg" -nobrowse -quiet | awk -F '\t' 'END{print $3}')

    if [[ -z "$mount_point" ]]; then
        print_error "Failed to mount Vulkan SDK DMG"
        rm -f "$temp_dmg"
        return 1
    fi

    local pkg_path=$(find "$mount_point" -name "*.pkg" | head -1)
    if [[ -n "$pkg_path" ]]; then
        sudo installer -pkg "$pkg_path" -target /
        local install_status=$?

        if [[ $install_status -ne 0 ]]; then
            print_error "Vulkan SDK installation failed with status $install_status"
            hdiutil detach "$mount_point" -quiet
            rm -f "$temp_dmg"
            return 1
        fi
    else
        print_error "No installer package found in DMG"
        hdiutil detach "$mount_point" -quiet
        rm -f "$temp_dmg"
        return 1
    fi

    hdiutil detach "$mount_point" -quiet
    rm -f "$temp_dmg"

    local sdk_dir="$HOME/VulkanSDK/$latest_version/macOS"
    if [[ -d "$sdk_dir" && -f "$sdk_dir/lib/libvulkan.1.dylib" ]]; then
        print_success "Vulkan SDK $latest_version installed at $sdk_dir"
    else
        print_warning "Vulkan SDK installed but verification failed - may need manual setup"
    fi
}

install_libraries() {
    print_status "Installing development libraries..."

    local libraries=(
        rtaudio
        ffmpeg
        glfw
        eigen
        onedpl
        magic_enum
        fmt
        googletest
    )

    for lib in "${libraries[@]}"; do
        if ! brew list --formula | grep -q "^${lib}\$"; then
            print_status "Installing $lib..."
            brew install $lib
        else
            print_status "$lib already installed"
        fi
    done
}

setup_environment() {
    print_status "Setting up environment..."

    local env_file="$HOME/.mayaflux_macos_env"

    cat >"$env_file" <<EOF
# MayaFlux Development Environment
# Generated by installer on $(date)

export LLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm"
export CMAKE_PREFIX_PATH="$LLVM_PREFIX:\$CMAKE_PREFIX_PATH"
export PATH="$LLVM_PREFIX/bin:\$PATH"

# Add Homebrew libraries to pkg-config path
export PKG_CONFIG_PATH="$HOMEBREW_PREFIX/lib/pkgconfig:$HOMEBREW_PREFIX/opt/ffmpeg/lib/pkgconfig:\$PKG_CONFIG_PATH"

# Vulkan SDK (if installed)
if [ -d "\$HOME/VulkanSDK" ]; then
    VULKAN_VERSION=\$(ls "\$HOME/VulkanSDK" | sort -V | tail -1)
    if [ -n "\$VULKAN_VERSION" ]; then
        export VULKAN_SDK="\$HOME/VulkanSDK/\$VULKAN_VERSION/macOS"
        export PATH="\$VULKAN_SDK/bin:\$PATH"
        export DYLD_LIBRARY_PATH="\$VULKAN_SDK/lib:\$DYLD_LIBRARY_PATH"
        export VK_ICD_FILENAMES="\$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json"
    fi
fi

# Homebrew paths
export PATH="$HOMEBREW_PREFIX/bin:\$PATH"
export CMAKE_PREFIX_PATH="$HOMEBREW_PREFIX:\$CMAKE_PREFIX_PATH"

echo "MayaFlux environment loaded"
EOF

    local shell_rc=""
    if [[ "$SHELL" == *"zsh" ]]; then
        shell_rc="$HOME/.zshrc"
    else
        shell_rc="$HOME/.bash_profile"
    fi

    if [[ -f "$shell_rc" ]] && ! grep -q "mayaflux_macos_env" "$shell_rc"; then
        echo "source $env_file" >>"$shell_rc"
        print_status "Added environment setup to $shell_rc"
    fi

    source "$env_file"

    print_success "Environment configured"
}

verify_installations() {
    print_status "Verifying installations..."

    local missing=()

    if [[ ! -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
        missing+=("LLVM")
    fi

    local libs=("rtaudio" "ffmpeg" "glfw" "eigen" "onedpl" "fmt")
    for lib in "${libs[@]}"; do
        if ! brew list --formula | grep -q "^${lib}\$"; then
            missing+=("$lib")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_warning "Some components may be missing: ${missing[*]}"
        return 1
    fi

    print_success "All core components verified"
}

main() {
    echo "Starting MayaFlux installation..."
    echo

    install_homebrew
    install_build_dependencies
    install_llvm
    install_vulkan_sdk
    install_libraries
    setup_environment
    verify_installations

    echo
    print_success "MayaFlux development environment installation complete!"
    echo
    echo "Next steps:"
    echo "1. Restart your terminal or run: source ~/.mayaflux_macos_env"
    echo "2. Clone or create your MayaFlux project"
    echo "3. Build using CMake:"
    echo "   mkdir build && cd build"
    echo "   cmake .."
    echo "   make -j\$(sysctl -n hw.ncpu)"
    echo
    echo "Environment variables set in: ~/.mayaflux_macos_env"
    echo "LLVM for JIT: $LLVM_PREFIX"
}

main "$@"
