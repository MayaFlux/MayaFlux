# 📦 MayaFlux macOS Distribution

Automated CI build for **macOS 14+ (ARM64, System Clang)**.

**Latest build for version {{VERSION}}**  
**Commit**: {{COMMIT_SHA}}

## What's Included

✅ **MayaFluxLib** - Core graphics/compute engine with full headers  
✅ **Lila** - JIT interface with LLVM compilation engine  
✅ **lila_server** - Ready-to-run interactive development environment  
✅ **Complete Runtime** - All libraries and headers for development

## Key Features

- **Native Performance**: Optimized for Apple Silicon ARM64
- **JIT Compilation**: Live C++23 code execution via Lila
- **GPU Acceleration**: Vulkan backend with compute shader support
- **Audio I/O**: RtAudio (CoreAudio) for macOS audio
- **Media Support**: FFmpeg for audio/video processing
- **Modern C++**: Full C++23 standard support

## System Requirements

- **OS**: macOS 14 (Sonoma) or later
- **Architecture**: ARM64 (Apple Silicon)
- **Dependencies**: Homebrew (for runtime dependencies)

## Quick Start

1. Extract archive to desired location
2. Run `./verify_components.sh` to check all components
3. Install dependencies: `brew install rtaudio ffmpeg shaderc glfw glm llvm vulkan-sdk`
4. Execute `./bin/lila_server` for interactive environment
5. Or link against MayaFluxLib in your project

## For Developers

Link against:
- **Headers**: `include/MayaFlux/` and `include/Lila/`
- **Libraries**: `lib/libMayaFluxLib.dylib` and `lib/libLila.dylib`
- **Frameworks**: Ensure required macOS frameworks are linked

## Required Homebrew Packages

```bash
brew install rtaudio ffmpeg shaderc googletest pkg-config cmake \
             eigen onedpl magic_enum fmt glfw glm llvm vulkan-sdk
```

## Technical Details

- **Build**: System Clang (Apple Clang) with C++23 support
- **Architecture**: ARM64 (Apple Silicon optimized)
- **LLVM**: Homebrew LLVM for JIT compilation
- **Vulkan**: MoltenVK (Vulkan over Metal) for graphics
- **Audio**: RtAudio with CoreAudio backend

## Distribution Contents

MayaFlux-{{VERSION}}-macos-14-{{COMMIT_SHA}}/
├── bin/ # Executables
│ └── lila_server
├── lib/ # Dynamic libraries
│ ├── libMayaFluxLib.dylib
│ └── libLila.dylib
├── include/ # Headers
│ ├── MayaFlux/
│ └── Lila/
├── share/ # Runtime data
│ └── lila/runtime/
├── README.md
└── verify_components.sh # Component verification script

## Troubleshooting

**"Library not found" errors**
- Verify all Homebrew dependencies are installed
- Run `brew doctor` to check for issues
- Ensure DYLD_LIBRARY_PATH includes library locations

**Lila JIT compilation failures**
- Ensure Homebrew LLVM is installed and in PATH
- Verify no conflicting LLVM installations
- Check system integrity with `codesign` verification

**GPU initialization errors**
- Update macOS to latest version
- Verify Metal support: `system_profiler SPDisplaysDataType`
- Check Vulkan SDK installation

**Permission errors**
- Ensure executables have execute permission: `chmod +x bin/lila_server`
- Verify Gatekeeper settings if needed

---
