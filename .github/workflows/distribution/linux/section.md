## 🐧 Linux Specific

<details>
<summary>Click to expand</summary>

### System Requirements

- **Distribution**: Fedora 43+, RHEL 10+, or compatible
- **Architecture**: x86_64
- **Toolchain**: GCC 15+ with C++23 support

### Installation & Dependencies

- **Package Manager**: DNF (Fedora/RHEL)
- **Required**: Modern system libraries (see README)
- **Optional**: Vulkan SDK for development

### Technical Details

- **Build**: Fedora 43 container with GCC 15
- **Architecture**: x86_64
- **LLVM**: System LLVM 21 for JIT compilation
- **Vulkan**: System Vulkan loader
- **Audio**: RtAudio with ALSA/PulseAudio/PipeWire
- **Graphics**: GLFW 3.4 with native Wayland support

### Distribution Contents

```
MayaFlux-{{VERSION}}-linux-fedora43-x64/
├── bin/              # Executables
│   └── lila_server
├── lib/              # Shared libraries
│   ├── libMayaFluxLib.so
│   └── libLila.so
├── include/          # Headers
│   ├── MayaFlux/
│   └── Lila/
├── share/            # Runtime data
│   └── MayaFlux/runtime/
├── README.md
└── verify_components.sh
```

### Compatibility

**✅ Supported (binary compatibility):**

- Fedora 43+
- RHEL 10+ (when released)
- Ubuntu 25.04+ (when released with compatible toolchain)

**⚠️ Build from source:**

- Older Fedora (<43)
- Ubuntu 24.04 and earlier
- Debian, Arch, openSUSE, etc.

**Why?** This distribution is built against Fedora 43's ABI (GCC 15, LLVM 21, GLFW 3.4, FFmpeg 6.1). Older systems have incompatible library versions.

### Common Linux Issues

**ABI compatibility errors**

```
error: version `GLIBCXX_X.X.XX' not found
error: version `LLVM_21' not found
```

**Solution**: Your distribution has older libraries. Build MayaFlux from source on your system.

**Library not found errors**

```bash
# Verify libraries are found
ldconfig -p | grep -E "MayaFlux|Lila"
```

**Lila JIT compilation failures**

- Verify LLVM: `dnf list installed llvm-libs`
- Check version: `llvm-config --version` (need 21+)
- Ensure no conflicting installations

**GPU/Vulkan initialization errors**

- Update graphics drivers (Mesa, NVIDIA, AMDGPU)
- Verify Vulkan: `vulkaninfo | head -20`
- Install loader: `sudo dnf install vulkan-loader`

**Wayland/X11 display issues**

- GLFW 3.4 supports both natively
- Force X11 if needed: `export GDK_BACKEND=x11`
- Check session: `echo $XDG_SESSION_TYPE`

</details>
