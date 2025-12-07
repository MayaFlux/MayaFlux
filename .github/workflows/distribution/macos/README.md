# MayaFlux Distribution - macOS

Complete MayaFlux platform distribution for macOS (ARM64, System Clang).

## Components

### 1. MayaFluxLib - Core Graphics/Compute Engine

- **Headers**: `include/MayaFlux/` - Full C++23 API
- **Library**: `lib/libMayaFluxLib.dylib` - Core engine dynamic library

### 2. Lila - JIT Compilation Interface

- **Headers**: `include/Lila/` - JIT compilation API
- **Library**: `lib/libLila.dylib` - JIT engine dynamic library

### 3. lila_server - User Application

- **Executable**: `bin/lila_server` - Ready-to-run server

## Runtime Dependencies (Homebrew)

Required Homebrew packages (automatically managed):

- **LLVM/Clang** - Lila JIT compilation engine
- **FFmpeg** - Media processing (audio/video)
- **GLFW 3.4** - Windowing and input
- **RtAudio** - Audio I/O (CoreAudio backend)
- **Vulkan SDK** - Graphics and compute API
- **Shaderc** - SPIR-V shader compilation
- **GoogleTest** - Testing framework
- **Eigen** - Linear algebra
- **OneDPL** - Parallel STL
- **magic_enum** - Enum reflection
- **fmt** - String formatting
- **vulkan** - Graphics development provided by following packages:
  - vulkan-headers
  - vulkan-loader
  - vulkan-tools
  - vulkan-validationlayers
  - vulkan-utility-libraries
- **Shader interaction** - Compilation and introspection provided by:
  - spirv-tools
  - spirv-cross
  - shaderc
  - glslang
- **molten-vk** - Macos Vulkan layer on Metal

## Architecture Support

### Pre-Built Binaries

This distribution includes binaries for:

- **ARM64** (Apple Silicon: M1, M2, M3, M4, M5) - macOS 14+
- **x86_64** (Intel) - macOS 15+

### System Requirements by Architecture

- **ARM64**: macOS 14 (Sonoma) or later
- **Intel**: macOS 15 (Sequoia) or later

### Building from Source

For older Intel Macs (pre-macOS 15), MayaFlux can be built from source following the [developer build instructions](../Getting_Started.md#building-from-source).

### Why These Minimums?

1. **ARM64 (macOS 14+)**: Primary development target
2. **Intel (macOS 15+)**: CI/CD uses GitHub's `macos-15` runners for Intel builds
3. **Older Intel Macs**: Source builds supported, but no pre-built binaries provided

**If you need binaries for older Intel systems**, either:

- Build from source (fully supported, well-documented)
- Open an issue requesting specific version support

## Quick Start

### For End Users:

```bash
# Run interactive development server
./bin/lila_server
```

### For Developers:

```cpp
#include <MayaFlux/MayaFlux.hpp>
#include <Lila/Lila.hpp>

// Link against libMayaFluxLib.dylib and libLila.dylib
```

## System Requirements

- **OS**: macOS 14+ (Sonoma) or later
- **Architecture**: ARM64 (Apple Silicon)
- **Dependencies**: Homebrew (for runtime dependencies)

## Installation & Setup

### Automatic Setup (Recommended):

```bash
# Run the setup script included in distribution
./setup_macos_dependencies.sh
```

### Manual Setup:

```bash
# Install required Homebrew packages
brew install rtaudio ffmpeg shaderc googletest pkg-config cmake eigen onedpl magic_enum fmt glfw glm llvm vulkan-sdk
```

## Troubleshooting

### Library Not Found Errors

- Ensure Homebrew is installed and up to date
- Run `brew doctor` to check for issues
- Verify all required packages are installed

### Lila JIT Issues

- Verify LLVM is installed via Homebrew: `brew install llvm`
- Check that `llvm-config` is in PATH
- Ensure no conflicting LLVM installations

### GPU/Vulkan Issues

- Update macOS to latest version
- Verify Metal support (Apple Silicon has full Metal support)
- Check Vulkan SDK installation via Homebrew
