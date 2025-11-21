## 🍎 macOS Specific

### System Requirements
- **OS**: macOS 14 (Sonoma) or later
- **Architecture**: ARM64 (Apple Silicon)
- **Dependencies**: Homebrew (for runtime dependencies)

### Installation & Dependencies
```bash
brew install rtaudio ffmpeg shaderc googletest pkg-config cmake \
             eigen onedpl magic_enum fmt glfw glm llvm vulkan-sdk
```

### Technical Details
- **Build**: System Clang (Apple Clang) with C++23 support
- **Architecture**: ARM64 (Apple Silicon optimized)
- **LLVM**: Homebrew LLVM for JIT compilation
- **Vulkan**: MoltenVK (Vulkan over Metal) for graphics
- **Audio**: RtAudio with CoreAudio backend

### Distribution Contents
```
MayaFlux-{{VERSION}}-macos-arm64/
├── bin/              # Executables
│   └── lila_server
├── lib/              # Dynamic libraries
│   ├── libMayaFluxLib.dylib
│   └── libLila.dylib
├── include/          # Headers
│   ├── MayaFlux/
│   └── Lila/
├── share/            # Runtime data
│   └── MayaFlux/runtime/
├── README.md
└── verify_components.sh
```

### Common macOS Issues
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

