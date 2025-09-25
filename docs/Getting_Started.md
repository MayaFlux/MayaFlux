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
and audio/multimedia libraries (rtaudio, ffmpeg, eigen3, gtest, magic_enum and glfw) using your distribution's package manager.

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
   - Set up IntelliSense for code completion
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
#define MAYASIMPLE;
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
├── res/                    # Audio files and resources
├── scripts/                # Platform setup scripts
├── build/                  # Generated build files (after setup)
```

## Next Steps

1. **Read the Digital Transformation Paradigm** [documentation](Digital_Transformation_Paradigm.md) for deep architectural understanding
2. **Explore composing with processing domains and tokens** [documentation](Domain_and_Control.md)

_Advanced_ users can refer to [Advanced Context Control](Advanced_Context_Control.md) that explores architecture and backend level
customization in `MayaFlux Core`

## Future Roadmap

MayaFlux is designed for expansion into a complete digital multimedia ecosystem:

- **GL/Vulkan GPU Integration**: Real-time visual processing and compute shaders
- **Live Coding**: Lua integration via Sol2 and Cling integration
- **Game Engine Plugins**: UE5 and Godot integration
- **Cross-Language FFI**:

## Philosophy

MayaFlux represents a fundamental shift from analog-inspired audio software toward truly digital paradigms. Instead of simulating vintage hardware, we embrace the computational possibilities that only exist in the digital realm - recursive processing, multi dimensional data handling, generating on the fly grammars and pipelines, data-driven control, cross-modal interaction, and algorithmic composition techniques that treat code as creative material.

Explore the shape of things to come!
