# 🚀 MayaFlux Multi-Platform Distribution

**Version**: {{VERSION}}  
**Commit**: {{COMMIT_SHA}}

## 📦 What's Included

✅ **MayaFluxLib** - Core graphics/compute engine with full headers  
✅ **Lila** - JIT interface with LLVM compilation engine  
✅ **lila_server** - Ready-to-run interactive development environment  
✅ **Complete Runtime** - All libraries and headers for development

## 🎯 Key Features

- **Native Performance**: Optimized for target platform architecture
- **JIT Compilation**: Live C++23 code execution via Lila
- **GPU Acceleration**: Vulkan backend with compute shader support
- **Audio I/O**: Platform-native audio (CoreAudio/WASAPI)
- **Media Support**: FFmpeg for audio/video processing
- **Modern C++**: Full C++23 standard support

{{MACOS_SECTION}}

{{WINDOWS_SECTION}}

## 🛠 Quick Start

1. Extract archive to desired location
2. Run verification script: `./verify_components.sh` (macOS/Linux) or `verify.bat` (Windows)
3. Install dependencies (see platform-specific instructions below)
4. Execute `./bin/lila_server` (macOS/Linux) or `bin\lila_server.exe` (Windows) for interactive environment
5. Or link against MayaFluxLib in your project

## 🔧 For Developers

Link against:
- **Headers**: `include/MayaFlux/` and `include/Lila/`
- **Libraries**: Platform-specific dynamic libraries in `lib/` directory
- **Application**: `bin/lila_server` ready-to-run executable

## 🐛 Troubleshooting

See platform-specific README files for detailed troubleshooting:
- `README.md` (macOS)
- `README.md` (Windows)

---
