# MayaFlux 0.4.0

**Commit**: {{COMMIT_SHA}}

---

0.3 completed the offline compute grammar, the Nexus spatial simulation layer, and the GPU-CPU bridge in both directions. 0.4 does something different in character: it turns MayaFlux from a framework into a substrate.

The change is architectural rather than additive. Every third-party runtime abstraction that existed for portability has been replaced with platform-native implementations. The build system has been rebuilt around composable presets. The JIT environment has been extracted into a standalone library any C++ project can embed. A complete GPU-resident compute pipeline lands alongside a symmetric write/export layer covering every data type the engine can ingest. Portal::Forma graduates from scaffolding to a production interactive surface system. CrossRoutine closes the cross-domain scheduling gap without a mutex in sight.

The result is a collection of focused, first-class systems that compose with each other and can be used independently: Lila without MayaFlux, Forma without the audio pipeline, GPU compute without the rendering loop, capture without the spatial system. None of these required the others. They interoperate because they share the same scheduler, the same buffer machinery, and the same data carriers, not because they are coupled.

---

## Platform

Every third-party backend abstraction has been replaced on Linux and Windows. GLFW is gone on both platforms. RtAudio is gone on all platforms. RtMidi is gone on Linux and Windows.

**Windowing**: native Wayland backend with full xkb key mapping, scroll direction correction, client-side key repeat via `timerfd`, and deferred `wl_display_disconnect`. Native Win32 backend with lock-free event queue and client-side key repeat. `MAYAFLUX_LINUX_USE_GLFW` restores the GLFW path when needed. macOS retains GLFW pending a native Cocoa backend.

**Audio**: native PipeWire backend on Linux with RT scheduling, device name resolution, `pw_time` exposure, xrun detection, and rate-match quantum. Native WASAPI backend on Windows with WASAPI hardware buffer size decoupled from the engine buffer size. Native CoreAudio backend on macOS with separate HAL units for input and output.

**MIDI**: native PipeWire/ALSA MIDI backend on Linux via the ALSA sequencer enumerated through PipeWire. Native WinMM MIDI backend on Windows. Native CoreMIDI backend on macOS with refresh deadlock eliminated and port cleanup unified.

**System dialogs**: `Portal::System` provides native file choosers on all platforms: XDG Desktop Portal via libdbus-1 on Linux, `IFileOpenDialog`/`IFileSaveDialog` via Shell COM on Windows, `NSOpenPanel`/`NSSavePanel` via AppKit on macOS. `Depot` exposes typed convenience functions with native dialog backing: `choose_audio`, `choose_video`, `choose_image`, `choose_mesh`, `choose_mesh_network`, `save_audio`, `save_image`.

---

## Build system

The build system has been rebuilt around `CMakePresets.json` with a composable preset hierarchy. A `windows-base` standalone mixin and the `MAYAFLUX_BUILD_PROJECT` option make MayaFlux embeddable downstream without fighting the build system. XCode support has been dropped. LLVM is at 22.1.6 on Windows and macOS. `ccache` integration with PCH sloppiness flags lands across all paths.

Engine initialization state is now correctly split: `is_engine_configured()` covers whether `Init()` has completed and managers are ready; `is_running()` covers active processing. These were conflated before.

CI switches to Arch Linux for both dev-release and standard builds, aligning the CI baseline with the Steam Deck deployment environment. Dev releases move to a nightly schedule. The `MAYAFLUX_CONFIG_OVERRIDE` compile flag and `--config-override` launcher argument allow config injection without rebuilding. Shader sources are exposed in the Visual Studio Solution Explorer via the `compile_shaders` target. `reconfigure` target and `launch.bat` generation land for Windows developer ergonomics.

---

## Lila and MayaFluxHost

`MayaFluxHost` is extracted as a standalone shared library with its own CMake target, independent of `MayaFluxLib`. This is a significant boundary. Any C++ project can now link `MayaFluxHost` and gain a complete live C++23 evaluation environment backed by LLVM ORC JIT with Clang's `IncrementalCompilerBuilder`, with no dependency on Vulkan, audio backends, or any other MayaFlux system. The only hard dependencies are LLVM/Clang, `magic_enum` for enum introspection, and `nlohmann/json` for structured serialization. `Transitive` and `Lila` come transitively.

`MayaFlux::Host::attach_lila` enables in-process JIT attachment for embedding workflows. `skip_host_library_load` allows headless initialization. The Runtime API exposes JIT environment configuration externally.

`LiveArena` integrates into `Persist` and gains automatic exposure of Creator objects: any object constructed via the Vega factory API is immediately available to subsequent JIT compilations via `live_cast<T>(key)` without explicit registration. `live_cast_impl` routes through the DLL export table, resolving the Windows cross-boundary symbol resolution that was causing JIT crashes. `Transitive` is split into `Transitive_MD` and `Transitive_MT` OBJECT targets to resolve the MSVC CRT mismatch between `MayaFluxHost` and `Lila`.

---

## GPU-resident compute pipeline

`TextureExecutionContext` gains `OutputMode::IMAGE`: the output storage image stays GPU-resident after dispatch with no CPU download. `get_output_image(0)` retrieves the `VKImage` for direct use as a render texture or as input to a subsequent dispatch. This enables fully GPU-side texture production pipelines where the CPU dispatches, polls a `FenceID`, and binds the result, never touching the pixel data.

The pattern from `TextureExecutionContext` is uniform across the GPU compute layer. `ShaderExecutionContext::dispatch_async` returns a `FenceID`; `ShaderFoundry::is_fence_signaled` polls it; the calling code responds to completion. `GpuComputeNode` wraps this pattern for the node graph: it owns a `ShaderExecutionContext`, dispatches asynchronously via `dispatch_async`, polls the returned fence on each `compute_frame()` call, and fires `on_complete` callbacks with a `GpuComputeContext` once signaled.

`TextureExecutionContext` also gains `stage_image` for callers holding a live `VKImage` directly (render-pass output, camera frames, swapchain readback), bypassing the container resolution path. `set_input_layer` selects among array layers before dispatch. `aux_bindings` allows additional buffer bindings alongside the image.

`ComputeMeshBuffer` and `SDFMeshProcessor` apply the same fence-polling pattern to geometry: the SDF field is evaluated on the GPU and marching cubes runs as a compute shader, producing a triangle mesh in a GPU-side SSBO without any CPU-side isosurface traversal. `set_dirty()` triggers a new dispatch on the next cycle. `set_iso_level` and the field processor's runtime parameters (`set_time`, `set_param0`, `set_param1`) update push constants on the next dispatch. Multiple named SDF field shader variants ship in `shaders/sdf/`.

`InstanceNetwork` and `InstanceNetworkBuffer` enable GPU-driven instanced rendering. Each slot holds a `GeometryWriterNode` and a `mat4` transform. `InstanceSSBOProcessor` packs all transforms into an SSBO per cycle for a single `vkCmdDrawIndexed` call. `InstanceFieldOperator` supports an optional GPU path via `ShaderExecutionContext` that computes all transforms on the GPU and writes back through `on_complete`, eliminating per-slot CPU transform evaluation entirely.

All shaders are at GLSL 460 and SPIR-V 1.6. `shaderc` gains `FileIncluder` support for `#include` directives. The bitonic sort replaces odd-even in `sort_by_attribute.comp`. `noise.glsl` and `domain_warp.glsl` are available as includes.

---

## Portal::Forma

Forma is a signal-driven geometry system for interactive surfaces. Every element is a typed state mapped through a geometry function to a vertex stream. There is no retained widget tree, no layout engine, no panel hierarchy.

`Surface` is the top-level composition unit: a `Window`, a `Layer`, and a `Context`. `Portal::Forma::create_surface` is the primary entry point. `Layer` is a flat registry of `Element` instances in NDC space with hit-test predicates and optional parent-child cascade via `relate`. `Context` is the event router: `on_press`, `on_release`, `on_drag`, `on_scroll`, `on_move`, `on_enter`, `on_leave`, keyboard focus with `on_held` for repeat-driven interaction, `focused()` and `clear_focus()`.

`FormaBuffer` is the GPU vertex buffer for a single element, rewritten each cycle by the element's geometry function. `Mapped<T>` is the complete element descriptor: a `MappedState<T>` plus an `Element`. `create_element<T>` constructs the buffer, wires the geometry function, runs one `sync()` so bounds and containment are live before the first frame, and registers with `Bridge`.

The geometry primitive library in `Portal::Forma::Geometry` covers: `horizontal_fader`, `vertical_fader`, `stroke_slider` (arc-length scrubber along any polyline), `toggle`, `level_meter`, `radial`, `point`, `crosshair`, `position_picker`, `drawable_canvas`. `Kinesis::Geometry2D` provides lower-level filled and outlined shapes for static geometry: `filled_rect`, `filled_rounded_rect`, `filled_circle`, `filled_ring`, `filled_arc`, `filled_polygon`, `rect_outline`, `circle_outline`, `polyline`, and `arc_path` for path sampling.

`Bridge` is the application-wide two-way binding orchestrator. Inbound paths drive `MappedState` values each frame: `bind(state, node, projection)` reads a node's last output via a `GraphicsRoutine`; `bind(state, callable)` calls a `std::function<float()>`. Outbound paths route element values: `write(state, constant_node)`, `write(state, shader_processor, offset)` for push constants, `write(state, buffer, shader_path, descriptor_name)` for descriptor bindings, `write(state, audio_write_proc)` for audio routing, `write(state, span_sink)` for arbitrary bulk float consumers including IIR coefficient arrays and wavetable buffers. `wire_canvas_drag` wires a `drawable_canvas` element's full interaction in one call, including gap interpolation under fast drag.

`Collapsible` is a foldable header strip built on `FormaBuffer` and `Layer`. `LayoutCursor` accumulates vertical NDC position across element placement calls. `Link` drives repeating update loops for live-rerendering text rows.

`Inspector` provides live views into `NodeGraphManager` and `BufferManager` as Forma surfaces. `inspect_node_graph()` and `inspect_buffer_manager()` create or show dedicated inspection windows. `QueryUtils` provides `ValueSpec`, `ValueRow`, `ValueGroup`, and `InspectResult` for building structured inspector panels from arbitrary reader functions. `make_value_row` drives periodic `repress` updates on text images through `Link::tap()`.

`Portal::Forma::initialize()` stores engine-level references for all subsequent factory calls, matching the initialization contracts of `Portal::Text` and `Portal::Graphics`.

---

## Portal::Forma::Plot

`PlotContainer` is a `SignalSourceContainer` holding named scalar series, each bound to a data acquisition path at construction time and updated by `process_default()`. `Plot::source()` builds a container incrementally: `.as(name, count, role, modality).from(node)`, `.from(callable)`, or `.from(audio_buf)`.

`Plot::series()` describes the geometry: `.x()` and `.y()` accumulate axis role mappings with `AxisRange` (fixed, auto-scale, or condition-gated). `.as_waveform()`, `.as_scatter()`, `.as_bars()`, `.as_filled_waveform()` terminate into a `SeriesSpec` carrying the geometry function, topology, and buffer capacity arithmetic. Multiple `.x()` and `.y()` calls pair by mapping index, enabling multiple scatter clouds or phase portraits in a single draw call. The palette cycles across all matching series. `.background()`, `.bounds()`, `.y_ticks()`, `.x_ticks()`, `.legend()`, and `.label()` add adornments materialized automatically when using `Portal::Forma::plot()`.

`Portal::Forma::plot(title, width, height, container, spec)` is the single-call entry point: creates the window, surface, buffer, and first sync, returning the `Mapped` and `Surface`. `Plot::place()` places onto an existing surface. `Plot::series_by_role()` exposes the raw series lookup for custom geometry lambdas.

---

## Viewport navigation

Four navigation controllers are available as one-call bindings, each wiring event handlers and driving `set_view_transform_source` on any `RenderProcessor`:

`bind_fly_preset`: first-person WASD/QE translation with right-mouse-drag yaw/pitch, scroll dolly, and numpad ortho snaps. Remappable via `FlyKeyMap`.

`bind_orbit_preset`: focal-point orbit. MMB drag rotates around the focal point; Shift+MMB pans it. Scroll dollies the distance. Ortho snap slots match Blender conventions. `OrbitConfig` tunes distance, elevation, sensitivity, and depth range.

`bind_pan_zoom_preset`: 2D orthographic pan and zoom. Drag translates; scroll scales the orthographic half-height. View matrix remains identity throughout.

`bind_screenspace_preset`: translates the eye along the camera's local right and up axes without modifying rotation, the natural mode for 2D canvas workflows with perspective preserved.

All four share `unbind_viewport_preset` for teardown and can be swapped at runtime without touching the rendering pipeline. Per-processor and per-window overloads are provided for all four modes.

`set_view_transform_source` accepts any callable, including closures over node outputs, `NavigationState`, or Nexus entity positions, making the viewpoint a live signal in the data graph.

---

## Nexus

Conventional spatial simulation systems give you an actor hierarchy, a scene graph, a world object, a camera type, a light type. Nexus gives you none of those, which means you are not constrained to build the things they were designed for. Instead: `Fabric` is a commit-driven simulation container whose entities perceive their spatial neighborhood and push influence in the same step. `Emitter` fires influence without perceiving. `Sensor` perceives without acting. `Agent` does both, and what "perceiving" and "acting" mean is entirely user-defined. These are not archetypes, they are roles. The same entity can drive a render processor's view transform, modulate an audio sink's amplitude, and update a shader push constant in the same commit cycle.

`Locus` is an `Agent` whose position tracks a `NavigationState`, making the camera a spatial entity that participates in proximity queries, gets detected by `Sensor` neighbors, and can trigger `Expanse` crossings. `Presence` adds distance-attenuated influence via `falloff_curve` and `falloff_radius`, giving any entity a soft spatial field without specifying what that field does to its targets. `Tapestry` manages a collection of `Fabric` instances and owns named `Expanse`s that span multiple fabrics. An `Expanse` is a named spatial region with a `ContainsFn` predicate and `on_enter`/`on_exit` crossing callbacks evaluated on each commit, usable for anything from parameter zone triggers to game region detection to reactive audio spaces.

`StateEncoder`/`StateDecoder` reach schema v5: five RGBA32F EXR rows per entity covering position/intensity, color/size, radius/query radius/type, trigger/timing/sink bits, and Locus navigation parameters. The EXR format is intentional: spatial state is image data, addressable, compositable, and readable by anything that reads images. `user_state` passthrough allows application-defined data to survive round-trips. `StateDecoder::reconstruct` constructs entities from schema records without them needing to exist in the live `Fabric`, enabling cold-start session reconstruction from a saved performance state.

`Wiring` gains `mouse_drag`, `on_scroll`, `on_release`, and `key_held` trigger variants. The Fabric function registry maps string names to `InfluenceFn`, `ContainsFn`, and crossing functions, enabling `StateDecoder` to resolve callable names back to live function objects across sessions.

---

## Cross-domain scheduling

`CrossRoutine` is the dual-clock coroutine. It lives in the `MULTI_RATE` task list scanned by both the sample-clock pump on the audio thread and the frame-clock pump on the graphics thread. `MultiRateDelay` arms both clocks simultaneously; a zero count on one axis disarms that clock for the current suspension. A compare-exchange on `active_delay_context` ensures exactly one thread resumes the handle when both pumps reach the gate. Added with `initialize=false`; `Fabric::use(CrossFactory)` handles initialization automatically for Nexus-driven workflows.

`FreeRoutine` is backed by `conditional_promise` and resumed by a dedicated `CONDITIONAL` scheduler thread evaluating a stored predicate on each iteration. No clock, no polling cadence, arbitrary condition.

`BroadcastSource<T>` and `BroadcastAwaiter<T>` provide coroutine-native one-to-many event broadcast. `audio_output_tick()` is a `BroadcastSource` factory that fires on every completed audio output cycle, letting any coroutine await the output tick the same way it awaits any other event. `AudioOutputContainer` wraps the live engine output as a first-class `DynamicSoundStream`, making the engine's own output tap-able for metering, recording, and visualization without a separate capture thread.

`ProcessingToken` is propagated through `EventChain`, `Timer`, `TemporalActivation`, `metro`, `sequence`, `pattern`, `line`, and `Gate`.

---

## Write and capture

The read/write axis is now symmetric. Every major data type MayaFlux can ingest can be exported.

**Images**: `ImageWriterRegistry` dispatches by file extension to registered writer factories. `STBImageWriter` covers PNG, JPG, JPEG, BMP, TGA, and HDR via stb_image_write. `EXRWriter` covers full-precision float export via vendored tinyexr. `IOManager::save_image` has four overloads: from a live `VKImage` (GPU download async, encode dispatched to worker thread), from a `TextureBuffer`, from a `TextBuffer` (GPU text-rendered images), and from a pre-downloaded `ImageData`. `wait_for_pending_saves()` provides explicit flush semantics.

**Video**: `VideoFileWriter` is an asynchronous encoder with a lock-free SPSC work queue and a dedicated worker thread that owns all FFmpeg state. It accepts five input paths: the `DisplayService` frame observer for live swapchain capture (encoder opened lazily on the first delivered frame, dimensions taken from the live swapchain), raw `uint8_t` pixel spans, `TextureContainer` pixel bytes, `VideoStreamContainer`/`CameraContainer` frames, and `TextureBuffer` with async GPU download fallback. `SwsContext` handles dimension and format rescaling internally. `IOManager::capture_window` wraps this as a single call returning an opaque handle. `stop_capture` finalizes the container and flushes the worker. `create_writer` provides direct encoder access for custom frame submission workflows.

**Audio**: `SoundFileWriter` encodes to any FFmpeg-supported container from interleaved `span<double>`, planar `vector<DataVariant>`, `AudioBuffer`, or `SoundStreamContainer`, on a worker thread via lock-free queue. `IOManager::capture_output` registers with `AudioBackendService`'s output observer to tap the engine's live output continuously into a `SoundFileWriter`. `AudioOutputContainer` is the container counterpart: a `DynamicSoundStream` subclass whose write head advances each output cycle via `AudioOutputAccessProcessor`, making the engine output addressable as a first-class Kakshya container for any downstream consumer.

---

## IO introspection

`IOManager` gains reader introspection APIs: `get_video_reader_ids()`, `get_video_reader(id)`, `get_camera_reader_ids()`, `get_camera_reader(id)`, `get_audio_readers()`, `get_video_writers()`, `get_video_capture_ids()`. `NodeGraphManager`, `BufferManager`, `InputManager`, and `TaskScheduler` gain parallel introspection APIs used by the Forma Inspector subsystem and accessible from Lila sessions.

---

## Breaking changes

`DataWriteProcessor` replaces both `GeometryWriteProcessor` and `TextureWriteProcessor`. The distinction was architectural noise; `DataWriteProcessor` accepts `NDData`-typed payloads from `Bridge` outbound paths and stages them modality-aware into any `VKBuffer` target. Call sites in `RenderSink`, `Bridge`, and any user code routing element values to geometry or texture buffers must migrate to `DataWriteProcessor`.

`PointNode` is removed. `PointCollectionNode` is the replacement throughout.

Native windowing is the default on Linux and Windows. Code depending on GLFW window handles directly, or on GLFW event callbacks, must be reviewed. `MAYAFLUX_LINUX_USE_GLFW` and `GLFW_BACKEND` restore the GLFW path.

`ComputeOutNode` is removed. `GpuComputeNode` is the replacement for arbitrary GPU dispatch in the node graph. `ComputeMeshBuffer` with `SDFMeshProcessor` is the path for GPU marching cubes.

The `ViewTransform` is a UBO at `set=0, binding=0`. This was established in 0.3.3 but any shaders still using the push constant path for `ViewTransform` will not compile against 0.4 descriptors.

---

**Source**: [github.com/MayaFlux/MayaFlux](https://github.com/MayaFlux/MayaFlux)
**Docs**: [mayaflux.org](https://mayaflux.org)
**License**: GPLv3
