## 📦 MayaFlux Windows Distribution

Automated CI build for **Windows (x64, MSVC 2022)**.

**Latest build for version {{VERSION}}**  
**Commit**: {{COMMIT_SHA}}

### What's Included

✅ **MayaFluxLib** - Core graphics/compute engine with full headers  
✅ **Lila** - JIT interface with LLVM 21.1.3 compilation engine  
✅ **lila_server.exe** - Ready-to-run interactive development environment  
✅ **All Runtime DLLs** - LLVM, Clang, FFmpeg, GLFW, RtAudio, Vulkan bundled

### Key Features

- **Standalone**: All dependencies bundled (no additional downloads needed)
- **JIT Compilation**: Live C++23 code execution via Lila
- **GPU Acceleration**: Vulkan backend with compute shader support
- **Audio I/O**: RtAudio (WASAPI) for Windows audio
- **Media Support**: FFmpeg for audio/video processing

### System Requirements

- **OS**: Windows 10/11 (x64)
- **GPU**: Any GPU with Vulkan 1.1+ support
- **Runtime**: Visual C++ Redistributable 2022 (usually pre-installed)

### Quick Start

1. Extract ZIP to desired location
2. Run `verify.bat` to check all components
3. Execute `bin\lila_server.exe` for interactive environment
4. Or link against MayaFluxLib in your project

### For Developers

Link against:
- **Headers**: `include/MayaFlux/` and `include/Lila/`
- **Libraries**: `lib/MayaFluxLib.lib` and `lib/Lila.lib`
- **Runtime**: Ensure DLLs from `bin/` are on PATH

### Technical Details

- **Build**: MSVC 2022 (cl.exe)
- **C++ Standard**: C++23 with LTO optimization
- **Architecture**: x64 (Intel/AMD)
- **LLVM Version**: 21.1.3 (critical for Lila JIT)
- **Vulkan**: Auto-detected from system install

### Distribution Contents

MayaFlux-{{VERSION}}-windows-2022-{{COMMIT_SHA}}/
├── bin/ # Executables & runtime DLLs
│ ├── lila_server.exe
│ ├── MayaFluxLib.dll
│ ├── Lila.dll
│ ├── llvm.dll # Lila JIT engine
│ ├── clang.dll
│ ├── ffmpeg*.dll # Media processing
│ ├── glfw3.dll # Windowing
│ ├── rtaudio.dll # Audio I/O
│ └── vulkan*.dll # Graphics
├── lib/ # Import libraries for linking
│ ├── MayaFluxLib.lib
│ └── Lila.lib
├── include/ # Headers
│ ├── MayaFlux/
│ └── Lila/
├── share/ # Runtime data
│ └── lila/runtime/
├── README.md
└── verify.bat # Component verification script

### Troubleshooting

**"DLL Not Found" errors**
- Verify all DLLs in `bin/` exist
- Add `bin/` to system PATH or place application there
- Check for conflicting library installations

**Lila JIT compilation failures**
- Ensure LLVM/Clang DLLs are present in `bin/`
- Verify no other LLVM installations conflict
- Check Windows Event Viewer for detailed errors

**GPU initialization errors**
- Update GPU drivers (NVIDIA, AMD, Intel)
- Verify Vulkan runtime installed (usually automatic)
- Run `vulkaninfo.exe` to diagnose Vulkan setup

---

*This is an automated CI build. Previous builds for this version are replaced.*  
*For source code and development, see: https://github.com/{{REPOSITORY}}*
