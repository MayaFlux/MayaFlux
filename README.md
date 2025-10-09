# MayaFlux

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)]()

**Digital-first multimedia processing framework built on modern C++20**

MayaFlux provides unified audio-visual computation through lock-free node graphs, C++20 coroutines for temporal coordination, and grammar-driven operation pipelines. Designed for creative coding, real-time processing, and cross-modal interaction.

---

## Core Features

- **Lock-Free Processing**: Atomic node/buffer coordination without blocking
- **Coroutine Temporal Control**: Sample-accurate and frame-accurate scheduling via C++20 coroutines
- **Grammar-Driven Computation**: Declarative rule-based operation matching and adaptive pipelines
- **Unified Cross-Modal Data**: Audio, video, spectral, and tensor abstractions through NDData
- **Complete Composability**: Every component—nodes, buffers, schedulers, backends—is substitutable

---

## Quick Start

Refer to [Getting Started](docs/Getting_Started.md) for setup information and usage guides.

### Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- RtAudio (audio backend)
- GLFW (windowing)
- FFmpeg (optional, for media file support)

### Build

```sh
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux
./scripts/setup_macos.sh
# or
./scrips/setup_windows.ps1
```

---

## Project Status

**Early-stage architectural research** (~6 months development)

- ✓ Core systems functional (700+ component tests)
- ✓ Audio backend operational (RtAudio)
- ✓ Lock-free node/buffer processing
- ✓ Coroutine scheduling infrastructure
- ⚙ OpenGL graphics pipeline (pending external)
- ⚙ Vulkan graphics pipeline (in development)
- ⚙ Grammar stress testing (ongoing)
- ⚙ Inter-component integration testing (~40% coverage)

This is **not production-ready software**. It's a paradigm exploration seeking validation through community testing.

---

## Documentation

- [Getting Started](docs/Getting_Started.md) - Setup and basic usage
- [Digital Transformation Paradigm](docs/Digital_Transformation_Paradigm.md) - Core architectural philosophy
- [Domain and Control](docs/Domain_and_Control.md) - Processing tokens and domain composition
- [Advanced Context Control](docs/Advanced_Context_Control.md) - Backend customization

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.  
See the [LICENSE](LICENSE) file for full license text.

---

## Authorship & Ethics

For authorship, ownership, and ethical positioning, refer to [ETHICAL_DECLARATIONS](ETHICAL_DECLARATIONS.md).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

All contributors are expected to follow the [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

---

## Contact

For research collaboration, alpha testing, or technical inquiries, see contact information in [ETHICAL_DECLARATIONS](ETHICAL_DECLARATIONS.md).
