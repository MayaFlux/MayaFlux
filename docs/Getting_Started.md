# MayaFlux: Getting Started

MayaFlux is a C++20/23 framework for real-time computation across sound, geometry, image, and network. All data is the same numerical substrate; the domain annotation decides where a buffer cycle goes, not what the data is.

This guide covers environment setup and the structure of a MayaFlux program. Tutorials that build on this foundation are at [mayaflux.org/tutorials](https://mayaflux.org/tutorials).

---

## Weave: Recommended for Most Users

**Weave** is the installer and project tool for MayaFlux. Download it from the [Weave releases page](https://github.com/MayaFlux/Weave/releases).

### Installing MayaFlux

Launch Weave and choose **Install MayaFlux**. Weave downloads the framework, installs all required dependencies, and configures your environment. Restart your terminal when it finishes.

### Creating a Project

Launch Weave and choose **Create Project**. Enter a name and pick a destination. Weave generates a ready-to-build project:

```
MyProject/
├── CMakeLists.txt
├── CMakePresets.json
├── community.cmake
├── cmake/
│   ├── mayaflux.cmake
│   ├── shaders.cmake
│   └── build_community.cmake
├── src/
│   ├── main.cpp
│   └── user_project.hpp
├── data/
│   └── shaders/
└── .gitignore
```

`src/user_project.hpp` is where you write your code. `CMakeLists.txt` is yours to edit; the MayaFlux integration lives in `cmake/mayaflux.cmake` and is included automatically.

`CMakePresets.json` ships with every project and defines `debug` and `release` presets. CLion, VS Code with the CMake extension, and Visual Studio all pick up the presets automatically when you open the project folder.

```bash
cmake --preset release
cmake --build --preset release
./build/MyProject
```

On Windows the binary lands in `build\MyProject.exe`.

### Live Coding (Lila)

Check **Enable Live Coding (Lila)** in the project creation dialog to embed the Lila JIT compiler in your process. Connect [LilaCode](https://github.com/MayaFlux/LilaCode) (VS Code) or [lila.nvim](https://github.com/MayaFlux/lila.nvim) (Neovim) to evaluate C++ against your running application in real time.

### Community Modules

Community modules are C++ source libraries that compile directly into your project with no plugin boundary. In Weave, open your project and choose **Add Community Module**. Weave fetches the [community registry](https://github.com/MayaFlux/community-sources-registry), checks version compatibility, clones the module into `community/<name>/`, and registers it in `community.cmake`. Rebuild after adding modules.

After Weave completes setup, jump to [Program Structure](#program-structure).

---

## Building from Source

### Requirements: All Platforms

- **Compiler:** C++20 (GCC 12+, Clang 16+, MSVC 2022+)
- **Build system:** CMake 3.28+
- **Required dependencies:** Vulkan SDK, FFmpeg (avcodec, avformat, avutil, swresample, swscale, avdevice), GLM, Eigen, HIDAPI, Asio, Assimp, FreeType, utf8proc, nlohmann_json, fmt, TBB, STB, LLVM 21+

All dependencies are required. CMake will not generate if any are missing.

### Requirements: Platform-Specific

**Linux:**
- PipeWire (audio and MIDI)
- libdbus-1 (XDG Portal file dialogs)
- wayland-protocols, libwayland-client, xkbcommon (windowing)
- fontconfig (system font discovery)

**macOS:**
- macOS 15+ (Sequoia), Apple Silicon or Intel
- Apple Clang 16+ via Xcode Command Line Tools
- GLFW (via Homebrew, for windowing)
- Frameworks linked automatically: CoreAudio, AudioUnit, AudioToolbox, CoreMIDI, AppKit, UniformTypeIdentifiers, CoreFoundation

**Windows:**
- Visual Studio 2022+ (MSVC) or MinGW-w64
- LLVM 22+ (for Lila JIT)

### Build

```sh
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux

# Install all dependencies and configure environment variables
./scripts/setup_macos.sh                      # macOS
./scripts/setup_linux.sh                      # Linux
.\scripts\win64\setup_windows.ps1             # Windows - must be run as Administrator (requires UAC elevation)

# Build using CMakePresets
cmake --preset linux-release       # or macos-release / windows-release
cmake --build --preset linux-release
```

`setup_windows.ps1` installs all required packages via `packages.psd1` and configures environment variables. It must be run elevated; UAC will prompt if you launch it normally.

---

## Program Structure

A MayaFlux program has two entry points defined in `src/user_project.hpp`:

```cpp
#pragma once
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

void settings() {
    // Runs before the engine starts.
    // Configure sample rate, buffer size, channels, logging.
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 256;
    stream.output.channels = 2;
}

void compose() {
    // Runs after the engine starts.
    // All computation, routing, scheduling, and rendering lives here.
}
```

`main.cpp` calls `Init()`, `settings()`, `Start()`, `compose()`, `Await()`, and `End()` in order. You do not edit it unless you need custom engine configuration beyond what `settings()` exposes.

### MAYASIMPLE

Defining `MAYASIMPLE` before including `MayaFlux.hpp` pulls in the full concrete type set and brings all MayaFlux namespaces into scope. Without it you get the API surface only. User projects built via Weave define it by default.

### The `vega` global

`vega` is the global `Creator` instance. It is the factory for all computation objects: generators, filters, networks, buffers, containers, mesh loaders, input nodes. Every object created through `vega` is registered with the engine automatically.

```cpp
auto sine   = vega.Sine(440.0, 0.5) | Audio[0];
auto modal  = vega.ModalNetwork(12, 220.0) | Audio[{0, 1}];
auto audio  = vega.read_audio("res/drum.wav") | Audio;
auto img    = vega.read_image("res/texture.png") | Graphics;
auto mesh   = vega.read_mesh_network("res/scene.glb");
```

The `| Audio` and `| Graphics` operators are domain annotations. They attach a processing token that decides which subsystem drives the object and at what rate. They do not change what the object is.

---

## A First Program

The example below is complete and runnable. It loads a texture, generates a parametric surface, and animates it frame by frame via a coroutine inside a Nexus entity. A movable light agent influences all render processors simultaneously. Keyboard input moves the light.

```cpp
#pragma once
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nexus/Tapestry.hpp"

void settings()
{
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 256;
    stream.output.channels = 2;
}

void compose()
{
    auto window = MayaFlux::create_window({ "surface", 1920, 1080 });

    auto view = []() -> Kinesis::ViewTransform {
        return Kinesis::look_at_perspective(
            glm::vec3(0.0F, 2.0F, 5.0F),
            glm::vec3(0.0F),
            glm::radians(45.0F), 1920.0F / 1080.0F, 0.1F, 100.0F);
    };

    auto img = vega.read_image("res/texture.png") | Graphics;

    // Parametric surface: initial geometry
    auto surf_fn = [](float u, float v, float t) -> glm::vec3 {
        float a = u * glm::two_pi<float>();
        float b = v * glm::two_pi<float>();
        return {
            std::sin(a + t) * (1.0F + 0.3F * std::cos(3.0F * b)),
            std::sin(2.0F * a - t) * std::sin(b) * 0.5F,
            std::cos(a + t) * (1.0F + 0.3F * std::cos(3.0F * b)),
        };
    };

    auto data = Kinesis::generate_parametric_surface(
        [&](float u, float v) { return surf_fn(u, v, 0.0F); }, 64, 64);

    auto surf_node = std::make_shared<MeshWriterNode>(data.vertex_count()) | Graphics;
    surf_node->set_mesh(data);

    auto surf_buf = vega.GeometryBuffer(surf_node) | Graphics;
    surf_buf->set_texture(img);
    surf_buf->setup_rendering({
        .target_window = window,
        .vertex_shader   = "triangle_lit.vert.spv",
        .fragment_shader = "mesh_textured_lit.frag.spv",
        .topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST,
    });
    surf_buf->get_render_processor()->set_view_transform_source(view);

    // Light agent: influences the surface render processor
    auto light_pos = make_persistent_shared<glm::vec3>(0.0F, 1.5F, 0.0F);
    constexpr glm::vec3 half { 0.04F, 0.04F, 0.04F };
    auto& mgr = *MayaFlux::get_buffer_manager();

    auto light = std::make_shared<Nexus::Agent>(
        0.0F,
        [](const Nexus::PerceptionContext&) {},
        [](const Nexus::InfluenceContext&) {});
    light->set_position(*light_pos);
    light->set_color(glm::vec3(1.0F, 0.9F, 0.7F));
    light->set_intensity(1.5F);
    light->set_radius(4.0F);
    light->render(mgr, { .target_window = window,
        .topology = Portal::Graphics::PrimitiveTopology::LINE_LIST });
    light->get_render_processor(window)->set_view_transform_source(view);
    light->set_vertices<LineVertex>(Kakshya::to_line_vertices(
        Kinesis::cuboid_wireframe(*light_pos, half, glm::vec3(1.0F, 0.9F, 0.7F))));
    light->add_influence_target(surf_buf->get_render_processor());

    auto fabric = make_persistent_shared<Nexus::Fabric>(
        *MayaFlux::get_scheduler(), *MayaFlux::get_event_manager());

    fabric->wire(light)
        .every(1.0 / 60.0, Vruta::ProcessingToken::FRAME_ACCURATE)
        .finalise();

    // Surface animation: coroutine updates mesh geometry each frame
    auto st = make_persistent(0.F);
    auto surf_driver = std::make_shared<Nexus::Emitter>(
        [surf_node, surf_fn, &st](const Nexus::InfluenceContext&) {
            auto d = Kinesis::generate_parametric_surface(
                [&](float u, float v) { return surf_fn(u, v, st); }, 64, 64);
            const auto& vb = std::get<std::vector<uint8_t>>(d.vertex_variant);
            surf_node->set_mesh_vertices(
                std::span(reinterpret_cast<const MeshVertex*>(vb.data()), d.vertex_count()));
            st += 0.01F;
        });

    fabric->wire(surf_driver)
        .every(1.0 / 60.0, Vruta::ProcessingToken::FRAME_ACCURATE)
        .finalise();

    // Keyboard: move the light
    constexpr float step = 0.05F;
    auto move = [&](glm::vec3 delta) {
        *light_pos += delta;
        light->set_position(*light_pos);
        light->set_vertices<LineVertex>(Kakshya::to_line_vertices(
            Kinesis::cuboid_wireframe(*light_pos, half, glm::vec3(1.0F, 0.9F, 0.7F))));
    };

    auto mk_mover = [&](glm::vec3 d) {
        return std::make_shared<Nexus::Emitter>(
            [move, d](const Nexus::InfluenceContext&) { move(d); });
    };

    fabric->wire(mk_mover({ step,  0,     0    })).on(window, IO::Keys::D, true).finalise();
    fabric->wire(mk_mover({-step,  0,     0    })).on(window, IO::Keys::A, true).finalise();
    fabric->wire(mk_mover({ 0,     step,  0    })).on(window, IO::Keys::W, true).finalise();
    fabric->wire(mk_mover({ 0,    -step,  0    })).on(window, IO::Keys::S, true).finalise();
    fabric->wire(mk_mover({ 0,     0,     step })).on(window, IO::Keys::Q, true).finalise();
    fabric->wire(mk_mover({ 0,     0,    -step })).on(window, IO::Keys::E, true).finalise();

    window->show();
}
```

What this shows:

- `vega.read_image` and `vega.GeometryBuffer` are factory calls; domain annotation follows at the call site
- A `GraphicsRoutine` coroutine inside a `Nexus::Emitter` owns its animation loop, suspended one frame at a time via `FrameDelay`
- A `Nexus::Agent` acting as a light registers influence targets directly on render processors; moving the light position updates all of them simultaneously
- Keyboard input wires through `Wiring::on(key, held)` - each key is its own entity, each entity's influence function applies the delta

---

## What to Read Next

- **[Digital Architecture](Digital_Architecture.md)**: how the substrate works, what each namespace does, how everything composes
- **[Sculpting Data I](https://mayaflux.org/tutorials/sculpting-data/foundations/)**: first tutorial series; start here if you have not written a MayaFlux program before
- **[Processing Expression](https://mayaflux.org/tutorials/sculpting-data/processing_expression/)**: buffers, processors, math as expression
- **[Visual Materiality I](https://mayaflux.org/tutorials/sculpting-data/visual_materiality_i/)**: graphics pipeline from a user perspective
- **[API docs](https://mayaflux.github.io/MayaFlux/)**: generated reference; build locally with `doxygen doxyconf`

---

## FAQ

### macOS: do I need Homebrew LLVM?

No. MayaFlux compiles with Apple Clang from Xcode Command Line Tools. Homebrew LLVM is not used for compilation. LLVM is required as a runtime dependency for Lila (the JIT environment) and is installed by the setup script, but it is not your compiler.

### What is the minimum macOS version?

macOS 15 (Sequoia) on both Apple Silicon and Intel. Earlier versions lack the C++20 stdlib coverage MayaFlux requires.

### What happens if a dependency is missing?

CMake configuration fails with an explicit error. There are no optional dependencies; everything in the requirements list is required for the build to complete.

### Can I use MayaFlux without the JIT (Lila)?

Yes. Lila is part of the engine but you do not have to use it. Programs written entirely in `compose()` and compiled normally do not touch the JIT path. LLVM must still be present for the build to succeed.

### Where does my code go?

In `src/user_project.hpp`. It is never overwritten by Weave updates. `main.cpp` is the engine entry point and should not be modified unless you need a custom startup sequence.
