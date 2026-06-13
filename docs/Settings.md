# MayaFlux Engine Configuration

`settings()` runs before `MayaFlux::Init()`. Anything you set here takes effect when the engine initialises and is locked for the lifetime of the process. Modifying config structs after `Init()` logs a warning and has no effect.

Defaults are intentionally reasonable: 48 kHz, 512-frame buffer, stereo output, 60 FPS, Vulkan rendering. Leave `settings()` empty if the defaults are adequate.

---

## Configuration via JSON file

The entire engine configuration can be expressed as a JSON file and loaded in one call before `Init()`. Every config struct exposes a `describe()` method used by `JSONSerializer`; the JSON keys map directly to those field names.

```cpp
void settings() {
    MayaFlux::Config::load_config_from_file("mayaflux.json");
}
```

Fields absent from the file retain their defaults. A `--config-override path/to/file.json` flag on the launcher binary also loads a config file before `settings()` runs, which can be combined with in-code overrides afterward.

A complete file covering all four sections plus journal:

```json
{
  "stream": {
    "sample_rate": 48000,
    "buffer_size": 256,
    "format": "FLOAT64",
    "non_interleaved": false,
    "output": { "enabled": true, "channels": 2, "device_id": -1, "device_name": "" },
    "input":  { "enabled": false, "channels": 2, "device_id": -1, "device_name": "" },
    "priority": "REALTIME",
    "handle_xruns": true,
    "stream_latency_ms": 0.0
  },
  "graphics": {
    "target_frame_rate": 60,
    "surface_info": {
      "format": "B8G8R8A8_SRGB",
      "color_space": "SRGB_NONLINEAR",
      "present_mode": "FIFO",
      "image_count": 3,
      "enable_hdr": false,
      "measure_frame_time": false
    },
    "resource_limits": {
      "max_windows": 16,
      "max_staging_buffer_mb": 256,
      "max_compute_buffer_mb": 1024,
      "max_texture_cache_mb": 2048,
      "max_descriptor_sets": 1024,
      "max_pipelines": 256
    },
    "text_config": {
      "family": "sans-serif",
      "style": "",
      "pixel_size": 24,
      "atlas_size": 512
    }
  },
  "input": {
    "hid":   { "enabled": false },
    "midi":  { "enabled": false },
    "osc":   { "enabled": false, "receive_port": 8000 },
    "serial":{ "enabled": false }
  },
  "network": {
    "udp": { "enabled": false, "default_receive_port": 8000, "default_send_port": 9000 },
    "tcp": { "enabled": false, "listen_port": 0 },
    "shared_memory": { "enabled": false }
  },
  "journal": {
    "severity": "INFO",
    "sink_to_console": false,
    "log_file": "",
    "disable_components": [],
    "disable_contexts": []
  }
}
```

---

## Audio stream: `GlobalStreamInfo`

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();

    stream.sample_rate = 48000;       // Hz
    stream.buffer_size = 256;         // frames per callback; lower = less latency, more CPU

    stream.output.channels   = 2;     // stereo out
    stream.output.device_id  = -1;    // -1 = system default
    stream.output.device_name = "";   // empty = system default; name takes precedence over id

    stream.input.enabled  = false;    // disabled by default
    stream.input.channels = 2;

    stream.priority = Core::GlobalStreamInfo::StreamPriority::REALTIME;
    stream.handle_xruns = true;
    stream.stream_latency_ms = 0.0;   // 0 = driver default
}
```

The audio backend (`PIPEWIRE`, `WASAPI`, `COREAUDIO`) is selected at compile time and cannot be changed at runtime. Backend-specific tuning can be passed through `stream.backend_options` as an `unordered_map<string, any>`.

`StreamPriority` options: `LOW`, `NORMAL`, `HIGH`, `REALTIME`.

---

## Graphics: `GlobalGraphicsConfig`

```cpp
void settings() {
    auto& gfx = MayaFlux::Config::get_global_graphics_config();

    gfx.target_frame_rate = 60;

    // Surface defaults inherited by all windows; individual windows can override.
    gfx.surface_info.present_mode = Core::GraphicsSurfaceInfo::PresentMode::FIFO;
    gfx.surface_info.format       = Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB;
    gfx.surface_info.image_count  = 3;   // triple buffering
    gfx.surface_info.enable_hdr   = false;

    // Default font for Portal::Text
    gfx.text_config.family     = "sans-serif";
    gfx.text_config.pixel_size = 24;
    gfx.text_config.atlas_size = 512;

    // Resource budgets
    gfx.resource_limits.max_texture_cache_mb  = 2048;
    gfx.resource_limits.max_compute_buffer_mb = 1024;
    gfx.resource_limits.max_windows           = 16;
}
```

`PresentMode` options: `FIFO` (vsync), `IMMEDIATE` (no vsync), `MAILBOX` (triple buffer), `FIFO_RELAXED`.

`SurfaceFormat` options: `B8G8R8A8_SRGB` (default), `R8G8B8A8_SRGB`, `R16G16B16A16_SFLOAT` (HDR), `A2B10G10R10_UNORM`, others.

The windowing backend (`GLFW` on macOS, `WAYLAND` on Linux, `WINDOWS` on Windows) is selected at compile time. `WindowingBackend::NONE` disables windowing entirely for headless/offline use and must be set if no windows are created.

On macOS, `gfx.glfw_preinit_config` exposes GLFW pre-init hints.

Key repeat timing for native backends (Wayland, Win32) is in `gfx.key_repeat_config`.

---

## Input: `GlobalInputConfig`

Input backends are all disabled by default. Enable only what you need.

```cpp
void settings() {
    auto& input = MayaFlux::Config::get_global_input_config();

    input.midi.enabled = true;

    input.osc.enabled      = true;
    input.osc.receive_port = 8000;

    input.hid.enabled = true;
    // input.hid.filters to restrict to specific device types

    // Convenience factory methods:
    // auto input = Core::GlobalInputConfig::with_midi();
    // auto input = Core::GlobalInputConfig::with_osc(8000);
    // auto input = Core::GlobalInputConfig::with_gamepads();
    // auto input = Core::GlobalInputConfig::with_all_hid();
}
```

---

## Network: `GlobalNetworkConfig`

```cpp
void settings() {
    auto& net = MayaFlux::Config::get_global_network_config();

    net.udp.enabled              = true;
    net.udp.default_receive_port = 8000;
    net.udp.default_send_port    = 9000;

    net.tcp.enabled     = true;
    net.tcp.listen_port = 0;    // 0 = OS-assigned

    // Convenience factory methods:
    // auto net = Core::GlobalNetworkConfig::with_udp(8000, 9000);
    // auto net = Core::GlobalNetworkConfig::with_tcp();
    // auto net = Core::GlobalNetworkConfig::with_osc(8000, 9000);
}
```

---

## Journal (logging)

```cpp
void settings() {
    // Severity threshold: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
    MayaFlux::Config::set_journal_severity(MayaFlux::Journal::Severity::INFO);

    // Sinks (additive; cannot be removed after being added)
    MayaFlux::Config::sink_journal_to_console();
    MayaFlux::Config::store_journal_entries("run.log");  // also sets severity to TRACE

    // Per-component or per-context filtering
    MayaFlux::Config::set_journal_component_filter(
        { MayaFlux::Journal::Component::Buffers,
          MayaFlux::Journal::Component::Vruta }, false);

    MayaFlux::Config::set_journal_context_filter(
        { MayaFlux::Journal::Context::AudioProcessing }, false);
}
```

Via JSON the same fields are in the `"journal"` key. `"disable_components"` and `"disable_contexts"` take string names matching the enum values (case-insensitive).

Journal configuration is the only part of `settings()` that takes effect immediately rather than at `Init()`.

---

## Reading config at runtime

After `Init()`, config values are readable from anywhere:

```cpp
void compose() {
    uint32_t sr  = MayaFlux::Config::get_sample_rate();
    uint32_t buf = MayaFlux::Config::get_buffer_size();
    uint32_t ch  = MayaFlux::Config::get_num_out_channels();
}
```

The full structs are also accessible via `get_global_stream_info()` etc., but writing to them after `Init()` logs a warning and will not take effect.

---

## Common configurations

**Low-latency performance:**
```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 128;
    stream.output.channels = 2;

    auto& gfx = MayaFlux::Config::get_global_graphics_config();
    gfx.surface_info.present_mode = Core::GraphicsSurfaceInfo::PresentMode::MAILBOX;
    gfx.target_frame_rate = 120;
}
```

**Audio input enabled:**
```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.sample_rate = 48000;
    stream.buffer_size = 512;
    stream.input.enabled  = true;
    stream.input.channels = 2;
}
```

**Headless / offline (no window):**
```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.buffer_size = 4096;

    auto& gfx = MayaFlux::Config::get_global_graphics_config();
    gfx.windowing_backend = Core::GlobalGraphicsConfig::WindowingBackend::NONE;
}
```

**OSC + MIDI input:**
```cpp
void settings() {
    auto& input = MayaFlux::Config::get_global_input_config();
    input.osc.enabled      = true;
    input.osc.receive_port = 9000;
    input.midi.enabled     = true;
}
```

**Load all settings from file, then override one value:**
```cpp
void settings() {
    MayaFlux::Config::load_config_from_file("mayaflux.json");

    // Override just the buffer size regardless of what the file says
    MayaFlux::Config::get_global_stream_info().buffer_size = 128;
}
```
