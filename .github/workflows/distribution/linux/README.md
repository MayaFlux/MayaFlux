# MayaFlux Distribution - Linux

Complete MayaFlux platform distribution for Linux (x86_64, Fedora 43).

## Components

### 1. MayaFluxLib - Core Graphics/Compute Engine

- **Headers**: `include/MayaFlux/` - Full C++23 API
- **Library**: `lib/libMayaFluxLib.so` - Core engine shared library

### 2. Lila - JIT Compilation Interface

- **Headers**: `include/Lila/` - JIT compilation API
- **Library**: `lib/libLila.so` - JIT engine shared library

### 3. lila_server - User Application

- **Executable**: `bin/lila_server` - Ready-to-run server

## Runtime Dependencies (DNF)

This distribution is built on **Fedora 43** with modern toolchain (GCC 15, LLVM 21). Required packages:

```bash
dnf install -y \
    @development-tools \
    llvm llvm-devel llvm-libs \
    clang clang-devel \
    cmake \
    pkgconfig \
    rtaudio-devel \
    glfw-devel \
    glm-devel \
    eigen3-devel \
    spirv-headers-devel \
    spirv-tools \
    vulkan-headers \
    vulkan-loader \
    vulkan-loader-devel \
    vulkan-tools \
    vulkan-validation-layers \
    ffmpeg-free-devel \
    stb-devel \
    magic_enum-devel \
    tbb-devel \
    glslc \
    libshaderc-devel \
    wayland-devel
```

## System Requirements

- **Distribution**: Fedora 43+, RHEL 10+, or compatible
- **Architecture**: x86_64
- **Toolchain**: GCC 15+ or Clang 18+ with C++23 support

### Compatibility Notes

This distribution targets **modern Linux with Fedora 43 ABI**:

- ✅ **Fedora 43+**: Direct binary compatibility
- ✅ **RHEL 10+**: Direct binary compatibility
- ✅ **Ubuntu 25.04+**: Compatible when released
- ✅ **Arch linux**: Use AUR package `mayaflux-dev-bin`
- ⚠️ **Other distros**: Build from source

The binaries are linked against:

- GCC 15 libstdc++
- LLVM 21 libraries
- GLFW 3.4 (Wayland support)
- FFmpeg 6.1 (modern codecs)

If your distribution has older versions, you must **build MayaFlux from source** to match your system's ABI.

## Quick Start

Use Weave to install dependenceis automatically.
Weave will also download and install this distribution for you.

### For End Users (Immediate Use):

```bash
# Extract distribution
tar xf MayaFlux-*-linux-fedora43-x64.tar.xz
cd MayaFlux-*-linux-fedora43-x64/

# Run interactive development server
./bin/lila_server
```

### For Developers:

```cpp
#include <MayaFlux/MayaFlux.hpp>
#include <Lila/Lila.hpp>

// Link against libMayaFluxLib.so and libLila.so
// g++ -std=c++23 your_code.cpp -lMayaFluxLib -lLila
```

## Installation & Setup

### Automatic Verification:

```bash
# Run verification script
./verify_components.sh
```

## Building from Source (Other Distros)

If your distribution is not compatible, build from source:

```bash
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=23

cmake --build . --parallel $(nproc)
sudo cmake --install .
```

## Troubleshooting

### Library Not Found Errors

```bash
# Check if all dependencies are installed
dnf list installed | grep -E "llvm-libs|rtaudio|glfw|vulkan"

# Check library paths
ldconfig -p | grep -E "MayaFlux|Lila"

# If libraries aren't found, add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/MayaFlux/lib:$LD_LIBRARY_PATH
```

### ABI Compatibility Errors

**Symptom**: `version GLIBCXX_X.X.XX not found` or similar errors

**Solution**: Your distribution has an older GCC/libstdc++. You must build MayaFlux from source on your system to match your distro's ABI.

### Lila JIT Issues

- Verify LLVM is installed: `dnf list installed llvm-libs`
- Check LLVM version: `llvm-config --version` (should be 21+)
- Ensure no conflicting LLVM installations

### GPU/Vulkan Issues

- Update graphics drivers
- Verify Vulkan: `vulkaninfo | grep deviceName`
- Install Vulkan loader: `sudo dnf install vulkan-loader`

### Wayland/X11 Issues

- GLFW 3.4+ supports both Wayland and X11
- Force X11 if needed: `export GDK_BACKEND=x11`
- Check display: `echo $WAYLAND_DISPLAY` or `echo $DISPLAY`

## Technical Details

**Build Environment:**

- Fedora 43 container (Docker)
- GCC 15 with full C++23 support
- LLVM 21 for Lila JIT compilation
- Ninja build system

**Key Features:**

- Native GLFW 3.4 with Wayland support
- FFmpeg 6.1 with modern codec support
- Vulkan graphics and compute
- RtAudio for low-latency audio I/O
