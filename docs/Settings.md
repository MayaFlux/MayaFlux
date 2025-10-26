# MayaFlux `settings()` - Engine Configuration Guide

## Overview

The `settings()` function is your **single entry point for configuring the MayaFlux engine before it starts**. Think of it like opening **Preferences** in your DAW—you set up the environment once, and these settings are locked in when the engine initializes.

Once `MayaFlux::Init()` is called (which happens automatically in the default `main.cpp`), these settings take effect and generally cannot be changed without restarting the engine.

---

## Two Configuration Levels

### 1. **Audio Stream Configuration** (GlobalStreamInfo)

Controls how audio I/O behaves: sample rate, buffer size, input/output channels.

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();

    stream.sample_rate = 48000;        // Hz (44100, 48000, 96000, etc.)
    stream.buffer_size = 512;          // Frames per buffer (64, 128, 256, 512, 1024, etc.)

    // Output configuration (to speakers/DAC)
    stream.output.channels = 2;        // Stereo
    stream.output.format = stream.output.format; // Leave as-is unless you know what you're doing

    // Input configuration (from microphone/ADC)
    stream.input.channels = 2;         // Stereo in
    stream.input.enabled = true;       // Enable audio input
}
```

**Key Parameters:**

- **sample_rate**: Higher = better frequency response, more CPU. Typical: 44100 Hz (CD) or 48000 Hz (pro audio)
- **buffer_size**: Lower = lower latency but more CPU overhead. Typical: 256–1024 frames
- **output.channels**: Mono (1), Stereo (2), surround (>2)
- **input.enabled / input.channels**: Leave disabled if you don't need mic input

---

### 2. **Graphics Configuration** (GlobalGraphicsConfig)

Controls windowing, rendering backend, frame rate, and GPU settings.

```cpp
void settings() {
    auto& graphics = MayaFlux::Config::get_global_graphics_config();

    // Windowing backend
    graphics.windowing_backend = graphics.WindowingBackend::GLFW; // GLFW, SDL, NATIVE, or NONE

    // Graphics API
    graphics.requested_api = graphics.GraphicsApi::VULKAN;  // VULKAN, OPENGL, METAL, DIRECTX12

    // Frame rate
    graphics.target_frame_rate = 60;  // Hz (60, 120, 144, etc.)

    // Surface appearance
    graphics.surface_info.format = graphics.surface_info.SurfaceFormat::R8G8B8A8_SRGB;
    graphics.surface_info.color_space = graphics.surface_info.ColorSpace::SRGB_NONLINEAR;
    graphics.surface_info.present_mode = graphics.surface_info.PresentMode::FIFO;  // Vsync

    // GPU resource limits
    graphics.resource_limits.max_windows = 4;
    graphics.resource_limits.max_texture_cache_mb = 2048;
}
```

**Key Parameters:**

- **windowing_backend**: GLFW (cross-platform default), SDL, platform-native, or NONE (offscreen only)
- **requested_api**: Vulkan (modern, default), OpenGL (compatibility), Metal (macOS), DirectX12 (Windows)
- **target_frame_rate**: Sync visual processing to this rate (60 Hz typical)
- **surface_info.present_mode**:
  - `FIFO` = Vsync enabled (smooth, no tearing)
  - `IMMEDIATE` = No Vsync (lower latency, possible tearing)
  - `MAILBOX` = Triple buffering (best of both)

---

### 3. **Node Graph Semantics** (Rarely needed)

Controls how nodes behave when connected.

```cpp
void settings() {
    auto& graph = MayaFlux::Config::get_graph_config();

    // How nodes handle connections (usually leave as default)
    // graph.chain_semantics = MayaFlux::Utils::NodeChainSemantics::REPLACE_TARGET;
    // graph.binary_op_semantics = MayaFlux::Utils::NodeBinaryOpSemantics::REPLACE;
}
```

---

### 4. **Logging / Journal**

Control how much the engine logs and where logs go.

```cpp
void settings() {
    // Log everything (verbose debugging)
    MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::TRACE);

    // Or only warnings and errors
    MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::WARN);

    // Output logs to console in real-time
    MayaFlux::Config::sink_journal_to_console();

    // Or save logs to a file
    MayaFlux::Config::store_journal_entries("mayaflux.log");
}
```

---

## Common Scenarios

### Scenario 1: Simple Audio (Default)

```cpp
void settings() {
    // Defaults are fine for most uses:
    // - 48 kHz, stereo out, 512 sample buffer
    // - Vulkan graphics, 60 FPS
    // - Minimal logging

    // Just leave empty or add logging for debugging:
    MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::INFO);
}
```

### Scenario 2: Low-Latency Real-Time (Music Performance)

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 128;  // Low latency (2.6 ms at 48 kHz)
    stream.output.channels = 2;

    auto& graphics = MayaFlux::Config::get_global_graphics_config();
    graphics.target_frame_rate = 120;  // Smooth visuals
    graphics.surface_info.present_mode =
        graphics.surface_info.PresentMode::IMMEDIATE;  // No vsync for responsiveness
}
```

### Scenario 3: Studio Recording (High Quality)

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 96000;  // Studio quality
    stream.buffer_size = 1024;   // Can afford latency
    stream.input.enabled = true;
    stream.input.channels = 2;   // Mic input
    stream.output.channels = 2;

    MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::TRACE);
    MayaFlux::Config::store_journal_entries("studio_session.log");
}
```

### Scenario 4: Headless / Offline Processing (No Graphics)

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 44100;
    stream.buffer_size = 2048;  // Larger buffer for batch processing

    auto& graphics = MayaFlux::Config::get_global_graphics_config();
    graphics.windowing_backend = graphics.WindowingBackend::NONE;  // No window
}
```

### Scenario 5: Linux with Wayland (Platform-Specific)

```cpp
void settings() {
    auto& graphics = MayaFlux::Config::get_global_graphics_config();
    graphics.glfw_preinit_config.platform = graphics.glfw_preinit_config.Platform::Wayland;
    graphics.glfw_preinit_config.disable_libdecor = true;  // Avoid crashes on some Wayland

    graphics.surface_info.present_mode = graphics.surface_info.PresentMode::MAILBOX;
}
```

---

## Important Notes

1. **Timing**: `settings()` runs _before_ `MayaFlux::Init()` in your `main.cpp`. Changes made here take effect when the engine initializes.

2. **No Runtime Changes**: Once `Init()` is called, these settings are essentially locked. Don't try to change them during `compose()` or during playback—they won't take effect until the next restart.

3. **Defaults are Reasonable**: If you don't touch `settings()`, MayaFlux uses sensible defaults:
   - 48 kHz, stereo, 512 sample buffer (audio)
   - Vulkan, 60 FPS, SRGB surface (graphics)
   - Minimal logging

4. **Advanced Users**: If you need even finer control, access the raw config structs directly:

   ```cpp
   auto& stream = MayaFlux::Config::get_global_stream_info();
   auto& graphics = MayaFlux::Config::get_global_graphics_config();
   // Modify whatever you need
   ```

5. **This is C++**: You can use loops, conditionals, environment variables, config files—whatever makes sense:
   ```cpp
   void settings() {
       const char* env_sr = std::getenv("MAYAFLUX_SAMPLE_RATE");
       if (env_sr) {
           auto& stream = MayaFlux::Config::get_global_stream_info();
           stream.sample_rate = std::stoi(env_sr);
       }
   }
   ```

---

## Full Example: user_project.hpp

```cpp
#pragma once
#include "MayaFlux/MayaFlux.hpp"

/**
 * Configure the MayaFlux engine (sample rate, buffer size, graphics backend, etc.)
 *
 * This runs BEFORE MayaFlux::Init(). Use this to set preferences like you would
 * in a DAW's Preferences menu. Leave empty to use sensible defaults.
 *
 * Once Init() is called, these settings take effect and persist until shutdown.
 */
void settings()
{
    // Configure audio
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 512;
    stream.output.channels = 2;

    // Configure graphics
    auto& graphics = MayaFlux::Config::get_global_graphics_config();
    graphics.target_frame_rate = 60;

    // Enable logging for debugging
    // MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::TRACE);
    // MayaFlux::Config::sink_journal_to_console();
}

/**
 * Create and register your DSP nodes, buffers, and coroutines here.
 *
 * This runs AFTER MayaFlux::Init() but BEFORE MayaFlux::Start().
 * Everything you register here persists for the lifetime of the engine.
 */
void compose()
{
    // Example: create a simple sine oscillator
    // vega.sine(440).channel(0) | Audio;
}
```

---

## Accessing Configuration Later

From within `compose()` or anywhere in your code, you can _read_ the current configuration:

```cpp
void compose() {
    uint32_t sr = MayaFlux::Config::get_sample_rate();
    uint32_t buffer_sz = MayaFlux::Config::get_buffer_size();
    uint32_t out_channels = MayaFlux::Config::get_num_out_channels();

    // Use these values to initialize your nodes correctly
    auto sine = vega.sine(440.0).channel(0) | Audio;
}
```

But **don't try to modify** `settings()` values in `compose()`—they won't take effect until next restart.
