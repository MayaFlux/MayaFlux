#!/bin/sh

# Weave - MayaFlux Linux Setup
# Installs dependencies based on detected package manager
# Supports: Arch (pacman), Fedora (dnf), Ubuntu/Debian (apt), openSUSE (zypper)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

detect_distro() {
    if command -v pacman &> /dev/null; then
        echo "arch"
    elif command -v dnf &> /dev/null; then
        echo "fedora"
    elif command -v apt-get &> /dev/null; then
        echo "ubuntu"
    elif command -v zypper &> /dev/null; then
        echo "opensuse"
    else
        echo "unknown"
    fi
}

install_arch() {
    echo -e "${BLUE}Installing dependencies for Arch Linux...${NC}"
    
    PACKAGES=(
        "llvm"
        "llvm-libs"
        "clang"
        "cmake"
        "pkg-config"
        "rtaudio"
        "glfw"
        "glm"
        "eigen"
        "spirv-headers"
        "spirv-tools"
        "vulkan-headers"
        "vulkan-icd-loader"
        "vulkan-tools"
        "vulkan-utility-libraries"
        "vulkan-validation-layers"
        "ffmpeg"
        "stb"
    )
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo pacman -Syu --noconfirm "${PACKAGES[@]}"
    
    # magic_enum and eigen are header-only, available via AUR or manual
    echo -e "${YELLOW}Note: magic_enum is header-only library.${NC}"
    echo -e "${YELLOW}Install via: yay -S magic_enum${NC}"
    echo -e "${YELLOW}Or via: paru -S magic_enum${NC}"
}

install_fedora() {
    echo -e "${BLUE}Installing dependencies for Fedora...${NC}"
    
    PACKAGES=(
        "gcc-c++" 
        "clang" 
        "llvm" 
        "llvm-devel" 
        "llvm-libs" 
        "clang-devel" 
        "cmake" 
        "ninja-build"
        "pkgconfig"
        "rtaudio-devel"
        "glfw-devel" 
        "glm-devel"
        "eigen3-devel"
        "spirv-headers-devel"
        "spirv-tools"
        "vulkan-headers"
        "vulkan-loader"
        "vulkan-loader-devel"
        "vulkan-tools"
        "vulkan-validation-layers"
        "ffmpeg-free-devel"
        "stb-devel"
        "magic_enum-devel"
        "tbb-devel"
        "gtest-devel"
        "libshaderc-devel"
        "wayland-devel"
        "git"
    )
    
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo dnf install -y "${PACKAGES[@]}"
}

install_ubuntu() {
    echo -e "${BLUE}Installing dependencies for Ubuntu/Debian...${NC}"
    
    # Update package lists
    sudo apt-get update
    
    PACKAGES=(
        "cmake"
        "git"
        "gcc"
        "g++"
        "pkg-config"
        "llvm-21"
        "llvm-21-dev"
        "clang-21"
        "libclang-21-dev"
        "libtbb-dev"
        "vulkan-validationlayers"
        "vulkan-tools"
        "vulkan-utility-libraries-dev"
        "libvulkan-dev"
        "libvulkan1" 
        "wayland-protocols"
        "libglfw3-dev"
        "libglfw3-wayland"
        "glslc"
        "libshaderc-dev"
        "libglm-dev"
        "libeigen3-dev"
        "libmagicenum-dev"
        "libstb-dev"
        "librtaudio-dev"
        "ffmpeg"
        "libavcodec-dev"
        "libavformat-dev"
        "libswscale-dev"
    )
    
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo apt-get install -y "${PACKAGES[@]}"
}

install_opensuse() {
    echo -e "${BLUE}Installing dependencies for openSUSE...${NC}"
    
    PACKAGES=(
        "llvm-devel"
        "clang"
        "cmake"
        "pkg-config"
        "rtaudio-devel"
        "glfw-devel"
        "vulkan-devel"
        "ffmpeg-devel"
        "doxygen"
        "git"
    )
    
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo zypper install -y "${PACKAGES[@]}"
    
    echo -e "${YELLOW}Installing magic_enum and eigen (header-only)...${NC}"
    sudo zypper install -y magic_enum-devel eigen3-devel
}

main() {
    echo -e "${BLUE}Herald - MayaFlux Linux Dependency Installation${NC}"
    echo -e "${BLUE}================================================${NC}\n"
    
    DISTRO=$(detect_distro)
    
    if [ "$DISTRO" = "unknown" ]; then
        echo -e "${RED}Error: Could not detect package manager.${NC}"
        echo -e "${YELLOW}Supported: pacman (Arch), dnf (Fedora), apt (Ubuntu/Debian), zypper (openSUSE)${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Detected: $DISTRO${NC}\n"
    
    case "$DISTRO" in
        arch)
            install_arch
            ;;
        fedora)
            install_fedora
            ;;
        ubuntu)
            install_ubuntu
            ;;
        opensuse)
            install_opensuse
            ;;
    esac
    
    echo -e "\n${GREEN}Installation complete!${NC}"
    echo -e "${BLUE}You can now proceed with: cmake -B build && cmake --build build${NC}"
}

main "$@"
