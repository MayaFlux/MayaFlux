# MayaFlux - Getting Started Guide

**MayaFlux** is a next-generation digital signal processing framework that embraces true digital paradigms. Moving beyond analog synthesis metaphors, MayaFlux treats audio, video, and all data streams as unified numerical information that can interact, transform, and process together through powerful algorithms, coroutines, and ahead-of-time computation techniques.

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/MayaFlux/MayaFlux.git
# or
git clone https://gitlab.com/MayaFlux/MayaFlux.git
#or
git clone https://codeberg.org/MayaFlux/MayaFlux.git
# and then
cd MayaFlux
```

### 2. Run Platform Setup Script

MayaFlux includes automated setup scripts that handle all dependencies and build system configuration.

#### Windows

```powershell
# Run in PowerShell (Administrator recommended)
.\scripts\setup_windows.ps1

# For Visual Studio users
.\scripts\setup_visual_studio.ps1
```

#### macOS

```bash
# Run this in any terminal
./scripts/setup_macos.sh

# For XCode users
./scripts/setup_xcode.sh
```

**System Requirements:**
- **Minimum**: macOS 14 (Sonoma) with Apple Clang 15+
- **Recommended**: macOS 15+ (Sequoia)
- **Academic Users**: macOS 14 provides excellent compatibility for institutional/shared machines

> **Note**: macOS 13 and earlier are not supported due to missing modern C++ standard library features.

#### Linux

Linux users typically manage dependencies through their package manager. Install the standard development tools (cmake, build-essential, pkg-config)
and devel/multimedia libraries (rtaudio, ffmpeg, eigen3, gtest, magic_enum, glfw, vulkan-sdk, glm, stb) using your distribution's package manager.

However, you can also run the provided setup script to automate this process for Debian-based systems, fedora, and Arch Linux:

```bash
# Run this in any terminal
./scripts/setup_linux.sh
```

> **Note**: All setup scripts automatically handle dependency installation and build system configuration. No manual dependency management required.

## Development Environment Setup

### For VS Code Users (Recommended for New Programmers)

If you're coming from creative coding environments like p5.js, Processing, or SuperCollider, VS Code provides the most approachable transition to C++.

1. **Install VS Code** and the **C/C++ Extension Pack**

2. **Open the Project:**

   ```bash
   code MayaFlux
   ```

3. **That's it!** VS Code will automatically:
   - Detect the CMake configuration
   - Set up LSP/IntelliSense for code completion
   - Configure build and debug settings
   - Handle compilation when you press `F5`

### For Visual Studio Users

After running `setup_visual_studio.ps1`:

1. **Open the solution** file: `build/MayaFlux.sln`
2. **Set the startup project** in Solution Explorer
3. **Build and run** using standard Visual Studio workflows

Visual Studio users will find all familiar debugging and profiling tools work seamlessly with MayaFlux.

### For Neovim Users

Install **overseer.nvim** for seamless task management:

```lua
-- Add to your plugin manager
{
  'stevearc/overseer.nvim',
  config = function()
    require('overseer').setup()
  end,
}
```

Overseer will automatically detect the CMake build system and provide build/run tasks. Advanced users can create custom templates for MayaFlux-specific workflows.

## Your First MayaFlux Program

After running the setup script, you'll find a `user_project.hpp` file in the src/ directory of your project root. This is where all your MayaFlux code goes:

```cpp
// src/user_project.hpp - Your code space (never overwritten by updates)
#pragma once
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

void compose() {
    // Your MayaFlux code goes here!
}
```

The setup script automatically creates this file if it doesn't exist, but will never overwrite your existing code. This means you can safely pull updates to MayaFlux without losing your work.

## Understanding MayaFlux Architecture

### Core Paradigms

MayaFlux is built around four fundamental digital paradigms that replace traditional analog thinking:

- **Nodes**: Unit-by-unit transformation precision
- **Containers**: Multi-dimensional data as creative material
- **Coroutines**: Time as compositional material
- **Buffers**: Temporal gathering spaces for data accumulation

### Digital-First Approach

Unlike traditional audio software that mimics analog hardware, MayaFlux embraces computational possibilities:

- **Data-driven processing**: All streams (audio, video, control) are unified numerical data
- **Ahead-of-time computation**: Pre-calculate complex transformations
- **Recursive algorithms**: Process data in ways impossible with analog circuits
- **Grammar-defined operations**: Use parsing techniques for signal processing
- **Cross-modal interaction**: Audio parameters control visual processing and vice versa

## Project Structure

```
MayaFlux/
├── src/
│   ├── MayaFlux/           # Core library
│   └── main.cpp            # Your code entry point
│   └── user_project.hpp    # Your custom project code
├── res/                    # Audio files and resources
├── data/shaders/           # Shader files for graphics processing
├── scripts/                # Platform setup scripts
├── build/                  # Generated build files (after setup)
```


## Philosophy

MayaFlux represents a fundamental shift from analog-inspired audio software toward truly digital paradigms. Instead of simulating vintage hardware, we embrace the computational possibilities that only exist in the digital realm - recursive processing, multi dimensional data handling, generating on the fly grammars and pipelines, data-driven control, cross-modal interaction, and algorithmic composition techniques that treat code as creative material.

Explore the shape of things to come!

## Using This Guide

### For Beginners: Learning Just Enough C++

> **If you’re new to C++**
>
> MayaFlux uses modern C++ (C++17/20). You don’t need to be an expert—just comfortable reading and editing small functions.
> The best way to learn is hands-on:
>
> * **The openFrameworks “ofBook”** has an excellent short section on C++ basics that map well to creative coding workflows.
> * The free [cppreference.com](https://en.cppreference.com/w/) pages on *“Variables,” “Functions,”* and *“Classes”* are reliable quick reads.
> * If you prefer a creative-coding approach, check out *The openFrameworks C++ Chapter* or *Daniel Shiffman’s “The Nature of Code (C++ port)”* for conceptual grounding.
>
> You can safely begin MayaFlux tutorials with only this level of knowledge.
> The tutorials explain real C++ constructs as they appear—no memorization needed.

---

### Compiling and Running in VS Code

> **Running your first MayaFlux program**
>
> 1. Open your project folder in VS Code (`code MayaFlux`).
> 2. Press **`Ctrl+Shift+B`** (or **`Cmd+Shift+B`** on macOS). This runs the CMake *build* task.
> 3. When the build finishes, press **`F5`** to *run* the executable.
> 4. You’ll see output in the **terminal panel** and, if your code plays audio, you’ll hear it immediately.
>
> VS Code automatically uses the CMake configuration created by the setup script.
> You don’t need to run any manual compiler commands—just *build* and *run*.

*(Optional line to add if you want to reinforce the C++ learning mindset)*

> Every time you press **F5**, you’re compiling real C++ and running it live.
> This is the same pipeline used in professional software development—no “toy” interpreters here.

---

### Understanding Your `user_project.hpp`

 **Your creative workspace lives in `src/user_project.hpp`.**

 * `settings()` runs once, *before* the engine starts. Use it to set sample rate, buffer size, graphics API, and logging options.
 * `compose()` runs *after* setup—it’s your canvas for sound, data, graphics and computation.
 * You can define functions or global variables above `compose()` if you want to reuse them inside it.
 * This file is never overwritten by MayaFlux updates—you own it.

 Example:

 ```cpp
 void settings() {
     auto& stream = MayaFlux::Config::get_global_stream_info();
     stream.sample_rate = 48000;
     stream.buffer_size = 128;
 }

 void compose() {
     vega.load("res/audio/track.wav") | Audio;
 }
 ```

---

### How to Use the Tutorials

 **MayaFlux tutorials are designed for exploration, not passive reading.**

 1. **Run the visible code.**
    Copy the non-hidden (top-level) snippets into `compose()` and run them.
 2. **Listen or observe what happens.**
    Every tutorial produces a real, audible or visual result.
 3. **Open the dropdowns (`<details>`).**
    These reveal what’s happening under the hood. Read them slowly; come back later as your understanding grows.
 4. **Experiment.**
    Change numbers, reorder calls, or combine examples. Each tweak teaches you something about the system.
 5. **Iterate.**
    As you learn, revisit earlier tutorials. MayaFlux rewards depth—you’ll understand new layers every time.

 The goal is fluency, not memorization. You’re not “following a recipe”—you’re learning how the machinery works so you can build your own.

## Next Steps

1. **Read the Digital Transformation Paradigm** [documentation](Digital_Transformation_Paradigm.md) for deep architectural understanding
2. **Explore composing with processing domains and tokens** [documentation](Domain_and_Control.md)

_Advanced_ users can refer to [Advanced Context Control](Advanced_Context_Control.md) that explores architecture and backend level
customization in `MayaFlux Core`

## Starting Your First Tutorial

Now that your environment is set up, you're ready to begin.

**"Sculpting Data"** is the entry tutorial series. It starts with the simplest operation (loading a file) and builds systematically toward complete pipeline architectures.

Each section in Sculpting Data teaches one core idea and builds on the previous:
1. **Section 1: Load** - Understand Containers (how data is organized)
2. **Section 2: Connect** - Understand Buffers (how data flows)
3. **Section 2.5: Process** - Understand Processors (how data transforms)
4. **Section 3: Timing** - Understand timing control (how you schedule)
5. **Section 4:** (Coming) BufferOperation (how you compose)

**Start here:** Open [Sculpting Data Part I](Tutorials/SculptingData/SculptingData.md) and begin with Section 1.

Each section is designed to be executed immediately:
- Copy the code examples into your `compose()` function
- Run with `F5`
- Listen/observe the result
- Open the dropdowns to understand why it works
- Experiment by changing values

You'll have working audio within 5 minutes.

### Procedure

If you have already completed the aforementioned tutorial, proceed to the next tutorial 
at [Sculpting Data Part II, Processing Expression](Tutorials/SculptingData/ProcessingExpression.md)
which covers buffers, processors, math as expression, logic as creative decisions and more.
