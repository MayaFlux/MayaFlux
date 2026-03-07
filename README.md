# MayaFlux

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)]()

> **A C++20/23 multimedia framework treating audio, visual, and control data as unified numerical streams across rate-mismatched, independently executing real-time contexts.**

Real-time systems rarely fit a single execution model. Audio runs in hardware callbacks. Graphics render on fixed frame rates. Input arrives asynchronously. User code needs flexible timing. When these contexts must coordinate without blocking, traditional approaches fail: shared state requires mutexes that destroy real-time correctness, domain-specific rewrites multiply complexity, and the boundaries between execution contexts become walls rather than interfaces.

MayaFlux is built on the premise that these execution contexts should be treated as fundamentally separate, then coordinated through lock-free patterns, C++20 coroutines, and compile-time data abstraction. The result is a system where a single computation can exist simultaneously at audio-rate precision, graphics-frame updates, input-driven events, and user-defined timing, without domain-specific rewrites or real-time correctness compromises.

This is not a plugin framework, DAW, or audio library. The infrastructure generalises to any real-time C++ system with mismatched execution models.

For comprehensive onboarding, tutorials, design context, and philosophy, visit [the website](https://mayaflux.org)

---

## The Architecture

Four execution contexts with non-negotiable constraints form the foundation:

- **RtAudio callbacks**: Sample-accurate, zero-allocation, no blocking
- **Vulkan render threads**: Frame-rate execution, GPU command recording, swapchain timing
- **Async input backends**: Event-driven, hardware-interrupt-adjacent, unpredictable arrival
- **User coroutines**: Flexible timing, compositional, must not compromise the above

Coordination across these contexts rests on three technical foundations:

- **Lock-free as foundation**: `atomic_ref`, CAS-based dispatch, zero-mutex registration across thread boundaries. No mutexes anywhere in the real-time path.
- **Coroutines as the weaving layer**: Temporal intent survives thread transitions without central scheduler overhead. Time becomes compositional material rather than a constraint.
- **Compile-time data unification**: Concepts and data views eliminate context-boundary copies that destroy performance. Audio, video, geometry, and control data share processing primitives without translation layers.

---

## Processing Model

Five composable abstractions implement this across all domains:

| Component               | What It Does                                                                                  |
| ----------------------- | --------------------------------------------------------------------------------------------- |
| **Nodes**               | Unit-by-unit transformation precision; mathematical relationships become creative decisions   |
| **Buffers**             | Temporal gathering spaces accumulating data without blocking or unnecessary allocation        |
| **Coroutines**          | C++20 primitives treating time itself as malleable, compositional material                    |
| **Containers (NDData)** | Multi-dimensional data structures unifying audio, video, spectral, and tensor representations |
| **Compute Matrix**      | Composable and expressive semantic pipelines to analyze, sort, extract and transform NDData   |

All abstractions are composable and concurrent. Processing domains are encoded via bit-field tokens enabling type-safe cross-modal coordination. Audio, graphics, and control data operate on the same underlying primitives, not bolted together through adapters, but unified at the architectural level.

---

## In Practice

MayaFlux is not about producing a particular audiovisual result that cannot be replicated elsewhere. Such novelties are neither the point, nor interesting even if true.

It's about making the structure of computation fluid enough that small changes open entirely different creative possibilities.
The ideas, their ontology, and how they compose drive the work. Not mastering the API. Not fighting it.

The two examples below are the same sound engine. The second changes roughly 20 lines. What those 20 lines unlock is not a variation on the first, it's a different compositional universe.

<a href="https://youtu.be/OecKzGCxpRM" target="_blank">
 <img src="https://img.youtube.com/vi/OecKzGCxpRM/maxresdefault.jpg?v=1" alt="Example #1" width="600" />
</a>

<details>
<summary>Example 1: Bouncing Bell</summary>

A modal resonator body with dynamic spatial identity. A sine oscillator provides continuous motion that becomes a structural timing source via zero-crossing detection.
Each crossing excites the resonator, randomises its fundamental, and alternates its stereo position. The same oscillator value drives visual color and spiral growth.
Three visual modes (spiral accumulation, localized burst, distributed field), share the same audio engine and switch at runtime. Mouse position maps directly to strike position and pitch.

```cpp
void bouncing_bell()
{
    // Unified audiovisual playground with multiple representation modes
    auto window = create_window({ .title = "Bouncing Bell", .width = 1920, .height = 1080 });

    // System memory: timing, spatial alternation, visual accumulation, representation mode
    struct State {
        double last_wobble = 0.0; // previous oscillator value for transition detection
        uint32_t side = 0; // stereo side toggle
        float angle = 0.0F; // spiral phase memory
        float radius = 0.0F; // spiral growth memory
        bool explode = false; // reserved structural flag (future extension)
        int visual = 0; // 0=spiral, 1=burst, 2=field
    };
    auto state = std::make_shared<State>();

    // Single resonant body — spatial identity is dynamic
    auto bell = vega.ModalNetwork(12, 220.0, ModalNetwork::Spectrum::INHARMONIC);

    // Slow continuous motion becomes structural timing source
    auto wobble = vega.Sine(0.3, 1.0);

    // Continuous → discrete event (structural bounce)
    auto swing = vega.Logic([state](double x) {
        bool crossed = (state->last_wobble < 0.0) && (x >= 0.0);
        state->last_wobble = x;
        return crossed;
    });
    wobble >> swing;

    // Each bounce excites the body and flips spatial polarity
    swing->on_change_to(true, [bell, state](auto&) {
        bell->excite(get_uniform_random(0.5F, 0.9F));
        bell->set_fundamental(get_uniform_random(200.0F, 800.0F));

        route_network(bell, { state->side }, 0.15);
        state->side = 1 - state->side; // alternate left/right
    });

    // Oscillator also shapes decay — timing signal influences physical response
    bell->map_parameter("decay", wobble, MappingMode::BROADCAST);

    // High-capacity visual field for multiple accumulation strategies
    auto points = vega.PointCollectionNode(2000) | Graphics;
    auto geom = vega.GeometryBuffer(points) | Graphics;
    geom->setup_rendering({ .target_window = window });
    window->show();

    // Representation layer: same sound, multiple visual ontologies
    schedule_metro(0.016, [points, bell, wobble, state]() {
        float energy = bell->get_audio_buffer().has_value()
            ? (float)rms(bell->get_audio_buffer().value()) * 5.0F
            : 0.0F;

        auto wobble_val = (float)wobble->get_last_output(); // continuous state influences color and motion
        float hue = (wobble_val + 1.0F) * 0.5F;
        float x_pos = (state->side == 0) ? -0.5F : 0.5F;

        if (state->visual == 0) {
            // Mode 0: spiral accumulation (memory + growth)
            if (energy > 0.1F) {
                state->angle += 0.5F + wobble_val * 0.3F;
                state->radius += 0.002F;
            } else {
                state->angle += 0.01F;
                state->radius += 0.0001F;
            }

            if (state->radius > 1.0F) {
                state->radius = 0.0F;
                points->clear_points(); // visual cycle reset
            }

            points->add_point({ .position = glm::vec3(
                                    std::cos(state->angle) * state->radius,
                                    std::sin(state->angle) * state->radius * (16.0F / 9.0F),
                                    0.0F),
                .color = glm::vec3(hue, 0.8F, 1.0F - hue),
                .size = 8.0F + energy * 15.0F });

        } else if (state->visual == 1) {
            // Mode 1: localized burst (energy → multiplicity)
            for (int i = 0; i < (int)(energy * 80); i++) {
                auto a = (float)get_uniform_random(0.0F, 6.28F);
                auto r = (float)get_uniform_random(0.01F, 0.05F);

                points->add_point({ .position = glm::vec3(x_pos + std::cos(a) * r, std::sin(a) * r, 0.0F),
                    .color = glm::vec3(energy, 0.5F, 1.0F - energy),
                    .size = (float)get_uniform_random(5.0F, 20.0F) });
            }

            if (points->get_point_count() > 2000) {
                points->clear_points(); // density control
            }

        } else {
            // Mode 2: distributed field (energy → spatial diffusion)
            for (int i = 0; i < (int)(energy * 60); i++) {
                points->add_point({ .position = glm::vec3(
                                        (float)get_uniform_random(-0.8F, 0.8F),
                                        (float)get_uniform_random(-0.8F, 0.8F),
                                        0.0F),
                    .color = glm::vec3(hue, energy, 1.0F - hue),
                    .size = 3.0F + energy * 5.0F });
            }

            if (points->get_point_count() > 2000) {
                points->clear_points(); // prevent runaway accumulation
            }
        }
    });

    // Direct physical interaction: position maps to pitch and intensity
    on_mouse_pressed(window, IO::MouseButtons::Left, [window, bell](double x, double y) {
        glm::vec2 pos = normalize_coords(x, y, window);
        float pitch = 200.0F + ((float)pos.y + 1.0F) * 300.0F;
        float intensity = 0.3F + std::abs((float)pos.x) * 0.6F;
        bell->set_fundamental(pitch);
        bell->excite(intensity);
    });

    // Switch representation modes (same sound engine, different interpretation)
    on_key_pressed(window, IO::Keys::N1, [points, state]() { state->visual = 0; points->clear_points(); });
    on_key_pressed(window, IO::Keys::N2, [points, state]() { state->visual = 1; points->clear_points(); });
    on_key_pressed(window, IO::Keys::N3, [points, state]() { state->visual = 2; points->clear_points(); });

    // Collapse spatial polarity
    on_key_pressed(window, IO::Keys::M, [bell]() { route_network(bell, { 0, 1 }, 0.3); });
}
```

</details>

<details>
<summary>Example 2: Bouncing Bell (extended)</summary>

The resonator gains modal coupling between adjacent modes, a slow pitch drift oscillator mapped directly into the network's frequency parameter and into visual hue simultaneously, and a runtime-swappable rhythm source (slow sine, fast sine, random noise) wired through the same zero-crossing logic node. Mouse now maps to strike position on the resonator body rather than pitch. Space cycles the rhythm source at runtime without stopping anything.

The audio engine is structurally identical. The relationships between signals changed.

```cpp
void bouncing_v2()
{
    // Unified audiovisual playground with multiple representation modes
    auto window = create_window({ .title = "Bouncing Bell", .width = 1920, .height = 1080 });

    // System memory: timing, spatial alternation, visual accumulation, representation mode
    struct State {
        double last_wobble = 0.0; // previous oscillator value for transition detection
        uint32_t side = 0; // stereo side toggle
        float angle = 0.0F; // spiral phase memory
        float radius = 0.0F; // spiral growth memory
        bool explode = false; // reserved structural flag (future extension)
        int visual = 0; // 0=spiral, 1=burst, 2=field
    };
    auto state = std::make_shared<State>();

    // Single resonant body — spatial identity is dynamic
    auto bell = vega.ModalNetwork(12, 220.0, ModalNetwork::Spectrum::INHARMONIC);

    // Slow continuous motion becomes structural timing source
    auto wobble = vega.Sine(0.3, 1.0);

    // Continuous → discrete event (structural bounce)
    auto swing = vega.Logic([state](double x) {
        bool crossed = (state->last_wobble < 0.0) && (x >= 0.0);
        state->last_wobble = x;
        return crossed;
    });
    wobble >> swing;

    // Each bounce excites the body and flips spatial polarity
    swing->on_change_to(true, [bell, state](auto&) {
        bell->excite(get_uniform_random(0.5F, 0.9F));
        bell->set_fundamental(get_uniform_random(200.0F, 800.0F));

        route_network(bell, { state->side }, 0.15);
        state->side = 1 - state->side; // alternate left/right
    });

    // Oscillator also shapes decay — timing signal influences physical response
    bell->map_parameter("decay", wobble, MappingMode::BROADCAST);

    // High-capacity visual field for multiple accumulation strategies
    auto points = vega.PointCollectionNode(2000) | Graphics;
    auto geom = vega.GeometryBuffer(points) | Graphics;
    geom->setup_rendering({ .target_window = window });
    window->show();

    // Representation layer: same sound, multiple visual ontologies
    schedule_metro(0.016, [points, bell, wobble, state]() {
        float energy = bell->get_audio_buffer().has_value()
            ? (float)rms(bell->get_audio_buffer().value()) * 5.0F
            : 0.0F;

        auto wobble_val = (float)wobble->get_last_output(); // continuous state influences color and motion
        float hue = (wobble_val + 1.0F) * 0.5F;
        float x_pos = (state->side == 0) ? -0.5F : 0.5F;

        if (state->visual == 0) {
            // Mode 0: spiral accumulation (memory + growth)
            if (energy > 0.1F) {
                state->angle += 0.5F + wobble_val * 0.3F;
                state->radius += 0.002F;
            } else {
                state->angle += 0.01F;
                state->radius += 0.0001F;
            }

            if (state->radius > 1.0F) {
                state->radius = 0.0F;
                points->clear_points(); // visual cycle reset
            }

            points->add_point({ .position = glm::vec3(
                                    std::cos(state->angle) * state->radius,
                                    std::sin(state->angle) * state->radius * (16.0F / 9.0F),
                                    0.0F),
                .color = glm::vec3(hue, 0.8F, 1.0F - hue),
                .size = 8.0F + energy * 15.0F });

        } else if (state->visual == 1) {
            // Mode 1: localized burst (energy → multiplicity)
            for (int i = 0; i < (int)(energy * 80); i++) {
                auto a = (float)get_uniform_random(0.0F, 6.28F);
                auto r = (float)get_uniform_random(0.01F, 0.05F);

                points->add_point({ .position = glm::vec3(x_pos + std::cos(a) * r, std::sin(a) * r, 0.0F),
                    .color = glm::vec3(energy, 0.5F, 1.0F - energy),
                    .size = (float)get_uniform_random(5.0F, 20.0F) });
            }

            if (points->get_point_count() > 2000) {
                points->clear_points(); // density control
            }

        } else {
            // Mode 2: distributed field (energy → spatial diffusion)
            for (int i = 0; i < (int)(energy * 60); i++) {
                points->add_point({ .position = glm::vec3(
                                        (float)get_uniform_random(-0.8F, 0.8F),
                                        (float)get_uniform_random(-0.8F, 0.8F),
                                        0.0F),
                    .color = glm::vec3(hue, energy, 1.0F - hue),
                    .size = 3.0F + energy * 5.0F });
            }

            if (points->get_point_count() > 2000) {
                points->clear_points(); // prevent runaway accumulation
            }
        }
    });

    // Direct physical interaction: position maps to pitch and intensity
    on_mouse_pressed(window, IO::MouseButtons::Left, [window, bell](double x, double y) {
        glm::vec2 pos = normalize_coords(x, y, window);
        float pitch = 200.0F + ((float)pos.y + 1.0F) * 300.0F;
        float intensity = 0.3F + std::abs((float)pos.x) * 0.6F;
        bell->set_fundamental(pitch);
        bell->excite(intensity);
    });

    // Switch representation modes (same sound engine, different interpretation)
    on_key_pressed(window, IO::Keys::N1, [points, state]() { state->visual = 0; points->clear_points(); });
    on_key_pressed(window, IO::Keys::N2, [points, state]() { state->visual = 1; points->clear_points(); });
    on_key_pressed(window, IO::Keys::N3, [points, state]() { state->visual = 2; points->clear_points(); });

    // Collapse spatial polarity
    on_key_pressed(window, IO::Keys::M, [bell]() { route_network(bell, { 0, 1 }, 0.3); });
}
```

</details>

---

## Current Implementation Status

### Nodes

- Lock-free node graph with atomic compare-exchange processing coordination; networks process exactly once per cycle regardless of how many channels request output
- Signal generators: `Sine`, `Phasor`, `Impulse`, `Polynomial`, `Random` (unified stochastic infrastructure via `Kinesis::Stochastic`, cached distributions, fast uniform RNG)
- Filters: `FIR`, `IIR`
- Networks: `ResonatorNetwork` (IIR biquad bandpass, formant presets, per-resonator or shared excitation), `WaveguideNetwork` (unidirectional and bidirectional propagation, explicit boundary reflections, measurement mode), `ModalNetwork` (exciter system, spatial excitation, modal coupling), `ParticleNetwork`, `PointCloudNetwork`
- Compositing: `CompositeOpNode` (N-ary combination), `ChainNode` (pipeline), `BinaryOpNode` (conduit refactor)
- Graph API: `>>` pipeline operator, `ChainNode` construction, `NodeGraphManager` with injected `NodeConfig` owned by Engine, `MAX_CHANNEL_COUNT` constant
- `NodeSpec` for enhanced node metadata
- Channel routing with block-based crossfade transitions
- HID input via `HIDNode`, MIDI via `MIDINode`
- `NetworkAudioBuffer` and `NetworkAudioProcessor` for node network audio capture
- `StreamReaderNode` and `NodeFeedProcessor` for buffer-to-node streaming
- `Constant` generator node

### Buffers

- `SoundContainerBuffer`, `SoundStreamReader`, `SoundStreamWriter` (breaking renames from 0.1)
- `VideoContainerBuffer`, `NetworkGeometryBuffer`, `CompositeGeometryBuffer`, `CompositeGeometryProcessor`
- `TextureBuffer`, `NodeTextureBuffer`, `NodeBuffer`
- `FeedbackBuffer` (span-backed ring buffer, `HistoryBuffer` migration)
- `TransferProcessor` for bidirectional audio/GPU buffer transfer
- `BufferDownloadProcessor`, `BufferUploadProcessor` (staging)
- `FilterProcessor`, `LogicProcessor`, `PolynomialProcessor`, `NodeBindingsProcessor`
- `DescriptorBindingsProcessor`, `RenderProcessor`
- INTERNAL/EXTERNAL processing modes on binding processors

### IO and Containers

- `IOManager` centralising file reading, buffer wiring, and reader lifetime management for audio, video, camera, and image; replaces scattered load/hook patterns
- `FFmpegDemuxContext`, `AudioStreamContext`, `VideoStreamContext` as RAII FFmpeg resource owners
- `SoundFileContainer`, `VideoFileContainer` (HEVC frame rate detection, decode loop error handling, black frame elimination), `VideoStreamContainer`, `CameraContainer` (single-slot live camera)
- Live camera input: Linux (`/dev/video0`), macOS (numeric device index), Windows (DirectShow filter name via `video=<filter name>`)
- Background decode thread on camera to prevent `av_read_frame` blocking the graphics thread
- Deferred scaler creation via `ensure_scaler_for_format()` to handle `AV_PIX_FMT_NONE` at setup time
- `FrameAccessProcessor`, `SpatialRegionProcessor` (parallel spatial region extraction), `RegionOrganizationProcessor`, `DynamicRegionProcessor`
- `ContiguousAccessProcessor` for audio container access
- `WindowContainer` and `WindowAccessProcessor` for pixel readback from GLFW windows
- `ImageReader` (PNG, JPG, BMP, TGA; auto-converts RGB to RGBA)
- `VideoStreamReader` for stream-based video access

### Graphics

- Vulkan 1.3 dynamic rendering: no `VkRenderPass` objects, no primary window concept, no manual swapchain or framebuffer management
- Multiple simultaneous windows, each an independent rendering target; no viewport partitioning or offset hacks required
- Secondary command buffers for draw commands; `PresentProcessor` orchestrates primary command buffers per window per frame
- `BackendResourceManager` for centralised format traits and swapchain readback
- Geometry nodes: `PointNode`, `PointCollectionNode`, `PathGeneratorNode` (temp points, delayed resolution), `TopologyGeneratorNode` (proximity graphs), `GeometryWriterNode`, `ProceduralTextureNode`, `TextureNode`, `ComputeOutNode`
- Graphics operator pipeline: `PhysicsOperator` (N-body, owns bounds mode and RNG), `PathOperator` (curve-based interpretation), `TopologyOperator` (proximity-based network generation)
- `VertexSampler` for spatial generation extracted from `Kinesis`
- Vertex layouts: static factory constructors, per-buffer layout override, vertex range drawing, `SCALAR_F32` modality
- Mesh shader infrastructure present; paused pending MoltenVK upstream merge
- macOS line rendering fallback via CPU-side quad expansion
- Compute shaders: `ComputePress` dispatch, `ComputeOutNode` for compute output; pipeline stable
- Shader compilation via shaderc or external GLSL to SPIR-V path; SPIRV-Cross for runtime reflection
- Dynamic dispatch loader for Vulkan extension functions

### Coroutines

- `Vruta::TaskScheduler` with multi-domain clock management (`SampleClock`, `FrameClock`); clocks created automatically on first token use
- `Kriya::EventChain` as the primary temporal sequencing primitive: `.then()`, `.repeat()`, `.times()`, `.wait()`, `.every()`; decoupled coroutine from instance
- `Kriya::metro`, `Kriya::schedule`, `Kriya::pattern`, `Kriya::line`
- `SampleDelay`, `FrameDelay`, `MultiRateDelay` awaiters for cross-domain coordination
- `Kriya::Gate`, `Kriya::Trigger` for condition-driven and signal-driven suspension
- `EventFilter` for granular GLFW windowing event filtering
- `EventAwaiter` refactored to use `EventFilter`
- `BufferPipeline` for coroutine-driven buffer operation chains
- `Temporal` proxy DSL replacing `NodeTimeSpec`; `TemporalActivation` replacing `NodeTimer` (breaking)
- `MayaFlux::schedule_metro`, `MayaFlux::create_event_chain` convenience wrappers
- `Await` function for main-thread blocking

### Lila (Live Coding)

- LLVM ORC JIT via embedded Clang interpreter (requires LLVM 21+)
- Evaluates arbitrary C++20 at runtime, one buffer/frame cycle of latency
- TCP server mode for networked live coding sessions
- Symbol lookup and runtime introspection
- Configurable include paths and compile flags
- Windows JIT window responsiveness via main-thread execution

### Input

- `InputSubsystem` coordinating `HIDBackend` (HIDAPI) and `MIDIBackend` (RtMidi)
- `InputManager` for async dispatch to `InputNode` instances; lock-free registration list (platform-specific hazard pointer path on macOS)
- `EventFilter`-based key and mouse event helpers via coroutine input event helpers in `Kriya`
- `GlobalInputConfig` for unified input typing

---

## Quick Start (Projects) — Weave

MayaFlux provides a management tool called `Weave`, currently compatible with Windows and macOS.

### Management Mode

Automates:

- Downloading and installing MayaFlux
- Installing all necessary dependencies for that platform
- Setting up environment variables
- Storing templates for new projects

### Project Creation Mode

Automates:

- Creating new C++ projects
- Setting up CMakeLists.txt with necessary configurations (**do not edit if you are unfamiliar with CMake**)
- Adding necessary MayaFlux includes and linkages
- Setting up editor tools
- Creating a templated `user_project.hpp`

Weave can be found here: [Weave Repository](https://github.com/MayaFlux/Weave)

---

## Quick Start (Developer)

This section is for developers looking to build MayaFlux from source.

### Requirements

- **Compiler**: C++20 compatible (GCC 12+, Clang 16+, MSVC 2022+)
- **Build System**: CMake 3.20+
- **Dependencies**: RtAudio, GLFW, FFmpeg (avcodec, avformat, avutil, swresample, swscale, avdevice),
  GLM, Vulkan SDK, STB, HIDAPI, RtMidi, LLVM 21+ (for Lila live coding), Eigen (linear algebra)
- **Optional**: google-test (unit tests)

### macOS Requirements

| Aspect                   | Requirement      | Notes                                                |
| ------------------------ | ---------------- | ---------------------------------------------------- |
| **OS Version (ARM64)**   | macOS 15+        | Earlier versions lack required C++20 stdlib features |
| **OS Version (Intel)**   | macOS 15+        | Pre-built binaries; older requires source build      |
| **Binary Distributions** | ARM64 and x86_64 | Pre-built binaries available for both architectures  |
| **Building from Source** | ARM64 or x86_64  | Both architectures fully supported                   |

### Build

```sh
# Clone repository
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux

# Run platform-specific setup
./scripts/setup_macos.sh       # macOS
./scripts/setup_linux.sh       # Linux
./scripts/win64/setup_windows.ps1    # Windows (Requires UAC elevation to install dependencies)

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

For detailed setup, see [Getting Started](docs/Getting_Started.md).

---

## Releases and Builds

MayaFlux provides two release channels:

### Stable Releases

Tagged releases (e.g., `v0.1.0`, `v0.2.0`) available on the [Releases page](https://github.com/MayaFlux/MayaFlux/releases). These are tested, documented, and recommended for most users.

### Development Builds

Rolling builds tagged as `x.x.x-dev` (e.g., `0.1.2-dev`, `0.2.0-dev`). These tags are reused: each push overwrites the previous build. Useful for testing recent changes, but may be unstable or incomplete.

| Channel         | Tag Example | Stability | Use Case                           |
| --------------- | ----------- | --------- | ---------------------------------- |
| **Stable**      | `v0.1.2`    | Tested    | Production, learning, projects     |
| **Development** | `0.2.0-dev` | Unstable  | Testing new features, contributing |

Weave defaults to stable releases. Development builds require manual download from the releases page.

The `main` branch tracks head (latest commits). Stable releases are tagged branches only.

---

## Using MayaFlux

### Basic Application Structure

For actual code examples refer to [Getting Started](docs/Getting_Started.md) or any of the tutorials linked within.

```cpp
#include "MayaFlux/MayaFlux.hpp"

int main() {
    // Pre-init config goes here.
    MayaFlux::Init();

    // Post-init setup for starting subsystems goes here.

    MayaFlux::Start();

    // ... your application

    MayaFlux::Await(); // Press [Return] to exit

    MayaFlux::End();
    return 0;
}
```

If not building from source, it is recommended to use the auto-generated `src/user_project.hpp` instead of editing `main.cpp`.

---

### Live Code Modification (Lila)

```cpp
Lila::Lila live_interpreter;
live_interpreter.initialize();

live_interpreter.eval(R"(
    auto math = vega.Polynomial([](double x){ return x*x*x; });
    auto node_buffer = vega.NodeBuffer(0, 512, math)[0] | Audio;
)");

// Modify while running
live_interpreter.eval(R"(
    math->set_input_node(vega.Impulse(1, 0.1f));
)");
```

For contextual overview start with [Digital Transformation Paradigm](docs/Digital_Transformation_Paradigm.md).

---

## Documentation

- **[Getting Started](docs/Getting_Started.md)** — Setup, basic usage, first program
- **[Digital Transformation Paradigm](docs/Digital_Transformation_Paradigm.md)** — Core architectural philosophy and design rationale
- **[Domain and Control](docs/Domain_and_Control.md)** — Processing tokens, domain composition, cross-modal coordination
- **[Advanced Context Control](docs/Advanced_Context_Control.md)** — Backend customization and specialized architectures

### Tutorials

- **[Sculpting Data Part I](docs/Tutorials/SculptingData/SculptingData.md)**: Foundational concepts, data-driven workflow, containers, buffers, processors. Runnable code examples with optional deep-dive expansions. [Getting Started](docs/Getting_Started.md) is a prerequisite.
- **[Processing Expression](docs/Tutorials/SculptingData/ProcessingExpression.md)**: Buffers, processors, math as expression, logic as creative decisions, processing chains and buffer architecture.
- **[Visual Materiality](docs/Tutorials/SculptingData/VisualMateriality.md)**: Graphics basics, geometry nodes, the Vulkan rendering pipeline from a user perspective, multi-window rendering.

### API Documentation

Build locally:

```sh
doxygen doxyconf
open docs/html/index.html
```

Auto-generated docs (enabled once CI pipelines are set up):

- **[GitHub Pages](https://mayaflux.github.io/MayaFlux/)**
- **[GitLab Pages](https://mayaflux.gitlab.io/MayaFlux/)**
- **[Codeberg Pages](https://mayaflux.codeberg.page/)**

---

## Project Maturity

| Area                     | Status      | Notes                                                       |
| ------------------------ | ----------- | ----------------------------------------------------------- |
| Core DSP Architecture    | Stable      | Lock-free, concurrent, sample-accurate                      |
| Audio Backend            | Stable      | RtAudio; JACK, PipeWire, CoreAudio, WASAPI                  |
| Live Coding (Lila)       | Stable      | Sub-buffer JIT compilation via LLVM ORC                     |
| Node Graphs              | Stable      | Generator, filter, network, input, routing, compositing     |
| IO and Containers        | Stable      | Audio, video file, live camera, image; FFmpeg-backed        |
| Graphics (Vulkan)        | Stable      | Dynamic rendering, multi-window, geometry, texture, compute |
| Coroutine Infrastructure | Stable      | Multi-domain scheduling, EventChain, cross-domain awaiters  |
| Input                    | Stable      | HID and MIDI backends, async dispatch                       |
| Kinesis Analysis         | In Progress | Mathematical substrate; analysis tooling planned for 0.3    |
| Grammar-Driven Pipelines | In Progress | Core framework ready, advanced matching in progress         |

**Current version**: 0.2.0-dev
**Trajectory**: Stable core, completing live signal matrix (file/live x audio/video)

---

## Philosophy

MayaFlux represents a fundamental shift from analog-inspired design toward digital-native paradigms.

Traditional tools ask: "How do we simulate vintage hardware in software?"
**MayaFlux asks: "What becomes possible when we embrace purely digital computation?"**

Answers include:

- Recursive signal processing (impossible in analog)
- Real-time code modification with deterministic behavior
- Grammar-driven adaptive pipelines (data shapes processing)
- Unified cross-modal scheduling (not separate clock domains)
- Time as compositional material (not just a timeline)

This requires rethinking how audio, visuals, and interaction relate, not as separate tools, but as unified computational phenomena.

---

## For Researchers and Developers

If you're investigating:

- Real-time DSP without sacrificing flexibility
- Why coroutines enable new compositional paradigms
- GPU and CPU as unified processing substrates
- Digital paradigms for multimedia computation
- Live algorithmic authorship at production scale

...this is the reference implementation.

**Everything is open source. Contribute, fork it, build on it, challenge it.**

---

## Roadmap (Provisional)

### Phase 1 (Complete)

- Core audio architecture
- Live coding via Lila
- Public availability
- Vulkan graphics pipeline

### Phase 2 (Current, Q1 2026)

- Complete live signal matrix (file/live x audio/video)
- Release plumbing, documentation, and example demos
- Cross-domain synchronization
- Video file and live camera input
- HID and MIDI input
- Physical modelling primitives and classes
- Essentially the first public-ready/production-ready release

### Phase 3 (0.3)

- 3D graphics expansion:
  minor pipeline changes, but major opportunities for creative coding, and expanded default classes

### Phase 4 (0.4)

- SIMD optimisation of transformation algorithms in `Kinesis`
- `Kinesis` as a first-class analysis namespace: Eigen-level linear algebra for graphics data, FluCoMa-level audio analysis primitives
- Yantra/Kinesis restructuring: Kinesis becomes pure mathematical substrate, Yantra becomes orchestrator and GPU dispatch coordinator

### Phase 5 (0.5)

- Native UI framework (groundwork already in place via the Region system and WindowContainer infrastructure)

---

## License

**GNU General Public License v3.0 (GPLv3)**

See [LICENSE](LICENSE) for full terms.

---

## Contributing

MayaFlux welcomes collaboration. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

New to MayaFlux? Start with [Starter Tasks](docs/StarterTasks.md) or [Build Operations](docs/BuildOps.md).

All contributors must follow [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

---

## Authorship and Ethics

**Primary Author and Maintainer:**
Ranjith Hegde — independent researcher and developer.

MayaFlux is a project initiated, designed, and developed almost entirely by Ranjith Hegde since March 2025, with discussions and early contributions from collaborators. The project reflects an ongoing investigation into digital-first multimedia computation treating sound, image, and data as a unified numerical continuum rather than separate artistic domains.

For authorship, project ownership, and ethical positioning, see [ETHICAL_DECLARATIONS.md](ETHICAL_DECLARATIONS.md).

---

## Contact

**Research Collaboration**: Interested in joint research or academic partnerships
**Alpha Testing**: Want early access for production evaluation
**Technical Questions**: Architecture, design decisions, integration questions

---

**Made by an independent developer who believes that digital multimedia computation deserves better infrastructure.**

**Status**: Alpha. Stable core. Expanding capabilities.
