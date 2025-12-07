# MayaFlux - Getting Started Guide

**MayaFlux** is a next-generation digital signal processing framework that embraces true digital paradigms.
Moving beyond analog synthesis metaphors, MayaFlux treats audio, video, and all data streams as unified numerical information
that can interact, transform, and process together through powerful algorithms, coroutines, and ahead-of-time computation techniques.

## Weave: The Easy Way (Recommended for Most Users)

**Weave is the recommended way to start with MayaFlux**.
It handles everything from installation to project creation in one tool, available for **Windows, macOS, and Linux**.

### What Weave Does

- **Downloads & Installs** MayaFlux automatically
- **Manages Dependencies** (build tools, audio/video libraries, SDKs)
- **Creates New Projects** with proper configuration
- **Platform-Specific Setup** with intelligent fallbacks

### Getting Weave

**Download the latest release:** [Weave Releases Page](https://github.com/MayaFlux/Weave/releases)

### Using Weave

**Windows & Linux:**

1. Download Weave and double-click the executable
2. A GUI opens with two modes:
   - **Management Mode**: Install MayaFlux and dependencies
   - **Project Creation Mode**: Create new projects (available after installation)
3. Follow the intuitive interface - your project is ready in minutes

**macOS:**

1. **Installation**: Run the `.pkg` installer (includes both MayaFlux and dependencies)
2. **Project Creation**: Find `Weave.app` in your Applications folder for creating new projects
3. **Architecture Support**: Separate builds available for Intel (x86) and Apple Silicon (ARM)

> **Why Use Weave?** You get a properly configured MayaFlux environment without dealing with compilers, CMake, or dependency management.
> Perfect for artists, educators, and rapid prototyping.

Jump to [Working with MayaFlux](#your-first-mayaflux-program)

---

## Building from Source (For Developers)

**For developers who want the latest features or need to customize MayaFlux's internals.**

### Prerequisites

**All Platforms:**

- **Compiler**: C++20 compatible (GCC 12+, Clang 16+, MSVC 2022+)
- **Build System**: CMake 3.20+

**Platform-Specific Requirements:**

- **macOS**: Minimum macOS 14 (Sonoma) with Apple Clang 15+
- **Windows**: Visual Studio 2022+ or MinGW-w64
- **Linux**: Standard development tools and multimedia libraries

### Quick Build

```bash
# Clone the repository
git clone https://github.com/MayaFlux/MayaFlux.git
cd MayaFlux

# Run platform setup script
./scripts/setup_macos.sh       # macOS (brew will ask for sudo )
./scripts/setup_linux.sh       # Linux (package manager will ask for sudo)
.\scripts\win64\setup_windows.ps1  # Windows (PowerShell) (run as Administrator to install dependencies)

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

#### macOS clarifications

**System Requirements:**

- **ARM64 (Apple Silicon)**: macOS 14 (Sonoma) or later
- **x86_64 (Intel)**: macOS 15 (Sequoia) or later
- **Binary distributions**: Available for both ARM64 and x86_64
- **Building from source**: Supported on macOS 14+ (ARM64) or 15+ (Intel)

**Apple Silicon Users**: All automated setup and distribution channels work seamlessly.

**Intel Mac Users (macOS 15+)**: Pre-built binaries available via releases.

**Intel Mac Users (pre-macOS 15)**: Requires manual source build:

```bash
# Standard CMake build (source build only)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(sysctl -n hw.ncpu)
```

See [Building on Intel Macs](#building-on-intel-macs) for detailed instructions.

### IDE-Specific Setup

**Visual Studio Code (Recommended):**

- Open the project folder - VS Code automatically detects the CMake configuration
- Uses pre-configured `.vscode/` settings for build tasks, debugging, and IntelliSense
- **Build**: `Ctrl+Shift+B` (Cmd+Shift+B on macOS)
- **Run**: `F5`

**Xcode (macOS):**

```bash
./scripts/setup_xcode.sh
open build/MayaFlux.xcodeproj
```

**Visual Studio (Windows):**

```powershell
.\scripts\win64\setup_visual_studio.ps1
# Open build/MayaFlux.sln in Visual Studio
```

**Neovim:**

- Install [overseer.nvim](https://github.com/stevearc/overseer.nvim) for task management
- Overseer automatically reads `.vscode/` tasks for seamless build/run/debug workflows

> **Note**: All setup scripts handle dependency installation automatically. No manual package management required.

### Project Structure

```text

MayaFlux/
├── src/
│   ├── MayaFlux/           # Core library
│   ├── Lila/               # Live coding interface
│   └── main.cpp            # Your code entry point
│   └── user_project.hpp    # Your custom project code
├── res/                    # Audio files and resources
├── data/shaders/           # Shader files for graphics processing
├── scripts/                # Platform setup scripts
├── build/                  # Generated build files (after setup)
```

---

## Building on Intel Macs

MayaFlux provides pre-built x86_64 (Intel) binaries for macOS 15+. For older Intel Macs (pre-macOS 15), you'll need to build from source.

### Prerequisites

1. **macOS 14+** with Xcode Command Line Tools
2. **Homebrew** (for dependency management)
3. Manual dependency installation

### Build Process

```bash
# 1. Verify LLVM installation (critical for Lila JIT)
brew list llvm  # Should show installed
llvm-config --version  # Should print version

# 2. Clone MayaFlux
git clone https://github.com/MayaFlux/MayaFlux.git

# 3. Install dependencies manually
./scripts/setup_macos.sh

# 4. Build MayaFlux
cd MayaFlux
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++

cmake --build . --parallel $(sysctl -n hw.ncpu)
```

### Known Considerations

- **Xcode's system Clang**: May not have full C++23 support. Use Homebrew LLVM above.
- **Vulkan on Intel**: MoltenVK layer used; performance acceptable for development.
- **LLVM JIT (Lila)**: Ensure Homebrew LLVM is installed, not system Clang.

### Troubleshooting

**"llvm-config not found"**

```bash
export LLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm
# Then retry cmake
```

**"Clang version too old"**

```bash
# Use Homebrew's Clang
cmake .. -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++
```

---

## Your First MayaFlux Program

Now that your environment is set up, let's create your first MayaFlux application.

### Understanding the Code Structure

MayaFlux projects use a simple but powerful structure:

**`src/user_project.hpp`** - Your creative workspace (never overwritten by updates)
**`src/main.cpp`** - Engine entry point (don't modify unless you need custom startup)

### The Two Key Functions

**`void settings()` - Configuration (Runs BEFORE engine start)**
Think of this as your "Preferences" panel - it runs once when the application launches to set up the environment:

```cpp
void settings() {
    // Audio settings
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;  // Studio quality
    stream.buffer_size = 128;    // Low latency
    stream.output.channels = 2;  // Stereo

    // Graphics settings
    MayaFlux::Config::target_frame_rate = 60;

    // Logging
    MayaFlux::Journal::set_journal_severity(MayaFlux::Journal::Severity::INFO);
}
```

**`void compose()` - Your Creative Canvas (Runs AFTER engine start)**
This is where your audio, graphics, and algorithms live - it executes continuously while the engine runs:

```cpp
void compose() {
    // Load and play an audio file
    vega.read_audio("res/audio/example.wav") | Audio;

    // Your creative code goes here!
}
```

### The Digital Bell: A Cross-Modal Experience

Replace the contents of your `src/user_project.hpp` with this working example
This example demonstrates the core philosophy of MayaFlux: **unified data transformation across audio, logic, and graphics.**

```cpp
#pragma once
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

void settings() {
    // Low-latency audio setup
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
}

void compose() {

    // 1. Create the bell
    auto bell = vega.ModalNetwork(
                    12,
                    220.0,
                    ModalNetwork::Spectrum::INHARMONIC)[0]
        | Audio;

    // 2. Create audio-driven logic
    auto source_sine = vega.Sine(0.2, 1.0f); // 0.2 Hz slow oscillator

    static double last_input = 0.0;
    auto logic = vega.Logic([](double input) {
        // Arhythmic: true when sine crosses zero AND going positive
        bool crossed_zero = (last_input < 0.0) && (input >= 0.0);
        last_input = input;
        return crossed_zero;
    });

    source_sine >> logic;

    // 3. When logic fires, excite the bell
    logic->on_change_to([bell](auto& ctx) {
        if (ctx.value != 0) {
            bell->excite(get_uniform_random(0.5f, 0.9f));
            bell->set_fundamental(get_uniform_random(220.0f, 1000.0f));
        }
    },
        true);

    // 4. Graphics (same as before)
    auto window = MayaFlux::create_window({ "Audio-Driven Bell", 1280, 720 });
    auto points = vega.PointCollectionNode(500) | Graphics;
    auto geom = vega.GeometryBuffer(points) | Graphics;

    geom->setup_rendering({ .target_window = window });
    window->show();

    // 5. Visualize: points grow when bell strikes (when logic fires)
    MayaFlux::schedule_metro(0.016, [points]() {
        static float angle = 0.0f;
        static float radius = 0.0f;

        if (last_input != 0) {
            angle += 0.5f; // Quick burst on strike
            radius += 0.002f;
        } else {
            angle += 0.01f; // Slow growth otherwise
            radius += 0.0001f;
        }

        if (radius > 1.0f) {
            radius = 0.0f;
            points->clear_points();
        }

        float x = std::cos(angle) * radius;
        float y = std::sin(angle) * radius * (16.0f / 9.0f);
        float brightness = 1.0f - (radius * 0.7f);

        points->add_point(Nodes::GpuSync::PointVertex {
            .position = glm::vec3(x, y, 0.0f),
            .color = glm::vec3(brightness, brightness * 0.8f, 1.0f),
            .size = 8.0f + radius * 4.0f });
    });
}
```

### What This Demonstrates

**In 50 lines of code, you've created:**

1. **Physical Modeling Synthesis** - A 12-mode modal bell with inharmonic spectrum
2. **Generative Logic** - Irregular timing using zero-crossing detection
3. **Cross-Modal Coordination** - Audio events driving visual behavior
4. **Real-time Graphics** - Dynamic point cloud with audio-reactive growth
5. **Unified Architecture** - All domains working as one system

### Running Your Program

**VS Code:** Press `F5` to build and run  
**Terminal:** From your project root:

```bash
cd build
cmake --build . --target run
```

You should hear a bell ringing at irregular intervals while watching a spiral pattern grow and burst in response to each strike!

### Understanding the Structure

- **`settings()`** - One-time audio configuration (low latency)
- **`compose()`** - Your live coding canvas where everything happens
- **Audio Domain** (`| Audio`) - Bell synthesis and logic
- **Graphics Domain** (`| Graphics`) - Window, points, and rendering
- **Cross-Domain** - Logic events trigger both audio and visual changes

### Key Concepts Introduced

- **Nodes** (`vega.ModalNetwork`, `vega.Sine`, `vega.Logic`) - Unit generators
- **Containers** (`vega.PointCollectionNode`) - Multi-dimensional data
- **Coroutines** (`MayaFlux::schedule_metro`) - Temporal coordination
- **Domain Tokens** (`| Audio`, `| Graphics`) - Processing characteristics
- **Event Handling** (`on_change_to`) - Reactive programming

> **This is MayaFlux**: Where mathematical relationships become creative decisions, time becomes compositional material, and audio/visual/data streams are unified numerical transformations.

This example perfectly captures the MayaFlux philosophy while being immediately runnable and visually/aurally compelling. It shows rather than tells what makes the framework unique.

---

## MayaFlux Philosophy

### Core Philosophy: Digital-First Multimedia

MayaFlux represents a fundamental shift from analog-inspired tools to true digital paradigms:

Traditional Approach (Analog Simulation):

- Audio, video, and data as separate domains
- Hardware simulation (knobs, cables, oscilloscopes)
- Callback-based timing from interrupt models
- File-based workflows with static resources

MayaFlux Approach (Digital Native):

- Unified data transformation: All streams are numerical data that can interact
- Coroutine temporal control: Time as creative material, not interrupt callbacks
- Grammar-defined operations: Declarative computation based on data characteristics
- Cross-modal coordination: Audio parameters control GPU compute shaders; visual analysis modulates audio filters

### Core Paradigms

MayaFlux is built around five fundamental digital paradigms that replace traditional analog thinking:

- **Nodes**: Unit-by-unit transformation precision
- **Containers**: Multi-dimensional data as creative material
- **Coroutines**: Time as compositional material
- **Buffers**: Temporal gathering spaces for data accumulation
- **Compute Matrix**: Declarative semantic pipelines

**Deep dive:** Read the [Digital Transformation Paradigm](Digital_Transformation_Paradigm.md) for comprehensive architectural understanding.

Explore the shape of things to come!

---

## Using This Guide & Tutorials

### For Beginners: Learning Just Enough C++

> **If you're new to C++**
>
> MayaFlux uses modern C++ (C++17/20). You don't need to be an expert—just comfortable reading and editing small functions.
> The best way to learn is hands-on:
>
> - **The openFrameworks "ofBook"** has an excellent short section on C++ basics that map well to creative coding workflows.
> - The free [cppreference.com](https://en.cppreference.com/w/) pages on _"Variables," "Functions,"_ and _"Classes"_ are reliable quick reads.
> - If you prefer a creative-coding approach, check out _The openFrameworks C++ Chapter_ or _Daniel Shiffman's "The Nature of Code (C++ port)"_ for conceptual grounding.
>
> You can safely begin MayaFlux tutorials with only this level of knowledge. The tutorials explain real C++ constructs as they appear—no memorization needed.

### How to Use the Tutorials

**MayaFlux tutorials are designed for exploration, not passive reading.**

1.  **Run the visible code.**
    Copy the non-hidden (top-level) snippets into `compose()` and run them.

2.  **Listen or observe what happens.**
    Every tutorial produces a real, audible or visual result.

3.  **Open the dropdowns (`<details>`).**
    These reveal what's happening under the hood. Read them slowly; come back later as your understanding grows.

4.  **Experiment.**
    Change numbers, reorder calls, or combine examples. Each tweak teaches you something about the system.

5.  **Iterate.**
    As you learn, revisit earlier tutorials. MayaFlux rewards depth—you'll understand new layers every time.

The goal is **fluency, not memorization**. You're not "following a recipe"—you're learning how the machinery works so you can build your own.

## Starting Your First Tutorial

Now that your environment is set up, you're ready to begin.

**"Sculpting Data"** is the entry tutorial series. It starts with the simplest operation
(loading a file) and builds systematically toward complete pipeline architectures.

Each section in Sculpting Data teaches one core idea and builds on the previous:

1. **Section 1: Load** - Understand Containers (how data is organized)
2. **Section 2: Connect** - Understand Buffers (how data flows)
3. **Section 2.5: Process** - Understand Processors (how data transforms)
4. **Section 3: Timing** - Understand timing control (how you schedule)
5. **Section 4:** (Coming) BufferOperation (how you compose)

**Start here:** Open [Sculpting Data Part I](Tutorials/SculptingData/SculptingData.md) and begin with Section 1.

Each section is designed to be executed immediately:

- Copy the code examples into your `compose()` function. This is always after a heading `Tutorial:`
- Build and run your project
- Listen/observe the result
- Open the dropdowns to understand why it works
- Experiment by changing values

You'll have working audio within 5 minutes.

### If You've Already Started

If you have already completed the aforementioned tutorial, proceed to the next tutorial at
[Sculpting Data Part II, Processing Expression](Tutorials/SculptingData/ProcessingExpression.md) which covers buffers, processors, math as expression, logic as creative decisions and more.

## FAQ (MacOS)

### Q: I have an Intel Mac. Can I use MayaFlux?

**A**: Yes, fully!

- **macOS 15+**: Pre-built binaries available via GitHub releases
- **Pre-macOS 15**: Build from source following the [Building on Intel Macs](#building-on-intel-macs) guide (~10-15 minutes)

Intel Macs have identical functionality to Apple Silicon.

### Q: Why is macOS 15 minimum for Intel binaries?

**A**: Our CI/CD uses GitHub's `macos-15` runners for Intel builds. For older Intel systems, source builds are fully supported and well-documented. If you need binaries for specific older versions, open an issue to discuss.

### Q: Performance difference between ARM64 and x86_64?

**A**: No meaningful difference for MayaFlux. Both are fully optimized. Vulkan performance may vary slightly due to MoltenVK translation layer, but both work well.
