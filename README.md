# MayaFlux

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)]()

> **A C++20/23 framework for real-time computation across sound, geometry, image, and network: unified at the substrate level, not bridged at the API level.**

![Logo](docs/assets/mayaflux_logo_reference.svg)

Sound, geometry, image, and control data are not separate domains in MayaFlux. They are the same numerical substrate with a scheduling annotation deciding where each buffer cycle goes. A field driving mesh vertex deformation is the same field shaping granular reconstruction of audio. A network message routes into the node graph, triggers a coroutine, and reshapes geometry and audio simultaneously. Zero-copy views mean audio is not converted to texture: it is texture, accessed through a different lens over the same memory.

The coordination problem this creates is real: audio runs in hardware callbacks, graphics on frame-rate cycles, input arrives asynchronously, user code needs flexible timing. MayaFlux handles this through lock-free dispatch, C++20 coroutines, and compile-time data abstraction in the real-time path, no domain-specific rewrites at execution context boundaries, no central scheduler overhead.

The closest reference points are early Blender and openFrameworks: independent, flexible infrastructure for computation that produces sound and image. Not a DAW, plugin host, or audio library.

For comprehensive onboarding, tutorials, design context, and philosophy, visit [the website](https://mayaflux.org)

---

## The Architecture

Four execution contexts with non-negotiable constraints form the foundation:

- **Audio callbacks**: Sample-accurate, zero-allocation, no blocking
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

## Current Implementation Status

### Nodes

- Lock-free node graph with atomic compare-exchange processing coordination; networks process exactly once per cycle regardless of how many channels request output
- Signal generators: `Sine`, `Phasor`, `Impulse`, `Polynomial`, `Random` (unified stochastic infrastructure via `Kinesis::Stochastic`, cached distributions, fast uniform RNG), `Constant`, `Counter` (modulo wrap, signed step, normalized output, `on_increment` / `on_wrap` / `on_count` callbacks)
- Filters: `FIR`, `IIR`
- Networks: `ResonatorNetwork` (IIR biquad bandpass, formant presets, per-resonator or shared excitation), `WaveguideNetwork` (unidirectional and bidirectional propagation, explicit boundary reflections, measurement mode, continuous exciter mode), `ModalNetwork` (exciter system, spatial excitation, modal coupling, continuous exciter mode), `ParticleNetwork`, `PointCloudNetwork`, `MeshNetwork` (named slot DAG, parent-child world transform propagation, primary operator and operator chain), `InstanceNetwork` (flat peer slot collection for GPU instanced rendering; single SSBO draw call per frame)
- Mesh operators: `MeshTransformOperator` (per-slot time-driven local transform, world transform propagation), `MeshFieldOperator` (Tendency field deformation per vertex attribute; GPU dispatch path via `ShaderExecutionContext`), `InstanceFieldOperator` (per-slot transform or position fields; GPU dispatch path packing all instance transforms into SSBO via `GpuComputeNode`)
- GpuSync nodes: `GpuComputeNode` (owns a `ShaderExecutionContext`, dispatches async, polls fence, fires `on_complete` callbacks), `GeometryReadbackNode`, `SDFNode`, `GlyphGeometryNode`
- Compositing: `CompositeOpNode` (N-ary combination), `ChainNode` (pipeline), `BinaryOpNode`
- Graph API: `>>` pipeline operator, `NodeGraphManager` with injected `NodeConfig`, `TypedHook<ContextT>` for typed callback dispatch
- `TypedHook<ContextT>` for typed callback dispatch; `NodeHook = TypedHook<>` alias for backward compatibility
- `NodeSpec` for enhanced node metadata
- Channel routing with block-based crossfade transitions
- HID input via `HIDNode`, MIDI via `MIDINode`, OSC via `OSCNode`
- `StreamReaderNode` and `NodeFeedProcessor` for buffer-to-node streaming

### Buffers

- `SoundContainerBuffer`, `VideoContainerBuffer`, `NetworkGeometryBuffer`, `CompositeGeometryBuffer`
- `TextureBuffer`, `TextBuffer` (FreeType glyph texture subclass), `NodeTextureBuffer`, `NodeBuffer`
- `MeshBuffer` (indexed draw, per-slot SSBO transform path, diffuse texture binding), `MeshNetworkBuffer` (per-slot texture array path)
- `InstanceNetworkBuffer`: renders an `InstanceNetwork` as a single instanced draw call; template geometry uploaded once, per-slot transforms packed into SSBO by `InstanceSSBOProcessor` each cycle
- `FeedbackBuffer` (span-backed ring buffer, `HistoryBuffer`)
- `FormaBuffer`: screen-space `VKBuffer` whose contents are rewritten each cycle by a typed geometry function; backing store for Portal::Forma elements
- `ComputeMeshBuffer`: owns SDF input and marching cubes vertex output; driven by `SDFMeshProcessor` via async compute
- `DataWriteProcessor`: modality-aware write processor for `VKBuffer` targets; used by `Bridge` outbound paths and `RenderSink`; replaced `GeometryWriteProcessor` and `TextureWriteProcessor`
- `FormaBindingsProcessor`: descriptor binding updates for Forma elements; writes into buffer pipeline context staging
- `FilterProcessor`, `LogicProcessor`, `PolynomialProcessor`, `NodeBindingsProcessor`
- `DescriptorBindingsProcessor`, `RenderProcessor` (with `instance_count` for instanced draws), `SDFMeshProcessor`, `SDFFieldProcessor`, `SdfPrepProcessor`
- `BufferDownloadProcessor`, `BufferUploadProcessor`, `TransferProcessor`

### IO and Containers

- `IOManager` centralising file reading, write orchestration, and reader lifetime management for audio, video, camera, image, and mesh
- `ModelReader`: Assimp-backed loader for glTF 2.0, FBX, OBJ, PLY, STL, DAE; `TextureResolver` callback for diffuse texture binding
- `FFmpegDemuxContext`, `AudioStreamContext`, `VideoStreamContext` as RAII FFmpeg resource owners
- `SoundFileContainer`, `VideoFileContainer`, `VideoStreamContainer`, `CameraContainer`, `TextureContainer`, `WindowContainer`
- `AudioOutputContainer`: `DynamicSoundStream` subclass wrapping the live engine audio output; each cycle written by `AudioOutputAccessProcessor`; multi-reader model via `CursorAccessProcessor`
- `PlotContainer`: time-series container for N concurrent series; atomic write head; independent reader cursors
- `SoundFileWriter`: encodes audio to any FFmpeg container via worker thread and lock-free queue; accepts interleaved span, planar `DataVariant`, `AudioBuffer`, or `SoundStreamContainer`
- `VideoFileWriter`: encodes video frames via `VideoEncodeContext` and `FFmpegMuxContext` on a worker thread; `IOManager::capture_window` for continuous window-to-file recording
- `FFmpegMuxContext`, `AudioEncodeContext`, `VideoEncodeContext`: RAII FFmpeg write-path owners
- `IOManager::capture_output`: hooks `AudioBackendService` observer to feed live output into a `SoundFileWriter`
- `ImageReader` (PNG, JPG, BMP, TGA; auto-converts RGB to RGBA), `STBImageWriter`, `EXRWriter` (tinyexr)
- Live camera: Linux (`/dev/video0`), macOS (numeric device index), Windows (DirectShow)
- `JSONSerializer` for structured read/write of engine config, Nexus schemas, and other serialization tasks
- `ContiguousAccessProcessor` for audio container access
- `WindowContainer` and `WindowAccessProcessor` for pixel readback from windows
- `ImageReader` (PNG, JPG, BMP, TGA; auto-converts RGB to RGBA)
- `VideoStreamReader` for stream-based video access

### Graphics

- Vulkan 1.3 dynamic rendering: no `VkRenderPass` objects, no primary window concept, no manual swapchain or framebuffer management
- Multiple simultaneous windows, each an independent rendering target
- Secondary command buffers for draw commands; `PresentProcessor` orchestrates primary command buffers per window per frame
- `BackendResourceManager` for centralised format traits and swapchain readback
- Geometry nodes: `PointCollectionNode`, `PathGeneratorNode`, `TopologyGeneratorNode`, `GeometryWriterNode`, `ProceduralTextureNode`, `TextureNode`
- `InstanceNetwork` + `InstanceNetworkBuffer`: single instanced draw call path with SSBO transform packing
- GPU SDF/marching cubes pipeline: `ComputeMeshBuffer`, `SDFMeshProcessor`, `SDFFieldProcessor`, `SdfPrepProcessor`; GPU field shaders with `noise.glsl` and `domain_warp.glsl` includes; `#include` directive support in shaderc via `FileIncluder`
- Async per-window frame readback off the graphics thread; `DisplayService` frame observers; `WindowContainer` holds N rendered frames
- View transform via UBO at `set=0, binding=0`; user descriptors at `set=1+`
- Compute shaders: `ComputePress` dispatch, `ComputeOutNode`, `TextureExecutionContext` for image-in/image-out pipelines
- `TextureLoom` storage image creation and layout transitions; deferred upload via semaphore-signalled pre-frame submission
- Shader compilation via shaderc or external GLSL to SPIR-V; SPIRV-Cross for runtime reflection
- macOS line rendering fallback via CPU-side quad expansion

### Portal::Text

- FreeType-backed glyph atlas with system font discovery (fontconfig on Linux, family/style walk on macOS and Windows)
- `press()`, `repress()`, `impress()` for initial creation, in-place update, and incremental append
- `GlyphOutline` vector decomposition path for geometry-based text
- `TypeFaceFoundry` singleton; `FontFace` and `GlyphAtlas` per pixel size
- `press()` for initial `TextBuffer` creation; `repress()` for in-place update with `RedrawPolicy::Clip` or `RedrawPolicy::Fit`; `impress()` for incremental append with automatic wrapping, budget growth, and overflow recomposite
- `create_layout()` returning `LayoutResult` with `vector<GlyphQuad>`; `GlyphQuad` carries codepoint for per-character identification
- `rasterize_quads()` and `ink_quads()` for caller-driven quad rasterization after arbitrary per-character transforms
- `TextBuffer` subclass of `TextureBuffer`; alpha blend and streaming mode set as defaults; pre-allocated budget dimensions for zero-reallocation incremental updates

### Portal::Forma

- Signal-driven UI surface system with no widget hierarchy; every element is a typed state mapped through a geometry function to a vertex stream
- `FormaBuffer`, `MappedState<T>`, `Mapped<T>`, `Element`, `Layer`, `Context`, `Surface`
- `Bridge`: two-way binding orchestrator. Inbound: `bind(id, Node)`, `bind(id, lambda)`. Outbound: push constant staging, descriptor binding via `FormaBindingsProcessor`, audio via `AudioWriteProcessor`, vertex data via `DataWriteProcessor`, node graph via `Constant`, bulk span sink for coefficient arrays
- `Inspector`: live views into `NodeGraphManager` and `BufferManager` as Forma surfaces; `inspect_node_graph()`, `inspect_buffer_manager()`, `inspect_scheduler()`
- `Plot` subsystem: `PlotSource` builder, `SeriesBuilder` fluent API, `Forma::plot()` automation entry point; adornments (labels, ticks, legend, grid, cursor); `FilledWaveformBuilder`
- `Collapsible` foldable header strip; `LayoutCursor` NDC-space layout accumulator
- Geometry primitives: `horizontal_fader`, `vertical_fader`, `stroke_slider`, `toggle`, `level_meter`, `crosshair`, `drawable_canvas`; `Geometry2D` 2D NDC shape primitives
- Keyboard focus and key event routing on `Context`; `on_drag` with out-of-bounds tracking; keyboard step adjustment for `Mapped` controls
- `wire_canvas_drag` helper for direct canvas interaction

### Portal::System

- Native OS file dialogs: XDG Desktop Portal (Linux, via libdbus-1), `COM IFileOpenDialog`/`IFileSaveDialog` (Windows), `NSOpenPanel`/`NSSavePanel` (macOS via AppKit)
- `Dialog::open_file`, `Dialog::save_file`: callback and blocking templated overloads
- `Depot` convenience layer: `choose_audio`, `choose_video`, `choose_image`, `choose_mesh`, `choose_mesh_network`, `save_audio`, `save_image`

### Coroutines

- Four routine types: `SoundRoutine` (sample clock, audio thread), `GraphicsRoutine` (frame clock, graphics thread), `CrossRoutine` (both clocks, `MULTI_RATE` list, CAS gate for single-thread resume), `FreeRoutine` (condition-driven, `CONDITIONAL` scheduler thread)
- `SampleDelay`, `BufferDelay`, `FrameDelay`, `MultiRateDelay` awaiters; `ConditionAwaiter` for `FreeRoutine`
- `BroadcastSource<T>` and `BroadcastAwaiter<T>`: lock-free single-value broadcast channel bridging callback producers to coroutine consumers
- `Kriya::audio_output_tick()`: `BroadcastSource<bool>` ticking once per audio output cycle via `AudioBackendService` observer
- `Kriya::window_frame_tick(window)`: `BroadcastSource<WindowFrame>` ticking once per captured window frame via `DisplayService` observer
- `Kriya::on_signal`, `on_signal_matching`: coroutine factory patterns for `BroadcastSource`
- `EventChain`: `.then()`, `.repeat()`, `.times()`, `.wait()`, `.every()`; `ProcessingToken` on `Timer`, `TimedAction`, `TemporalActivation`, `EventChain`
- `Kriya::metro`, `Kriya::sequence`, `Kriya::pattern`, `Kriya::line`; all drop scheduler parameter, accept `ProcessingToken`
- `BufferPipeline`, `SamplingPipeline`; `Gate`, `Trigger` drop scheduler parameter
- `key_held` event coroutine; `mouse_dragged` event / `on_mouse_drag` API

### Viewport Navigation

- `bind_fly_preset`: WASD/QE translate, RMB drag yaw/pitch, scroll dolly, KP ortho snaps; remappable via `FlyKeyMap`
- `bind_orbit_preset`: MMB rotate around focal point, Shift+MMB pan, scroll dolly; `OrbitState`, `OrbitKeyMap`
- `bind_pan_zoom_preset`: MMB drag pan, scroll zoom, orthographic projection; `PanZoom2DState`, `PanZoom2DKeyMap`
- `bind_screenspace_preset`: perspective pan in camera's local right/up plane, scroll dolly, no rotation; `ScreenspaceKeyMap`
- `bind_viewport_preset`: convenience wrapper selecting mode via `ViewportPresetMode` enum
- `unbind_viewport_preset`: cancels all registered event handlers, restores window input config
- All viewport files in `Kinesis/Viewport/` subdirectory

### Networking

- `NetworkSubsystem` with UDP and TCP backends via standalone Asio
- Persistent bidirectional endpoints; broadcast datagrams to all matching endpoints
- `NetworkSource` and `NetworkAwaiter` for coroutine-based receive
- `Portal::Network`: `as_osc()` zero-copy OSC parse; `serialize_osc()` for wire serialization
- `OSCNode` for OSC message argument extraction into the node graph

### Nexus

- `Fabric` orchestrator and `Wiring` builder for spatial entity lifecycle
- `Emitter`, `Sensor`, `Agent` entity types; `Locus` (Agent with `NavigationState`); `Presence` (Agent with falloff curve and radius)
- `Expanse`: named spatial region with `ContainsFn`, `on_enter`, `on_exit` crossing callbacks; multi-Fabric registration
- `Tapestry`: world-level collection of Fabrics; owns named Expanses
- `StateEncoder` / `StateDecoder`: EXR+JSON round-trip for Fabric and Tapestry state (schema v5); 5-row RGBA32F EXR layout covering position, color, radius, trigger/sink bits, Locus navigation; `reconstruct()` constructs missing entities from schema; user-state passthrough
- Per-Fabric function registry: auto-registration at `Wiring::finalise()`; `StateDecoder` resolves callable names to live function objects
- `Wiring`: mouse-drag overload; `on(key, held, on_release)` overload; `ProcessingToken` on `every` and `for_duration`
- `RenderSink` and `AudioSink` use `DataWriteProcessor`

### Yantra (Offline Compute)

- `GranularWorkflow`: grammar-driven offline granular pipeline; segment, attribute, sort, reconstruct; `REGION_GROUP`, `CONTAINER`, `CONTAINER_ADDITIVE`, `STREAM`, `STREAM_ADDITIVE` output modes
- `ShaderExecutionContext<>`: templated async compute dispatch; `dispatch_async` returns `FenceID`; `collect_result` after fence signals; used by `GpuComputeNode` and mesh field GPU paths
- `GpuResourceManager`: persistent mapped memory; explicit binding layout
- `GpuTransformer`, `GpuAnalyzer`, `GpuExtractor`, `GpuSorter`; `CHAINED` execution mode
- `ComputationGrammar`, `ComputeMatrix`, `ComputationPipeline`; `Datum<T>` as primary data wrapper

### Lila (Live Coding)

- LLVM ORC JIT via embedded Clang interpreter (LLVM 21+ on Linux; LLVM 22 on Windows/macOS)
- Evaluates arbitrary C++20/23 at runtime including templates, coroutines, and lambdas; one buffer/frame cycle of latency
- TCP server mode for networked live coding sessions; `skip_host_library_load` parameter
- `MayaFluxHost` as independent shared library; `Host::attach_lila` for in-process JIT attachment
- `LiveArena` bump allocator for stable cross-compilation-cycle object sharing; auto-exposed via `Creator` objects
- Runtime API for JIT environment configuration

### Audio Backends

- PipeWire native backend (Linux): `node.description` and process properties, xrun detection, rate-match quantum, `backend_options` passthrough, pause/resume, device name resolution; RT scheduling setup in packaging
- WASAPI native backend (Windows): decoupled engine buffer size from hardware buffer size
- CoreAudio native backend (macOS): separate HAL units for output and input; replaces RtAudio entirely on macOS
- RtAudio removed from Linux and Windows

### MIDI Backends

- PipeWire/ALSA native MIDI backend (Linux): replaces RtMidi on Linux
- WinMM native MIDI backend (Windows): replaces RtMidi on Windows
- CoreMIDI native backend (macOS): replaces RtMidi on macOS; deadlock fix and unified port cleanup
- RtMidi removed from all platforms

### Windowing Backends

- Wayland native backend (Linux): default on Linux; `xkb` key mapping, client-side key repeat via timerfd, scroll and scale corrections; `MAYAFLUX_LINUX_USE_GLFW` option to restore GLFW
- Win32 native backend (Windows): replaces GLFW on Windows; lock-free event queue, client-side key repeat, numpad fix; GLFW retained as opt-in via `GLFW_BACKEND`
- GLFW retained on macOS and for RenderDoc capture path
- `KeyRepeatConfig`: `initial_delay_ms`, `interval_ms`, `allow_compositor_override`

### Input

- `InputSubsystem` coordinating platform-native HID and MIDI backends
- `InputManager` for async dispatch to `InputNode` instances; lock-free registration list
- `key_held` event; mouse drag events; `on_release` overload on `Wiring::on(key)` and `Wiring::on(button)`

### Kinesis

- `Kinesis::Discrete`: Transform, Extract, Spectral, Analysis; `estimate_frequency` from zero-crossing intervals
- `Kinesis::Stochastic`: unified stochastic generation, cached distributions, `VertexSampler`
- `Tendency<D,R>` composable field primitive; vector, spatial, and UV field factories
- `NavigationState` first-person fly; `OrbitState` tumble; `PanZoom2DState` orthographic 2D; all in `Kinesis/Viewport/`
- `Geometry2D`: 2D NDC shape primitives for filled and outline drawing
- `Scalar.hpp`: scalar math utilities
- `generate_grid`, `generate_parametric_surface`, tube extrusion, surface-of-revolution generators
- `AABB3D`, `Morphology.hpp` geometric moment functions; `Bounds.hpp` AABB2D and 2D containment callables
- `generate_sdf_mesh`: marching cubes isosurface extractor; NEON-optimized cube index and normal generation
- `Kinesis/Viewport/` subdirectory for all navigation types

### Build and CI

- `CMakePresets.json` hierarchy; ccache integrated across all build paths with PCH sloppiness
- Arch Linux GitHub Actions workflow; dev-release on nightly schedule
- `MAYAFLUX_BUILD_PROJECT` option; `reconfigure` target and `launch.bat` generation on Windows
- Shared `copy_runtime_dlls` target on Windows; relocatable artifact layout
- magic_enum vendored as header-only third-party target
- dbus-1 added to Linux build, CI, and packaging
- `ide_commands.cmake` for VS solution organisation; shader sources exposed in `compile_shaders` target

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
- **Build System**: CMake 3.28+
- **Dependencies**: FFmpeg (avcodec, avformat, avutil, swresample, swscale, avdevice), GLM, Vulkan SDK, STB, HIDAPI, LLVM 21+ (Linux) / LLVM 22+ (Windows, macOS), Eigen
- **Linux additional**: PipeWire, libdbus-1, wayland-protocols, xkbcommon, fontconfig, tinyexr (vendored)
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
./scripts/setup_macos.sh
./scripts/setup_linux.sh
./scripts/win64/setup_windows.ps1    # requires UAC elevation

# Build
cmake --preset linux-release         # or windows-release / macos-release
cmake --build --preset linux-release
```

For detailed setup, see [Getting Started](docs/Getting_Started.md).

---

## Releases and Builds

MayaFlux provides two release channels:

### Stable Releases

Tagged releases (e.g., `v0.1.0` ... `v0.4.0`) available on the [Releases page](https://github.com/MayaFlux/MayaFlux/releases). These are tested, documented, and recommended for most users.

### Development Builds

Rolling builds tagged as `x.x.x-dev` (e.g., `0.5.0-dev`). These tags are reused: each push overwrites the previous build. Useful for testing recent changes, but may be unstable or incomplete.

| Channel         | Tag Example | Stability | Use Case                           |
| --------------- | ----------- | --------- | ---------------------------------- |
| **Stable**      | `v0.4.0`    | Tested    | Production, learning, projects     |
| **Development** | `0.5.0-dev` | Unstable  | Testing new features, contributing |

Weave defaults to stable releases. Development builds require manual download from the releases page.

The `main` branch tracks head. Stable releases are tagged branches only.

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
    auto node_buffer = vega.NodeBuffer(0, 512, math) | Audio[0];
)");

// Modify while running
live_interpreter.eval(R"(
    math->set_input_node(vega.Impulse(1, 0.1f));
)");
```

---

## Documentation

- **[Getting Started](docs/Getting_Started.md)** — Setup, basic usage, first program
- **[Onboarding from different tools](https://mayaflux.org/onboarding/)** — A rosetta stone for users coming from various multimedia tools and environments
- **[Entry point from professions](https://mayaflux.org/personas/)** — Guides for artists, musicians, researchers, and developers
- **[Release Notes](https://mayaflux.org/releases/)** — Quick changelog and rationale for each release

### Tutorials

- **[Sculpting Data Part I](https://mayaflux.org/tutorials/sculpting-data/foundations/)**: Foundational concepts, data-driven workflow, containers, buffers, processors.
- **[Processing Expression](https://mayaflux.org/tutorials/sculpting-data/processing_expression/)**: Buffers, processors, math as expression, logic as creative decisions.
- **[Visual Materiality](https://mayaflux.org/tutorials/sculpting-data/visual_materiality_i/)**: Graphics basics, geometry nodes, the Vulkan rendering pipeline from a user perspective, multi-window rendering.

### API Documentation

Build locally:

```sh
doxygen doxyconf
open docs/html/index.html
```

Auto-generated docs:

- **[GitHub Pages](https://mayaflux.github.io/MayaFlux/)**
- **[GitLab Pages](https://mayaflux.gitlab.io/MayaFlux/)**
- **[Codeberg Pages](https://mayaflux.codeberg.page/)**

---

## Project Maturity

| Area                        | Status      | Notes                                                                    |
| --------------------------- | ----------- | ------------------------------------------------------------------------ |
| Core DSP Architecture       | Stable      | Lock-free, concurrent, sample-accurate                                   |
| Audio Backends              | Stable      | PipeWire (Linux), WASAPI (Windows), CoreAudio (macOS); RtAudio removed   |
| MIDI Backends               | Stable      | PipeWire/ALSA (Linux), WinMM (Windows), CoreMIDI (macOS); RtMidi removed |
| Windowing Backends          | Stable      | Wayland native (Linux), Win32 native (Windows), GLFW (macOS)             |
| Live Coding (Lila)          | Stable      | Sub-buffer JIT compilation via LLVM ORC; MayaFluxHost as standalone lib  |
| Node Graphs                 | Stable      | Generator, filter, network, GpuSync, input, routing, compositing         |
| IO and Containers           | Stable      | Audio, video, camera, image, mesh; full read and write paths via FFmpeg  |
| Graphics (Vulkan)           | Stable      | Dynamic rendering, multi-window, geometry, texture, compute, instancing  |
| Coroutine Infrastructure    | Stable      | SoundRoutine, GraphicsRoutine, CrossRoutine, FreeRoutine, BroadcastSource|
| Input                       | Stable      | HID, MIDI, OSC backends; key held, mouse drag, release callbacks         |
| 3D Mesh Pipeline            | Stable      | glTF, FBX, OBJ, PLY via Assimp; MeshNetwork operator graph; GPU fields  |
| Instanced Rendering         | Stable      | InstanceNetwork, InstanceNetworkBuffer, InstanceFieldOperator GPU path   |
| GPU SDF / Marching Cubes    | Stable      | ComputeMeshBuffer, SDFMeshProcessor, GPU field shaders                   |
| Networking                  | Stable      | UDP/TCP transport, OSC, coroutine-based receive                          |
| Nexus                       | Stable      | Emitter, Sensor, Agent, Locus, Presence, Expanse, Tapestry, StateEncoder |
| Portal::Forma               | Stable      | Signal-driven UI surfaces, Bridge bindings, Inspector, Plot, Collapsible |
| Portal::System              | Stable      | Native OS file dialogs on Linux, Windows, macOS                          |
| Portal::Text                | Stable      | FreeType GPU text, incremental append, GlyphOutline decomposition        |
| SamplingPipeline            | Stable      | Polyphonic slice playback                                                |
| Viewport Navigation         | Stable      | Fly, Orbit, PanZoom2D, Screenspace presets; remappable key maps          |
| Yantra/GranularWorkflow     | Stable      | Offline granular grammar pipeline, OLA reconstruction                    |
| Video Write Path            | Stable      | SoundFileWriter, VideoFileWriter, FFmpegMuxContext, window capture       |
| BroadcastSource             | Stable      | Lock-free cross-thread signal delivery to coroutines                     |
| Yantra Grammar System       | In Progress | Core framework stable; additional grammars planned for 0.4               |

**Current version**: 0.4.0-dev  
**Trajectory**: 0.4 feature freeze approaching. Focus on Forma orchestration layer completion, documentation, contributor programs, and conference submissions (ICMC, NIME, CppCon, LAC).

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

...this is the standalone (but designed for integration) implementation.

**Everything is open source. Contribute, fork it, build on it, challenge it.**

---

## Roadmap (Provisional)

### Phase 1 (Complete)

- Core audio architecture
- Live coding via Lila
- Public availability
- Vulkan graphics pipeline

### Phase 2 (Complete)

- Complete live signal matrix (file/live x audio/video)
- Release plumbing, documentation, and example demos
- Cross-domain synchronization
- Video file and live camera input
- HID and MIDI input
- Physical modelling primitives and classes

### Phase 3 (Complete)

- 3D mesh pipeline: glTF/FBX/OBJ/PLY loading, MeshNetwork operator graph, MeshFieldOperator, indexed draw
- Networking: UDP/TCP transport, OSC, coroutine-based receive via NetworkSource
- Nexus spatial entity lifecycle: Emitter, Agent, Sensor, Fabric, Wiring
- Yantra/GranularWorkflow: offline granular grammar pipeline with OLA reconstruction
- SamplingPipeline: polyphonic slice playback
- Portal::Text: FreeType GPU text rendering with incremental append API
- Public debut: TOPLAP Bengaluru live performance on Steam Deck (Arch Linux, Distrobox)

### Phase 4 (0.4, current)

- Native audio backends: PipeWire (Linux), WASAPI (Windows), CoreAudio (macOS); RtAudio and RtMidi removed
- Native windowing: Wayland (Linux), Win32 (Windows)
- Portal::System: native OS file dialogs on all platforms
- Portal::Forma: signal-driven UI surface system; Bridge, Inspector, Plot, viewport presets
- Nexus: Locus, Presence, Expanse, Tapestry; StateEncoder/StateDecoder round-trip (schema v5)
- Instanced rendering: InstanceNetwork, InstanceNetworkBuffer, GPU field dispatch path
- GpuComputeNode: async GPU dispatch with fence polling; CrossRoutine, FreeRoutine, BroadcastSource
- Video write path: SoundFileWriter, VideoFileWriter, FFmpegMuxContext, window capture
- GPU SDF/marching cubes pipeline: ComputeMeshBuffer, SDFMeshProcessor, GPU field shaders
- DataWriteProcessor: unified write path replacing GeometryWriteProcessor and TextureWriteProcessor
- Viewport navigation presets: Fly, Orbit, PanZoom2D, Screenspace; remappable key maps
- CMakePresets.json hierarchy; ccache; Arch Linux CI

### Phase 5 (0.5)

- Forma as primary authoring surface; expanded UI primitive library
- ONNX Runtime integration for ML inference
- libdatachannel WebRTC/peer-to-peer networking
- Community module registry; MF_SC_NOISE as first reference module
- Institutional partnerships; expanded conference presence

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
