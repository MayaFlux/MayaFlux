**Version**: 0.2.0  
**Commit**: {{COMMIT_SHA}}

---

0.1.0 established the core audio architecture, Vulkan 1.3 dynamic rendering, node-to-GPU bindings, and the Lila JIT environment. 0.2.0 expands on all fronts.

The two primary focus areas of 0.2.0 are input/IO and physical modelling, alongside API stabilisation, breaking renames, and structural refactors across the codebase.

The live signal matrix completes: audio files, video files, live camera devices, and image assets all route through a single `IOManager` into the same buffer and processor architecture. MIDI and HID input become first-class node graph signal sources. Per-window keyboard and mouse input handling lands as a proper API. Physical modelling synthesis expands substantially with three network types covering modal, waveguide, and resonator synthesis. The graphics pipeline gains operator-driven geometry networks: topology generators with proximity graph algorithms, parametric path generators with multiple interpolation modes, and the Kinesis spatial generation and mathematical substrate. The coroutine scheduler gains `EventChain` as its primary temporal sequencing primitive.

---

## Audio

- `ModalNetwork`: exciter system, spatial excitation, modal coupling between adjacent modes
- `WaveguideNetwork`: unidirectional and bidirectional propagation, explicit boundary reflections, measurement mode
- `ResonatorNetwork`: IIR biquad bandpass filter network, formant presets, per-resonator or shared excitation
- `NetworkAudioBuffer` and `NetworkAudioProcessor` for node network audio capture
- `StreamReaderNode` and `NodeFeedProcessor` for buffer-to-node streaming
- Channel routing with block-based crossfade transitions
- `Constant` generator node
- `CompositeOpNode` for N-ary node combination
- `ChainNode` pipeline and `>>` graph operator
- `NodeConfig` ownership moved to Engine, propagated to `NodeGraphManager`
- JACK and PipeWire buffer size handling and API preference

## Graphics

- `TopologyGeneratorNode`: proximity graph generation (minimum spanning tree, k-nearest, nearest neighbor, sequential)
- `PathGeneratorNode`: parametric curve generation (Catmull-Rom, B-spline, linear), temp points, delayed resolution
- `PhysicsOperator` (N-body), `PathOperator`, `TopologyOperator`: operator-driven geometry network architecture
- `PointCloudNetwork` as operator-driven structural spatial network
- `CompositeGeometryBuffer` and `CompositeGeometryProcessor`
- `ViewTransform` push-constant helper and depth-enabled pipeline support
- Per-window depth image management and depth attachment propagation
- Per-window keyboard and mouse input handling via `on_key_pressed`, `on_mouse_pressed` and related helpers
- `EventFilter` for granular per-window GLFW event filtering
- Configurable blend and depth-stencil state on `RenderProcessor`
- `BackendResourceManager` for centralised format traits and swapchain readback
- macOS line rendering fallback via CPU-side quad expansion
- Mesh shader infrastructure present, paused pending MoltenVK upstream merge
- Compute shader infrastructure: `ComputePress` dispatch, dedicated compute command pool

## IO and Containers

- `IOManager`: unified interface for audio files, video files, live camera, and image assets
- `CameraContainer` and `CameraReader`: live camera input on Linux (`/dev/video0`), macOS (AVFoundation index), Windows (DirectShow filter name)
- Background decode thread on camera; graphics thread never blocked by device I/O
- `VideoFileContainer`, `VideoStreamContainer`, `VideoContainerBuffer`, `VideoStreamReader`
- HEVC frame rate detection, decode loop error handling, black frame elimination
- `FFmpegDemuxContext`, `AudioStreamContext`, `VideoStreamContext` as RAII FFmpeg resource owners
- `FrameAccessProcessor`, `SpatialRegionProcessor` (parallel spatial region extraction)
- `RegionOrganizationProcessor`, `DynamicRegionProcessor`
- `WindowContainer` and `WindowAccessProcessor` for pixel readback from GLFW windows
- `ImageReader`: PNG, JPG, BMP, TGA with auto RGB-to-RGBA conversion
- `SoundContainerBuffer`, `SoundStreamReader`, `SoundStreamWriter` (renamed from 0.1)
- `ContiguousAccessProcessor` for audio container access
- `TransferProcessor` for bidirectional audio/GPU buffer transfer
- `BufferDownloadProcessor`, `BufferUploadProcessor` for staging

## Coroutines

- `EventChain` as the primary temporal sequencing primitive: `.then()`, `.repeat()`, `.times()`, `.wait()`, `.every()`
- `Kriya::Gate`, `Kriya::Trigger` for condition-driven and signal-driven suspension
- `MultiRateDelay` awaiter for cross-domain coordination
- `EventFilter` for granular GLFW windowing event filtering
- `Temporal` proxy DSL replacing `NodeTimeSpec`
- `TemporalActivation` replacing `NodeTimer`
- `schedule_metro`, `create_event_chain` convenience wrappers
- `BufferPipeline` for coroutine-driven buffer operation chains

## Input

- `InputSubsystem` coordinating `HIDBackend` (HIDAPI) and `MIDIBackend` (RtMidi)
- `InputManager` async dispatch to `InputNode` instances
- Lock-free registration list; platform-specific hazard pointer path on macOS
- `HIDNode` for parsing HID reports into node signals
- `MIDINode` for MIDI event routing into the node graph
- Per-value smoothing modes on `InputNode`: linear, exponential, slew-limited
- Callback-driven events: `on_value_change`, `on_threshold_rising`, `on_button_press`, `while_in_range`

## Kinesis

- Unified stochastic generation infrastructure (`Kinesis::Stochastic`)
- `VertexSampler` for spatial generation: Lissajous, Fibonacci sphere, torus distributions
- Proximity graph generation extracted to `Kinesis`
- Geometric primitive generation and transformation utilities
- Eigen-to-NDData semantic conversion (`EigenInsertion`, `EigenAccess`)
- `SCALAR_F32` modality and refined vertex layouts

## Breaking Changes

- `ContainerBuffer` renamed to `SoundContainerBuffer`
- `ContainerToBufferAdapter` renamed to `SoundStreamReader`
- `StreamWriteProcessor` renamed to `SoundStreamWriter`
- `FileBridgeBuffer` removed; `SoundFileBridge` is the replacement: a single buffer with a new processing chain, no inherited specialized processors
- Processing chains gain explicit `preprocess` and `postprocess` slots
- `NodeTimer` replaced by `TemporalActivation`
- `NodeTimeSpec` replaced by `Temporal` proxy DSL
- `OutputTerminal` removed
- Sequence API removed; `EventChain` supersedes it
- `set_target_window` now requires buffer parameter

---

**Source**: [github.com/MayaFlux/MayaFlux](https://github.com/MayaFlux/MayaFlux)  
**Docs**: [mayaflux.org](https://mayaflux.org)  
**License**: GPLv3
