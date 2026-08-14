# MayaFlux: Dev Getting Started

For building the framework itself from source and running code against it in-tree. If you used Weave to create a standalone project, see the main [Getting Started](Getting_Started.md) guide instead. This doc is for contributors working inside the `MayaFlux/MayaFlux` repo directly.

---

## Just the Facts

```sh
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux

./scripts/setup_linux.sh    # or setup_macos.sh / win64/setup_windows.ps1

cmake --preset unix-dev     # or windows-vs2026-dev / windows-vs2022-dev
cmake --build --preset unix-dev

./build/project_launcher
```

Write your code in `src/user_project.hpp`. It is created for you from a template on first configure if it does not already exist, and it is never overwritten afterward. This is your scratch space for exercising `MayaFluxLib` before writing a real test, not an official example, see the section below.

If you configure without a dev preset, `project_launcher` will not exist as a target. `MAYAFLUX_BUILD_PROJECT` defaults OFF; the dev presets are the ones that turn it ON.

---

## Requirements

This list is only for building MayaFlux itself from source. If using Weave to create a standalone project, see the [Getting Started](Getting_Started.md) guide for that, where all dependencies are automatically handled via package managers.

Building from source currently tracks 0.5-dev, ahead of Weave's 0.4.1, and the minimum OS floor is correspondingly newer:

| Platform | Weave (0.4.1) | Building from source (0.5-dev) |
|---|---|---|
| Windows | 10, version 1909+ | 10, version 1909+ |
| Fedora | 43 | 44 |
| Ubuntu | 25.10 | 26.04 LTS |
| macOS | 15 | 26 (Tahoe) |

The Windows floor is set by MSVC 2022's own minimum supported OS, not by anything in `scripts/win64/setup_windows.ps1` or `packages.psd1`, neither contains an OS version check. Win32 windowing, WinMM, and WASAPI all predate Windows 10 by a wide margin and impose no additional constraint.

**All platforms:** C++20 compiler (GCC 12+, Clang 16+, MSVC 2022+), CMake 3.28+, Vulkan SDK, FFmpeg (avcodec, avformat, avutil, swresample, swscale, avdevice), GLM, Eigen, HIDAPI, Asio, Assimp, FreeType, utf8proc, nlohmann_json, fmt, TBB, STB, LLVM 21+.

All dependencies are required; CMake will not configure if any are missing.

**Linux (Fedora 44+ / Ubuntu 26.04 LTS+):** PipeWire (audio and MIDI), libdbus-1 (XDG Portal file dialogs), wayland-protocols, libwayland-client, xkbcommon, fontconfig.
**Note:** `jack2` conflicts with pipewire upstream. Use `pipewire-jack` which is functionally equivalent to jack2 but compatible with pipewire. If you have jack2 installed, remove it and install `pipewire-jack` instead.

**macOS (26+, Tahoe):** Apple Silicon or Intel, Apple Clang 17+ via Xcode Command Line Tools, GLFW via Homebrew (windowing; see the macOS platform maintainer callout in `CONTRIBUTING.md`, this is expected to change). Frameworks linked automatically: CoreAudio, AudioUnit, AudioToolbox, CoreMIDI, AppKit, UniformTypeIdentifiers, CoreFoundation.

**Windows (10, version 1909+):** Visual Studio 2022+ (MSVC) or MinGW-w64, LLVM 22+ (for Lila JIT).

### Setup Scripts

```sh
./scripts/setup_linux.sh                      # Linux
./scripts/setup_macos.sh                       # macOS
.\scripts\win64\setup_windows.ps1              # Windows, run as Administrator
```

These install the dependency list above and configure environment variables. `setup_windows.ps1` installs everything via `packages.psd1` and must run elevated, UAC will prompt if launched normally.

macOS note: Homebrew LLVM is not your compiler. MayaFlux compiles with Apple Clang from Xcode Command Line Tools. LLVM is a runtime dependency for Lila only, and the setup script installs it for that purpose.

---

## Presets

| Preset | MAYAFLUX_BUILD_PROJECT | MAYAFLUX_DEV | MAYAFLUX_BUILD_TESTS | Purpose |
|---|---|---|---|---|
| `unix-dev` | ON | ON | ON | Linux/macOS contributor build |
| `windows-vs2026-dev` | ON | ON | off | Windows contributor build, VS 2026 |
| `windows-vs2022-dev` | ON | ON | off | Windows contributor build, VS 2022 |
| `linux-ship-dev`, `macos-ship-dev` | off | off | off | Shipping build with debug info, no `project_launcher` |
| `linux-ship-rel`, `macos-ship-rel`, `windows-ship-rel` | off | off | off | Release packaging, no `project_launcher` |
| `launchpad-dev`, `launchpad-rel` | off | off | off | PPA packaging, no `project_launcher` |

A bare `cmake -B build` with no preset also leaves `MAYAFLUX_BUILD_PROJECT` OFF. If you want `project_launcher` without a preset:

```sh
cmake -B build -DMAYAFLUX_BUILD_PROJECT=ON
cmake --build build
```

---

## Targets

**project_launcher**
The executable that runs your code. Built from `main.cpp` plus `${USER_SOURCES}` (which includes `src/user_project.hpp`) plus anything under `src/examples/`. Gated behind `MAYAFLUX_BUILD_PROJECT`; the CMakeLists returns before defining this target if that option is off. This is the pre-install, pre-package way to exercise `MayaFluxLib` directly, not an official demo app; see below.

`main.cpp` includes `user_project.hpp` via `__has_include`, falling back to `MAYASIMPLE` mode if the file is not found. It runs, in order: parse `--config` / `--config-override` args, load JSON config if present, call your `settings()`, `MayaFlux::Init()`, `MayaFlux::Start()`, call your `compose()`, block on Enter, `MayaFlux::End()`.

**MayaFluxLib**
Core shared library and the actual deliverable. Built from `add_subdirectory(MayaFlux)`. Everything under `src/MayaFlux/` (Nexus, Yantra, Kakshya, Kinesis, Vruta, Kriya, Portal, and the rest) compiles into this target. No entry point. `project_launcher` links against this.

**MayaFluxHost**
Pulled in via `include(Host/Host.cmake)`. Embeds Lila directly into a host process instead of running it as a separate server. `Host::attach_lila` is the in-process attachment point. `project_launcher` links `MayaFluxLib MayaFluxHost`.

**Lila**
The JIT library, built from `add_subdirectory(Lila)`. Provides LLVM ORC JIT evaluation of arbitrary C++20/23 via an embedded Clang interpreter. `MayaFluxHost` embeds this.

**lila_server**
Standalone executable (`lila_server.cpp`), linked against `Lila` directly. Runs the JIT as its own process rather than embedded in a host, for networked live coding sessions over TCP. Separate from `project_launcher`.

---

## src/user_project.hpp

Generated automatically the first time you configure, from `cmake/user_project.hpp.in`, if it does not already exist at `${CMAKE_SOURCE_DIR}/src/user_project.hpp`. It is not touched again after that, so your edits persist across reconfigures and rebuilds.

MayaFlux is a library, not an app. `MayaFluxLib` is the actual deliverable; `project_launcher` exists to give that library somewhere to run without requiring an installed package or a separate consuming project. Treat `user_project.hpp` as your workbench, not as official example code:

- Sketch and exercise new `MayaFluxLib` code here before it has proper coverage in `tests/`.
- Reproduce a bug here first, then decide whether it belongs in `tests/` as a real GoogleTest case.
- Run demos here, your own or someone else's, without needing to spin up a separate project against an installed build.

If a change is going into a PR and it is warranted, back it with an actual test in `tests/`. `user_project.hpp` is not a substitute for that and is not itself part of the reviewed surface, it is scratch space that happens to be functionally identical to what Weave scaffolds for end users. Do not treat code left there as documentation or as a demonstrated contract.

This is also why it is never overwritten: several people's in-progress scratch work can sit in the same file across sessions without CMake clobbering it on reconfigure.

---

## src/examples/

`src/examples/` exists so `user_project.hpp` does not become a dumping ground. Any `.hpp` / `.cpp` under this directory is picked up automatically (`file(GLOB_RECURSE EXAMPLES ...)`) and compiled into `project_launcher` alongside `user_project.hpp`. Put a trial, a prototype, a demo you are handing to someone else, or a reproduction case here as its own file instead of overwriting or piling onto `user_project.hpp`. Several people's throwaway work can coexist this way without collisions.

`src/examples/shaders/` is the same idea applied to shaders. Anything compiled from here is example-only, scoped to `MAYAFLUX_DEV` builds via `compile_example_shaders`, and never promoted into `data/shaders/` unless it has actually earned general-purpose, official status. `data/shaders/` is the engine's own shader set, compiled unconditionally by `compile_shaders` into `${CMAKE_BINARY_DIR}/shaders`; treat that boundary as real. A shader under `src/examples/shaders/` staying there indefinitely is the expected outcome, not a sign it needs to be moved.

If `EXAMPLE_SHADER_DIR` is set (the dev presets set it to `${sourceDir}/src/examples/shaders`), any `.vert` / `.frag` / `.comp` / `.geom` / `.tesc` / `.tese` / `.mesh` / `.task` / `.rgen` / `.rchit` / `.rmiss` files under that directory are compiled to `.spv` via `glslc` by `compile_example_shaders`. `project_launcher` depends on this target when it exists and gets `MAYAFLUX_EXAMPLE_SHADER_DIR` defined to point at the source directory.

---

## IDE Setup

`compile_commands.json` is generated at `${CMAKE_BINARY_DIR}/compile_commands.json` and symlinked to the repo root on Unix automatically, for clangd and similar tools.

On Windows, `regenerate_solution` reruns `scripts/win64/setup_visual_studio.ps1` to rebuild the `.sln`. LLVM's internal generator targets (`acc_gen`, `clang-tablegen-targets`, etc.) are grouped under an `LLVM` solution folder to keep the tree navigable.

---

## Tests

`MAYAFLUX_BUILD_TESTS` is ON under `unix-dev`, off elsewhere by default. Run with the matching test preset:

```sh
cmake --build --preset unix-dev
ctest --preset unix-dev
```

---

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for workflow and PR conventions, [docs/StarterTasks.md](StarterTasks.md) for entry-level tasks, and [docs/BuildOps.md](BuildOps.md) if you are interested in CI, packaging, or distribution rather than the engine itself.
