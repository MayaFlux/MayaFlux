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
    print_status "Apple Silicon (M1/M2/M3/M4) detected"
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
        print_status "Xcode Command Line Tools not found"
        print_status "Attempting to install..."

        xcode-select --install 2>&1 | grep -v "already installed" || true

        print_status "Waiting for installation to complete..."
        print_status "(If installation dialog appeared, click Install)"

        local timeout=300
        local elapsed=0
        until xcode-select -p &>/dev/null; do
            if [[ $elapsed -ge $timeout ]]; then
                print_error "Xcode tools installation timeout"
                print_error "Please install manually: xcode-select --install"
                exit 1
            fi
            sleep 5
            elapsed=$((elapsed + 5))
        done

        print_success "Xcode Command Line Tools installed"
    else
        print_status "Xcode Command Line Tools already installed"
    fi

    brew update

    local tools=(cmake git pkg-config ninja)
    for tool in "${tools[@]}"; do
        if ! brew list --formula 2>/dev/null | grep -q "^${tool}\$"; then
            print_status "Installing $tool..."
            brew install $tool
        else
            print_status "$tool already installed"
        fi
    done
}

install_llvm_for_jit() {
    print_status "Installing LLVM 20/21 for Lila JIT environment..."
    print_status "NOTE: This is separate from your system compiler"
    print_status "      MayaFlux builds with system clang, but Lila JIT uses this LLVM"

    local found_llvm=false
    local llvm_versions=("llvm@21" "llvm@20")

    for version in "${llvm_versions[@]}"; do
        if brew list --formula 2>/dev/null | grep -q "^${version}\$"; then
            LLVM_PREFIX=$(brew --prefix $version 2>/dev/null)
            if [[ -n "$LLVM_PREFIX" ]] && [[ -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
                print_status "Found $version at $LLVM_PREFIX"
                found_llvm=true
                LLVM_JIT_VERSION=$version
                break
            fi
        fi
    done

    if ! $found_llvm; then
        if brew info llvm@21 &>/dev/null; then
            print_status "Installing LLVM 21 (latest compatible)..."
            brew install llvm@21
            LLVM_PREFIX=$(brew --prefix llvm@21)
            LLVM_JIT_VERSION="llvm@21"
        elif brew info llvm@20 &>/dev/null; then
            print_status "Installing LLVM 20..."
            brew install llvm@20
            LLVM_PREFIX=$(brew --prefix llvm@20)
            LLVM_JIT_VERSION="llvm@20"
        else
            print_error "LLVM 20 or 21 not available in Homebrew"
            print_error "Lila JIT will not be available"
            return 1
        fi
    fi

    if [[ ! -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
        print_error "LLVM installation incomplete - missing CMake configuration"
        exit 1
    fi

    if [[ ! -f "$LLVM_PREFIX/bin/clang++" ]]; then
        print_error "LLVM installation incomplete - missing clang++"
        exit 1
    fi

    print_success "LLVM for JIT installed: $LLVM_JIT_VERSION at $LLVM_PREFIX"
    print_status "Lila will use this LLVM for runtime compilation"
}

install_vulkan_sdk() {
    print_status "Installing Vulkan SDK..."

    if [[ -d "$HOME/VulkanSDK" ]]; then
        local installed_version=$(ls "$HOME/VulkanSDK" 2>/dev/null | sort -V | tail -1)
        if [[ -n "$installed_version" ]] && [[ -d "$HOME/VulkanSDK/$installed_version/macOS" ]]; then
            print_status "Vulkan SDK already installed: $installed_version"
            return 0
        fi
    fi

    local vulkan_json=$(curl -fsSL "https://vulkan.lunarg.com/sdk/latest/mac.json")
    local latest_version=$(echo "$vulkan_json" | grep -o '"mac":"[^"]*' | cut -d'"' -f3)

    if [[ -z "$latest_version" ]]; then
        print_error "Failed to fetch latest Vulkan SDK version"
        print_status "Install manually from: https://vulkan.lunarg.com/sdk/home"
        return 1
    fi

    local download_url="https://sdk.lunarg.com/sdk/download/${latest_version}/mac/vulkan_sdk.dmg"

    print_status "Downloading Vulkan SDK $latest_version..."

    local temp_dmg="/tmp/vulkan-sdk-${latest_version}.dmg"

    if ! curl -L "$download_url" -o "$temp_dmg" --progress-bar; then
        print_error "Failed to download Vulkan SDK"
        rm -f "$temp_dmg"
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
    if [[ -d "$sdk_dir" ]]; then
        print_success "Vulkan SDK $latest_version installed at $sdk_dir"
    else
        print_warning "Vulkan SDK installed but verification failed at expected path"
        print_status "Check ~/VulkanSDK for installation"
    fi
}

install_libraries() {
    print_status "Installing development libraries..."

    local libraries=(
        rtaudio
        ffmpeg
        glfw
        glm
        eigen
        onedpl
        fmt
        googletest
    )

    local failed_libs=()

    for lib in "${libraries[@]}"; do
        if ! brew list --formula 2>/dev/null | grep -q "^${lib}\$"; then
            print_status "Installing $lib..."
            if ! brew install $lib 2>&1 | grep -qv "Warning"; then
                print_warning "Failed to install $lib"
                failed_libs+=("$lib")
            fi
        else
            print_status "$lib already installed"
        fi
    done

    if ! brew list --formula 2>/dev/null | grep -q "^magic_enum\$"; then
        print_status "Installing magic_enum..."
        if ! brew install magic_enum 2>/dev/null; then
            print_warning "magic_enum not in Homebrew - will use FetchContent in CMake"
        fi
    fi

    if [[ ${#failed_libs[@]} -gt 0 ]]; then
        print_warning "Some libraries failed to install: ${failed_libs[*]}"
        print_status "You can retry with: brew install ${failed_libs[*]}"
    fi
}

setup_environment() {
    print_status "Setting up environment..."

    local env_file="$HOME/.mayaflux_macos_env"

    cat >"$env_file" <<EOF
# MayaFlux Development Environment
# Generated by installer on $(date)

# CRITICAL: LLVM for Lila JIT only - NOT for building MayaFlux
# MayaFlux builds with system clang to avoid ABI mismatch
# Lila uses this LLVM at runtime for JIT compilation
export LLVM_JIT_DIR="$LLVM_PREFIX/lib/cmake/llvm"
export LLVM_JIT_ROOT="$LLVM_PREFIX"

# Add LLVM JIT binaries to PATH (for runtime use only)
export PATH="$LLVM_PREFIX/bin:\$PATH"

# Homebrew libraries for pkg-config
export PKG_CONFIG_PATH="$HOMEBREW_PREFIX/lib/pkgconfig:$HOMEBREW_PREFIX/opt/ffmpeg/lib/pkgconfig:\$PKG_CONFIG_PATH"

# Vulkan SDK (if installed)
if [ -d "\$HOME/VulkanSDK" ]; then
    VULKAN_VERSION=\$(ls "\$HOME/VulkanSDK" 2>/dev/null | sort -V | tail -1)
    if [ -n "\$VULKAN_VERSION" ]; then
        export VULKAN_SDK="\$HOME/VulkanSDK/\$VULKAN_VERSION/macOS"
        export PATH="\$VULKAN_SDK/bin:\$PATH"
        export DYLD_LIBRARY_PATH="\$VULKAN_SDK/lib:\$DYLD_LIBRARY_PATH"
        export VK_ICD_FILENAMES="\$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json"
        export VK_LAYER_PATH="\$VULKAN_SDK/share/vulkan/explicit_layer.d"
    fi
fi

# Homebrew paths
export PATH="$HOMEBREW_PREFIX/bin:\$PATH"
export CMAKE_PREFIX_PATH="$HOMEBREW_PREFIX:\$CMAKE_PREFIX_PATH"

# Note: Build MayaFlux with system clang (/usr/bin/clang++)
# Only Lila JIT uses LLVM_JIT_DIR at runtime

echo "✓ MayaFlux environment loaded"
echo "  - System compiler: \$(which clang++)"
echo "  - Lila JIT LLVM: $LLVM_PREFIX"
EOF

    local shell_rc=""
    case "$SHELL" in
    */zsh)
        shell_rc="$HOME/.zshrc"
        ;;
    */bash)
        if [[ -f "$HOME/.bash_profile" ]]; then
            shell_rc="$HOME/.bash_profile"
        else
            shell_rc="$HOME/.bashrc"
        fi
        ;;
    */fish)
        shell_rc="$HOME/.config/fish/config.fish"
        ;;
    *)
        shell_rc="$HOME/.profile"
        ;;
    esac

    if [[ -f "$shell_rc" ]] && ! grep -q "mayaflux_macos_env" "$shell_rc"; then
        echo "" >>"$shell_rc"
        echo "# MayaFlux Development Environment" >>"$shell_rc"
        echo "source $env_file" >>"$shell_rc"
        print_status "Added environment setup to $shell_rc"
    fi

    print_success "Environment configured"
    print_warning "⚠️  Environment NOT active in current shell"
    print_status "To activate, run: source ~/.mayaflux_macos_env"
    print_status "Or restart your terminal"
}

verify_installations() {
    print_status "Verifying installations..."

    local missing=()

    if [[ ! -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
        missing+=("LLVM CMake config")
    fi

    if [[ ! -f "$LLVM_PREFIX/bin/clang++" ]]; then
        missing+=("Clang (required for Lila JIT)")
    fi

    if ! command -v clang++ &>/dev/null; then
        missing+=("System clang++ (Xcode)")
    fi

    local libs=("rtaudio" "ffmpeg" "glfw3" "glm")
    for lib in "${libs[@]}"; do
        if ! pkg-config --exists $lib 2>/dev/null; then
            missing+=("$lib pkg-config")
        fi
    done

    local brew_libs=("eigen" "onedpl" "fmt" "googletest")
    for lib in "${brew_libs[@]}"; do
        if ! brew list --formula 2>/dev/null | grep -q "^${lib}\$"; then
            missing+=("$lib")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_warning "Some components may be missing: ${missing[*]}"
        return 1
    fi

    print_success "All core components verified"
}

print_summary() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    print_success "MayaFlux development environment ready!"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Configuration:"
    echo "  ✓ System compiler:   $(clang++ --version | head -1)"
    echo "  ✓ Lila JIT LLVM:     $LLVM_JIT_VERSION at $LLVM_PREFIX"
    echo "  ✓ Homebrew prefix:   $HOMEBREW_PREFIX"
    if [[ -d "$HOME/VulkanSDK" ]]; then
        local vk_ver=$(ls "$HOME/VulkanSDK" 2>/dev/null | sort -V | tail -1)
        echo "  ✓ Vulkan SDK:        $vk_ver"
    fi
    echo ""
    echo "IMPORTANT:"
    echo "  • MayaFlux builds with system clang (ABI compatibility)"
    echo "  • Lila uses $LLVM_JIT_VERSION for JIT compilation at runtime"
    echo "  • Do NOT set CC/CXX to Homebrew LLVM for building MayaFlux"
    echo ""
    echo "Next steps:"
    echo "  1. Restart terminal or run:"
    echo "     source ~/.mayaflux_macos_env"
    echo ""
    echo "  2. Clone/navigate to MayaFlux project"
    echo ""
    echo "  3. Build:"
    echo "     mkdir build && cd build"
    echo "     cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "     cmake --build . --parallel \$(sysctl -n hw.ncpu)"
    echo ""
    echo "Environment file: ~/.mayaflux_macos_env"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

main() {
    echo "Starting MayaFlux installation..."
    echo

    install_homebrew
    install_build_dependencies
    install_llvm_for_jit
    install_vulkan_sdk
    install_libraries
    setup_environment
    verify_installations
    print_summary
}

main "$@"
