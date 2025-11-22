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
        "stb"
        "eigen"
        "vulkan-devel"
        "ffmpeg"
        "doxygen"
        "git"
    )
    
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo pacman -Syu --noconfirm "${PACKAGES[@]}"
    
    # magic_enum and eigen are header-only, available via AUR or manual
    echo -e "${YELLOW}Note: magic_enum is header-only library.${NC}"
    echo -e "${YELLOW}Install via: yay -S magic_enum${NC}"
}

install_fedora() {
    echo -e "${BLUE}Installing dependencies for Fedora...${NC}"
    
    PACKAGES=(
        "llvm-devel"
        "llvm-libs"
        "clang"
        "clang-tools-extra"
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
    sudo dnf install -y "${PACKAGES[@]}"
    
    echo -e "${YELLOW}Installing magic_enum and eigen (header-only)...${NC}"
    sudo dnf install -y magic_enum-devel eigen3-devel
}

install_ubuntu() {
    echo -e "${BLUE}Installing dependencies for Ubuntu/Debian...${NC}"
    
    # Update package lists
    sudo apt-get update
    
    PACKAGES=(
        "llvm"
        "llvm-dev"
        "clang"
        "clang-tools"
        "cmake"
        "pkg-config"
        "librtaudio-dev"
        "libglfw3-dev"
        "vulkan-tools"
        "libvulkan-dev"
        "libvulkan1"
        "ffmpeg"
        "libffmpeg-ocaml-dev"
        "doxygen"
        "git"
    )
    
    echo -e "${YELLOW}Installing: ${PACKAGES[*]}${NC}"
    sudo apt-get install -y "${PACKAGES[@]}"
    
    echo -e "${YELLOW}Installing magic_enum and eigen (header-only)...${NC}"
    sudo apt-get install -y libmagic-enum-dev libeigen3-dev
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
