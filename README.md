# MayaFlux

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)]()

> **A unified multimedia processing architecture treating audio, video, and algorithmic composition as interchangeable computational material.**

MayaFlux is not a plugin framework, DAW, or audio library. It's the **computational substrate** that makes new kinds of real-time multimedia systems possible—by asking a fundamental question: _What if audio, visuals, and control data shared the same processing paradigm instead of separate, disconnected domains?_

---

## The Paradigm

Existing tools force a choice: real-time audio precision **or** flexible visual programming—never both with unified timing.

**MayaFlux eliminates this false choice.**

### What Becomes Possible

- **Direct cross-modal data flow**: Audio features flow to GPU compute shaders without translation layers or callback hell
- **Live algorithmic authorship**: Modify audio and visual algorithms while they execute, sub-buffer latency (~1ms)
- **Recursive composition**: Treat time as creative material via C++20 coroutines—enabling temporal structures impossible in traditional DSP
- **Sample-accurate coordination**: Audio processing at sample rate, graphics at frame rate, both within unified scheduling
- **Adaptive pipelines**: Algorithms self-configure based on data characteristics at runtime via grammar-driven matching

This isn't incremental improvement on existing paradigms. It's a different computational substrate.

---

## Architecture

Four composable paradigms form the foundation:

| Component               | What It Does                                                                                  |
| ----------------------- | --------------------------------------------------------------------------------------------- |
| **Nodes**               | Unit-by-unit transformation precision; mathematical relationships become creative decisions   |
| **Buffers**             | Temporal gathering spaces accumulating data without blocking or unnecessary allocation        |
| **Coroutines**          | C++20 primitives treating time itself as malleable, compositional material                    |
| **Containers (NDData)** | Multi-dimensional data structures unifying audio, video, spectral, and tensor representations |
| **Compute Matrix**      | Composable and expresssive semantic pipelines to analyze, sort, extract and transform NDData  |

All components remain **composable and concurrent**. Processing domains are encoded via bit-field tokens, enabling type-safe cross-modal coordination.

**Result**: A system where audio, graphics, and algorithmic composition operate on the same underlying principles—not bolted together, but architecturally unified.

---

## Current Implementation Status

### ✓ Production-Ready

- Lock-free audio node graphs (sample-accurate, 700+ tests)
- Comprehensive audio backend (RtAudio, sample-accurate I/O)
- C++20 coroutine scheduling infrastructure
- **Lila JIT compiler**: Live C++ execution with sub-buffer latency (LLVM21-backed)
- Region-based memory management and NDData containers
- **100,000+ lines of tested, documented core infrastructure**

### ✓ Proof-of-Concept (Validated)

- Vulkan graphics pipeline (CPU → GPU unified data flow)
- Cross-domain node synchronization
- GPU rendering from NDData containers
- Demonstrates architecture scales across modalities

### → In Active Development

- GPU compute shader integration
- Complex visual effect pipelines
- Full audio-visual feedback loops
- Advanced coroutine coordination patterns

---

## Quick Start

### Requirements

- **Compiler**: C++20 compatible (GCC 12+, Clang 16+, MSVC 2022+)
- **Build System**: CMake 3.20+
- **Core Dependencies**: RtAudio, GLFW
- **Optional**: LLVM 21+ (for Lila live coding), FFmpeg (media I/O), Vulkan SDK (graphics)

### Build

```sh
# Clone repository
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux

# Run platform-specific setup
./scripts/setup_macos.sh       # macOS
./scripts/setup_linux.sh       # Linux
./scripts/setup_windows.ps1    # Windows

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

For detailed setup, see [Getting Started](docs/Getting_Started.md).

---

## Using MayaFlux

### Basic Audio Processing

```cpp
#include "MayaFlux/MayaFlux.hpp"

int main() {
    MayaFlux::Init();

    // Create nodes
    auto sine = vega.Sine(200)[{0, 1}] | Audio;
    uint32_t input_samples = 30;
    uint32_t output_samples = 8;
    auto filter = vega.IIR(30, 8);
    sine >> filter;

    auto impulse = vega.Impulse(1)[0] | Graphics;
    float freq = 200.f;
    impulse->on_impulse([&freq, sine](auto& ctx){
        freq+= ctx.value * 10;
        sine->set_frequence(freq);
    })


    // Start processing
    MayaFlux::Start();

    // ... your application

    MayaFlux::End();
    return 0;
}
```

### Live Code Modification (Lila)

```cpp
// Start live coding session
Lila::Lila live_interpreter;
live_interpreter.initialize();

// Execute C++ code and modify running graph
live_interpreter.eval(R"(
    auto math = vega.Polynomial([](double x){return x*x*x;});
    auto node_buffer = vega.NodeBuffer(0, 512, math)[0] | Audio;
    auto pipeline = MayaFlux::create_pipeline()
        ->with_strategy(Kriya::ExecutionStrategy::PHASED);
    pipeline
        >> Kriya::BufferOperation::capture_from(node_buffer)
            .for_cycles(20)
        >> Kriya::BufferOperation::transform([](Kakshya::DataVariant& data, uint32_t cycle) {
            const auto& accumulated = std::get<std::vector<double>>(data);
            accumulated += math->process_sample(get_uniform_random());
            return process_batch(accumulated);
        })
        >> Kriya::BufferOperation::route_to_container(output_stream);
    pipeline->execute_buffer_rate(10);

)");

// Later: modify while running
live_interpreter.eval(R"(
    math->set_input_node(vega.Impulse(1, 0.1f));  // math still exists, modified in real-time
)");
```

For more examples, see [Getting Started](docs/Getting_Started.md) and [Digital Transformation Paradigm](docs/Digital_Transformation_Paradigm.md).

---

## Documentation

- **[Getting Started](docs/Getting_Started.md)** — Setup, basic usage, first program
- **[Digital Transformation Paradigm](docs/Digital_Transformation_Paradigm.md)** — Core architectural philosophy and design rationale
- **[Domain and Control](docs/Domain_and_Control.md)** — Processing tokens, domain composition, cross-modal coordination
- **[Advanced Context Control](docs/Advanced_Context_Control.md)** — Backend customization and specialized architectures

### API Documentation

Auto-generated API docs (updated on every commit):

- **[GitHub Pages](https://mayaflux.github.io/MayaFlux/)**
- **[GitLab Pages](https://mayaflux.gitlab.io/MayaFlux/)**
- **[Codeberg Pages](https://mayaflux.codeberg.page/)**

Build locally:

```sh
doxygen doxyconf
open docs/html/index.html
```

---

## Project Maturity

| Area                     | Status            | Notes                                                          |
| ------------------------ | ----------------- | -------------------------------------------------------------- |
| Core DSP Architecture    | ✓ Stable          | 700+ tests, production use ready                               |
| Audio Backend            | ✓ Stable          | Sample-accurate I/O via RtAudio                                |
| Live Coding (Lila)       | ✓ Functional      | Sub-buffer JIT compilation working                             |
| Node Graphs              | ✓ Mature          | Lock-free, concurrent, well-tested                             |
| Graphics (Vulkan)        | ⚙ POC            | Architecture validated, compute shader integration in progress |
| Grammar-Driven Pipelines | ⚙ In Development | Core framework ready, advanced matching in progress            |

**Development began**: ~7 months ago (April 2025)  
**Current version**: 0.1.0 (alpha)  
**Trajectory**: Stable core, expanding multimedia capabilities

---

## Philosophy

MayaFlux represents a fundamental shift from **analog-inspired design** toward **digital-native paradigms**.

Traditional tools ask: "How do we simulate vintage hardware in software?"  
**MayaFlux asks: "What becomes possible when we embrace purely digital computation?"**

Answers include:

- Recursive signal processing (impossible in analog)
- Real-time code modification with deterministic behavior
- Grammar-driven adaptive pipelines (data shapes processing)
- Unified cross-modal scheduling (not separate clock domains)
- Time as compositional material (not just a timeline)

This requires rethinking how audio, visuals, and interaction relate—not as separate tools, but as **unified computational phenomena**.

---

## For Researchers & Developers

If you're investigating:

- Real-time DSP without sacrificing flexibility
- Why coroutines enable new compositional paradigms
- GPU and CPU as unified processing substrates
- Digital paradigms for multimedia computation
- Live algorithmic authorship at production scale

...this is the reference implementation.

**Everything is open source. Contributions, fork it, build on it, challenge it.**

---

## Roadmap (Provisional)

### Phase 1 (Now)

- ✓ Validate core audio architecture
- ✓ Implement live coding (Lila)
- → Public launch (November 2025)

### Phase 2 (Q1 2026)

- GPU compute shader integration
- Advanced visual effect pipelines
- Academic research publications

### Phase 3 (Q2-Q3 2026)

- Hardware controllers
- Network streaming (distributed processing)
- Advanced scheduling patterns

### Phase 4+ (TBD)

- OpenCV integration
- WASM targets (web deployment)
- Live hardware controllers with coroutines

---

## License

**GNU General Public License v3.0 (GPLv3)**

See [LICENSE](LICENSE) for full terms.

---

## Contributing

MayaFlux welcomes collaboration. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

All contributors must follow [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

---

## Authorship & Ethics

For authorship, project ownership, and ethical positioning, see [ETHICAL_DECLARATIONS.md](ETHICAL_DECLARATIONS.md).

---

## Contact

**Research Collaboration**: Interested in joint research or academic partnerships  
**Alpha Testing**: Want early access for production evaluation  
**Technical Questions**: Architecture, design decisions, integration questions

---

**Made by an independent developer who believes that digital multimedia computation deserves better infrastructure.**

**Status**: Early alpha. Stable core. Expanding possibilities.
