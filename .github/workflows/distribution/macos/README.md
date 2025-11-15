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
