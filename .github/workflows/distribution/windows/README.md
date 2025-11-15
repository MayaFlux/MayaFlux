# MayaFlux Distribution - Windows

Complete MayaFlux platform distribution for Windows (x64, MSVC 2022).

## Components

### 1. MayaFluxLib - Core Graphics/Compute Engine
- **Headers**: `include/MayaFlux/` - Full C++23 API
- **Library**: `lib/MayaFluxLib.lib` - Import library for linking  
- **Runtime**: `bin/MayaFluxLib.dll` - Core engine DLL

### 2. Lila - JIT Compilation Interface
- **Headers**: `include/Lila/` - JIT compilation API
- **Library**: `lib/Lila.lib` - Import library
- **Runtime**: `bin/Lila.dll` - JIT engine DLL

### 3. lila_server - User Application
- **Executable**: `bin/lila_server.exe` - Ready-to-run server

## Runtime Dependencies (Bundled)

All required DLLs are included in `bin/`:
- **LLVM/Clang 21.1.3** - Lila JIT compilation engine
- **FFmpeg** - Media processing (audio/video)
- **GLFW 3.4** - Windowing and input
- **RtAudio** - Audio I/O (WASAPI backend)
- **Vulkan** - Graphics and compute API

## Quick Start

### For End Users:
```cmd
# Run interactive development server
bin\lila_server.exe
```

### For Developers:
```cpp
#include <MayaFlux/MayaFlux.hpp>
#include <Lila/Lila.hpp>

// Link against MayaFluxLib.lib and Lila.lib
```

## System Requirements

- **OS**: Windows 10/11 (x64)
- **GPU**: Vulkan 1.1+ compatible graphics card
- **Runtime**: Visual C++ Redistributable 2022

## Troubleshooting

### DLL Not Found Errors
- Ensure all DLLs in `bin/` are accessible via PATH
- Place application in same directory as `bin/` folder

### Lila JIT Issues  
- Verify LLVM DLLs are present: `llvm*.dll`, `clang*.dll`
- Check for conflicting LLVM installations

### GPU/Vulkan Issues
- Update graphics drivers to latest version
- Verify Vulkan runtime is installed
