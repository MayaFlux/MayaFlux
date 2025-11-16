## 🪟 Windows Specific

### System Requirements
- **OS**: Windows 10/11 64-bit
- **Architecture**: x86_64
- **Dependencies**: Visual Studio 2022 Build Tools

### Installation & Dependencies
- **Package Manager**: vcpkg (automatically managed)
- **Required**: Visual Studio 2022 Build Tools
- **Optional**: Vulkan SDK for development

### Technical Details
- **Build**: MSVC 2022 with C++23 support
- **Architecture**: x64
- **LLVM**: Pre-built LLVM 21.1.3 for JIT compilation
- **Vulkan**: Vulkan SDK with Windows drivers
- **Audio**: RtAudio with WASAPI backend

### Distribution Contents
```
MayaFlux-{{VERSION}}-windows-x64/
├── bin/              # Executables & DLLs
│   ├── lila_server.exe
│   └── [runtime DLLs]
├── lib/              # Import libraries
│   ├── MayaFluxLib.lib
│   └── Lila.lib
├── include/          # Headers
│   ├── MayaFlux/
│   └── Lila/
├── share/            # Runtime data
│   └── lila/runtime/
├── README_Windows.md
└── verify.bat
```

### Common Windows Issues
**"DLL not found" errors**
- Ensure all required Visual C++ Redistributables are installed
- Check that all DLLs are present in the `bin/` directory
- Verify PATH includes the directory containing the DLLs

**Lila JIT compilation failures**
- Verify LLVM is properly installed and in PATH
- Check that the DIA SDK is available (should be with Visual Studio)

**GPU initialization errors**
- Update graphics drivers
- Verify Vulkan SDK installation

**Application won't start**
- Check Windows Event Viewer for error details
- Ensure all runtime dependencies are available

