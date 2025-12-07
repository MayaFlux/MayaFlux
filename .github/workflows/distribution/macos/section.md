## 🍎 macOS Specific

<details>
<summary>Click to expand</summary>

### System Requirements

- **ARM64 (Apple Silicon)**: macOS 14 (Sonoma) or later
- **x86_64 (Intel)**: macOS 15 (Sequoia) or later
- **Dependencies**: Homebrew (for runtime dependencies)

### Installation & Dependencies

```bash
brew install ffmpeg rtaudio glfw glm eigen fmt magic_enum onedpl googletest \
        vulkan-headers vulkan-loader vulkan-tools vulkan-validationlayers vulkan-utility-libraries \
        spirv-tools spirv-cross shaderc glslang molten-vk
```

### Technical Details

- **Build**: System Clang with C++23 support
- **Architectures**:
  - ARM64 (Apple Silicon optimized)
  - x86_64 (Intel)
- **LLVM**: Homebrew LLVM for JIT compilation
- **Vulkan**: MoltenVK (Vulkan over Metal) for graphics
- **Audio**: RtAudio with CoreAudio backend

### Distribution Contents

```
MayaFlux-{{VERSION}}-macos-arm64/  (or macos-x64/)
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

</details>
