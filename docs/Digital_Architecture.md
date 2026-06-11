# MayaFlux: The Computational Substrate

---

## Everything Is Numbers

This is not a slogan. It is the architectural load-bearing wall.

An audio sample at 48 kHz is a double. A vertex position is three doubles. A pixel color is four doubles packed into an integer. A control signal, a physics force, a texture coordinate, a MIDI velocity: all of them, when the hardware and the OS and the driver have finished their ceremony, are numbers in memory. The boundaries you learned in other tools were never properties of the data. They were properties of the tools, built at a time when the alternative was infeasible.

MayaFlux removes those boundaries at the architectural level, not by providing conversion utilities but by never drawing the lines in the first place.

Domain is a routing annotation applied at the last moment, not a property of the data itself. A `NodeBuffer` driving a sine node into a speaker and a `GeometryBuffer` driving vertex positions to the GPU are the same kind of thing, driven by the same scheduler, described by the same primitives. What differs is which subsystem consumes them.

The architecture that follows emerges from that premise.

---

## The Real-Time Core: Nodes, Buffers, Vruta, Kriya

These four namespaces form the real-time backbone. They are what runs when the audio callback fires, when the GPU frame begins, when the network packet arrives. Everything else either feeds into them or operates independently of their timing constraints.

### Nodes

A node is a moment of transformation. It takes one unit of data and produces one unit of data, evaluated as many times per second as its domain requires. There is no implicit audio-ness or visual-ness to a node. A node that computes `sin(phase)` is equally valid as a 440 Hz oscillator, the x-coordinate of a moving point, or a brightness modulator. The node does not know which it is. The connection that routes its output decides.

The node taxonomy reflects what kinds of transformation exist, not what domain they belong to.

**Generators** produce values from internal state: `Sine`, `Phasor`, `Impulse`, `Polynomial`, `Logic`, `Random`, `WindowGenerator`. A phasor is a ramp from 0 to 1 that repeats. That ramp can drive audio synthesis, animation, or shader parameters without modification.

**Filters** reshape incoming streams: `FIR`, `IIR`. Coefficients define the response. The filter does not care whether the stream is audio or control.

**Conduit** nodes route and combine: `NodeChain`, `NodeCombine`, `Constant`, `StreamReaderNode`. These are the plumbing layer, connecting transformation stages without introducing computation of their own.

**Input** nodes bridge hardware events into the graph: `HIDNode`, `MIDINode`, `OSCNode`. A MIDI note becomes a frequency value; a HID axis becomes a double. From that point it is just a number.

**GpuSync** nodes coordinate CPU-side node graph evaluation with asynchronous GPU compute dispatches. `GpuComputeNode` owns a `ShaderExecutionContext`, dispatches it asynchronously via `dispatch_async`, polls the returned `FenceID` on each subsequent `compute_frame()` call, and fires `on_complete` callbacks with a `GpuComputeContext` once the fence signals. The node carries no prescriptions about what consumers do with the result. `GeometryWriterNode` is the base for nodes that produce vertex geometry for the GPU pipeline.

**Graphics** nodes write computed values into GPU resources: geometry writers, mesh writers, topology generators, path generators, texture nodes, procedural texture nodes, point collection nodes. These are where numeric streams cross into the visual domain by routing them to a buffer that a render pipeline consumes.

**NodeNetworks** coordinate collections of nodes whose relationships define the output.

`ParticleNetwork` models entities with position, velocity, and force. Physics integrates forward each frame. The positions are numbers that flow wherever you connect them.

`PointCloudNetwork` holds spatial samples with no identity or persistence. Structure emerges through attached operators: `TopologyOperator` infers connectivity by k-nearest or radius or Delaunay; `PathOperator` interpolates curves through control points.

`ModalNetwork` decomposes resonance into independent modes, each a decaying oscillator with a frequency ratio, a decay coefficient, and an amplitude. Excite the network with an impulse and the modes respond according to their spectrum.

`WaveguideNetwork` simulates wave propagation through delay lines. Two modes: unidirectional for string-like structures; bidirectional for tube-like structures where two rails travel in opposite directions with reflection at each termination.

`ResonatorNetwork` applies IIR biquad bandpass sections to whatever signal enters. Feed it noise and it becomes a formant synthesizer; feed it a pulse and it voices a resonant space.

`MeshNetwork` operates on mesh topology as a first-class computational structure.

`InstanceNetwork` is a flat peer collection of `GeometrySlot` entries for GPU instanced rendering. All slots may hold the same node (shared template geometry) or distinct nodes. `process_batch()` runs the operator chain then drives `compute_frame()` on each slot's node. The buffer layer reads the slot list each cycle and packs per-instance transforms into an SSBO for a single instanced draw call. `InstanceFieldOperator` binds per-slot transform or position fields and supports an optional GPU path via `ShaderExecutionContext` that computes all instance transforms on the GPU and writes them back via an `on_complete` callback.

All networks accept `NetworkOperator` instances that define behavioral layers. Operators compose: `FieldOperator`, `MeshFieldOperator`, `PhysicsOperator`, `MeshTransformOperator`, `GraphicsOperator`, `InstanceFieldOperator`, `OperatorChain`.

### Buffers

If a node is a moment of transformation, a buffer is a span of time held in one place until something can be done with it.

A node produces one value per evaluation. A buffer accumulates those values over a cycle, holds them, and makes them available as a block. Buffers are cycle-driven, carry processing tokens, and are registered with a `BufferManager` that knows when to process them.

**Audio buffers** accumulate double-precision samples for the audio subsystem. `AudioBuffer` is the base. `NodeBuffer` is an audio buffer whose data source is a node. `FeedbackBuffer` adds a `HistoryBuffer` backed by a ring buffer for delay-line and recursive signal paths. `InputAudioBuffer` captures from hardware input.

**Geometry buffers** accumulate vertex data for the graphics pipeline: `GeometryBuffer`, `MeshBuffer`, `CompositeGeometryBuffer`.

**Network buffers** bridge NodeNetworks to subsystem outputs: `NetworkGeometryBuffer`, `MeshNetworkBuffer`, `NetworkTextureBuffer`, `NetworkAudioBuffer`. `InstanceNetworkBuffer` renders an `InstanceNetwork` as a single instanced draw call; template geometry from slot 0 is uploaded once and per-slot transforms are packed into an SSBO each cycle by `InstanceSSBOProcessor`.

**Texture buffers** hold image data flowing to GPU image resources: `TextureBuffer`, `NodeTextureBuffer`.

**Container buffers** bridge Kakshya containers into the real-time cycle: `SoundContainerBuffer`, `VideoContainerBuffer`.

**Forma buffers** serve the Portal::Forma surface system. `FormaBuffer` is a GPU vertex buffer whose contents are rewritten each cycle by a geometry function parameterized on a typed state value. It is the render-side counterpart to `MappedState<T>`.

**Root buffers** aggregate child buffers and present a single output to a subsystem. `RootAudioBuffer` mixes its children into the audio callback output. `RootGraphicsBuffer` aggregates geometry for the frame.

### Processors

Processing happens to buffers, not inside them. Every buffer carries a processing chain; processors attach to that chain and execute in order when the buffer's cycle fires.

`NodeFeedProcessor` evaluates a node unit by unit until the buffer is full. `FilterProcessor` applies IIR or FIR coefficients. `LogicProcessor` applies boolean operations. `PolynomialProcessor` applies a user-supplied polynomial function with optional recursive mode. `NodeBindingsProcessor` binds node outputs to GPU descriptor slots. `AggregateBindingsProcessor` combines multiple node outputs into a single descriptor update.

`DataWriteProcessor` is a modality-aware write processor for `VKBuffer` targets. It accepts `NDData`-typed payloads from `Bridge` outbound paths and stages them into the buffer's pipeline context each cycle, serving as the connective tissue between Forma element values and arbitrary GPU-side consumers.

`FormaBindingsProcessor` handles descriptor binding updates for Forma elements. It is constructed by `Bridge::write()` when an element value needs to drive a descriptor slot; it writes into the buffer's pipeline context staging rather than targeting an external `ShaderProcessor`.

On the graphics side: `GeometryBindingsProcessor`, `MeshProcessor`, `RenderProcessor`, `ComputeProcessor`, `DescriptorBindingsProcessor`, `UVFieldProcessor`. `InstanceSSBOProcessor` packs per-slot `mat4` transforms from an `InstanceNetwork` into an SSBO binding each cycle.

`MixProcessor`, `GraphicsBatchProcessor`, `PresentProcessor` handle aggregation and submission.

For data movement between CPU and GPU: `BufferUploadProcessor`, `BufferDownloadProcessor`, `TransferProcessor`.

For Kakshya container access: `AccessProcessor` variants, `StreamSliceProcessor`.

For NDData to audio: `AudioWriteProcessor`. For NDData to graphics: `GeometryWriteProcessor`, `TextureWriteProcessor`.

`SDFMeshProcessor` evaluates signed-distance fields on the GPU and runs marching cubes to produce mesh geometry; it operates on a `ComputeMeshBuffer` that carries both SDF input and the resulting triangle data.

`ComputeMeshBuffer` is a `VKBuffer` specialization for GPU-side SDF and mesh compute workflows. It owns the SSBO holding SDF samples and the vertex output from marching cubes, driven by `SDFMeshProcessor` via async compute dispatch.

### Vruta

Vruta is the execution model. It defines what time-structured work is in MayaFlux.

Four coroutine types cover the space of real-time work:

`SoundRoutine` ticks at audio rate and is resumed by the sample-clock pump on the audio thread. Suspend with `SampleDelay` or `BufferDelay`.

`GraphicsRoutine` ticks at frame rate and is resumed by the frame-clock pump on the graphics thread. Suspend with `FrameDelay`.

`CrossRoutine` lives in the `MULTI_RATE` task list, which both pumps scan. It suspends on `MultiRateDelay`, which arms both clocks simultaneously. A zero count on one clock disarms that clock for the current suspension. When both pumps reach the gate concurrently, a compare-exchange on `active_delay_context` in the promise ensures exactly one thread resumes the handle. `CrossRoutine` is added with `initialize=false`; the `Fabric::use(CrossFactory)` path handles initialization automatically.

`FreeRoutine` (backed by `conditional_promise`) is resumed by a dedicated `CONDITIONAL` scheduler thread that evaluates a stored predicate on each iteration. It carries no clock; it simply waits for an arbitrary condition to become true.

`DelayContext` distinguishes buffer-based and sample-based timing regimes. `Event` and `EventManager` hold named coroutines that respond to conditions. `EventSource` and `NetworkSource` are the sources events wait on. `Clock` tracks elapsed time in samples and converts between sample counts and seconds.

### Kriya

Kriya is the vocabulary for writing time-structured processing work against Vruta's runtime.

The awaiters `SampleDelay`, `BufferDelay`, `FrameDelay`, and `MultiRateDelay` express time in terms the system understands. `ProcessingGate` suspends until a condition becomes true. `EventAwaiter` and `NetworkAwaiter` suspend until an event or message fires.

`Tasks` provides common scheduling primitives: `metro` fires a callback at a regular interval with sample-accurate timing; `sequence` fires a list of callbacks at specified time offsets relative to start; `line` interpolates a float between two values over a duration; `pattern` calls a generator function with an incrementing step index at a regular interval, the natural primitive for algorithmic generation.

`BufferPipeline` is the primary composition surface for sequences of buffer operations as coroutine-driven chains. `SamplingPipeline` specializes this for polyphonic stream playback.

`Chain` (the `EventChain`) sequences discrete actions with sample-accurate delays via `then()`, `wait()`, `every()`, `repeat()`, `times()`.

`CycleCoordinator` synchronizes multiple buffer pipelines to a common cycle count. `Timer` and `TimedAction` provide fire-once and fire-with-cleanup scheduling.

Input events enter the scheduling system through `InputEvents`: `any_key`, `mouse_pressed`, `mouse_released`, `mouse_move`. Network events enter through `NetworkEvents`: `on_message`, `on_message_from`, `on_message_matching`.

---

## Kakshya: The Data Layer

Kakshya is not real-time. This is intentional and important.

Kakshya is the repository and classification layer for data at rest. It holds data, describes data, provides zero-cost views into data, and processes data on demand. It has no scheduler dependency. Its processors are pull-based, not tick-driven.

At the center is `SignalSourceContainer`, the base for all typed containers. Concrete types cover the full range of digital media:

`SoundFileContainer` holds decoded audio from a file. `DynamicSoundStream` holds a bounded stream that can grow. `SoundStreamContainer` wraps a live audio stream. `VideoFileContainer` holds decoded video frames. `VideoStreamContainer` wraps a live video feed. `CameraContainer` wraps camera capture. `TextureContainer` holds image data with pixel format and dimensions. `WindowContainer` wraps a windowing surface and exposes each completed rendered frame as addressable NDData via `WindowAccessProcessor`. `AudioOutputContainer` is a `DynamicSoundStream` subclass wrapping the live engine audio output; each completed output cycle is written by `AudioOutputAccessProcessor` into `m_processed_data` and accumulated in `m_data`, making the engine's output tap-able as a first-class container for metering, recording, and visualization. `PlotContainer` holds time-series data for the Portal::Forma plot subsystem; its write head advances atomically and readers hold independent cursors.

`NDData` and `DataVariant` are the carriers. `DataVariant` is the type-erased container for numeric data in motion. `NDData` provides structured N-dimensional access with typed interpretations: scalar, vec2, vec3, vec4, complex, mesh. `EigenAccess` and `EigenInsertion` bridge NDData to Eigen matrices. `MeshAccess`, `VertexAccess`, and `TextureAccess` bridge to their respective formats.

`Region` and `RegionGroup` are zero-cost markers over containers. A `Region` names a span: start frame, length, channel mask. It does not copy the data. `RegionGroup` organizes multiple regions with metadata. `RegionSegment` subdivides regions further. `OrganizedRegion` carries attribution.

`DataProcessingChain` applies `DataProcessor` instances on demand. The Processors subdirectory provides: `ContiguousAccessProcessor`, `CursorAccessProcessor`, `FrameAccessProcessor`, `WindowAccessProcessor`, `SpatialRegionProcessor`, `AudioOutputAccessProcessor`.

The relationship between Kakshya and Buffers is a one-way bridge. Buffers consume from Kakshya containers through container buffers; Kakshya never asks Buffers for anything.

---

## Yantra: The Offline Computation Universe

Yantra is what MayaFlux looks like when real-time constraints are removed: a complete offline computation environment with its own type system, operation taxonomy, composition mechanisms, and GPU execution layer.

The type system begins with `Datum<T>`. Where the real-time side works with raw doubles flowing through buffers, Yantra wraps everything in `Datum`. A `Datum` carries the data, optional container context, and metadata. `ComputeData` is the concept constraining what `T` can be. `StructureIntrospection` infers the shape of any `ComputeData` value.

**Analyzers** extract information: `EnergyAnalyzer`, `StatisticalAnalyzer`, `UniversalAnalyzer`, `GpuAnalyzer`.

**Transformers** modify data: `MathematicalTransformer`, `SpectralTransformer`, `TemporalTransformer`, `ConvolutionTransformer`, `UniversalTransformer`, `GpuTransformer`.

**Sorters** reorder data: `StandardSorter`, `UniversalSorter`, `GpuSorter`.

**Extractors** pull features: `FeatureExtractor`, `UniversalExtractor`, `GpuExtractor`.

**Executors** are the GPU execution layer. `GpuExecutionContext` manages the full lifecycle of a Vulkan compute dispatch: descriptor allocation, buffer staging, shader execution, result readback. `ShaderExecutionContext<>` is the templated dispatch type used by `GpuComputeNode` and `InstanceFieldOperator`'s GPU path; it carries input bindings, output bindings, and push constants, and exposes `dispatch_async` returning a `FenceID`. `TextureExecutionContext` specializes this for image shaders. `GpuDispatchCore` handles low-level dispatch mechanics. `GpuResourceManager` manages GPU-side buffer and image lifetimes across dispatches.

**Composition** is where the system becomes expressive beyond individual operations.

`ComputeMatrix` is a self-contained execution environment holding named operation instances, executed with configurable strategies: synchronous, asynchronous, parallel, or chained.

`ComputationGrammar` is a rule-based production system for operation selection. Rules carry matching logic, execution logic, priority, and metadata. When data arrives at a grammar-driven pipeline, matching rules are evaluated in priority order and the winning rule's executor runs.

`ComputationPipeline` composes operations in sequence with optional grammar-driven selection at each stage.

`GranularWorkflow` is a complete opinionated granular analysis and reconstruction implementation built on the grammar and matrix systems.

`OperationRegistry` maps operation types to factories for runtime discovery.

---

## The Visual Pipeline

The visual pipeline in MayaFlux is an independent, full-capability visual processing system sharing the same real-time infrastructure as audio: the same scheduler, the same buffer machinery, the same Kriya composition layer.

### Portal

Portal is the glue layer between MayaFlux's internal systems and external-facing capabilities requiring coordination across backends, resource lifetimes, and platform abstractions. It currently covers Graphics, Text, Forma, System, and Network.

**Portal::Graphics** is the Vulkan coordination layer.

`ShaderFoundry` manages shader compilation from SPIR-V, command buffer allocation and recording, descriptor set construction, and fence lifetime tracking. `is_fence_signaled` is polled by `GpuComputeNode` to detect async dispatch completion.

`RenderFlow` orchestrates graphics pipeline creation and draw command recording using Vulkan 1.3 dynamic rendering with no render pass objects.

`ComputePress` manages compute pipeline dispatch: pipeline creation, descriptor binding, push constant upload, and `vkCmdDispatch`.

`TextureLoom` creates and manages GPU textures: 2D, 3D, cubemaps, render targets.

`SamplerForge` manages sampler object lifecycle.

The descriptor layout contract with Portal is firm: `set=0, binding=0` is reserved for the `ViewTransform` UBO. User descriptors begin at `set=1`. Push constants are fully available.

**Portal::Text** is a complete text rendering pipeline: `FontFace` holds a loaded typeface; `GlyphAtlas` rasterizes and packs glyphs into a texture atlas; `TypeSetter` lays out text into a vertex stream; `InkPress` coordinates the full pipeline from string to rendered geometry; `TypeFaceFoundry` manages loaded faces.

**Portal::Forma** is the UI and surface orchestration system. It is not a widget hierarchy; it is a signal-driven geometry system where every interactive element is a typed state mapped through a geometry function to a vertex stream.

`FormaBuffer` is the GPU-side vertex buffer for a single element; its contents are rewritten each cycle by the element's geometry function applied to the current `MappedState<T>`.

`MappedState<T>` is the state atom for a Forma element. It holds the current typed value and an optional `bulk_reader` for vector-valued state. It is the currency passed through `Bridge` bindings.

`Mapped<T>` is the complete element descriptor: a `MappedState<T>` plus an `Element` containing id, buffer reference, bounds hint, contains predicate, and optional text overlay. Returned by `create_element`.

`Element` is the render-side record. It carries the `FormaBuffer`, spatial bounds, a hit-test predicate, an optional text label, and visibility state. `Layer` owns a collection of `Element` instances and handles relation (parent-child visibility), cascade, and bounds queries.

`Context` is the input-event dispatcher for a surface. It maps element ids to press, release, and drag callbacks, spawning `GraphicsRoutine` coroutines for each listener.

`Surface` is the top-level composition unit: a `Layer`, a `Context`, and a window reference. `Portal::Forma::create_surface` is the primary entry point.

`Bridge` is the two-way binding orchestrator for Forma elements. One Bridge instance serves the full application. Inbound paths drive element `MappedState` values each frame: `bind(id, Node)` reads a node's last output via a `GraphicsRoutine`; `bind(id, lambda)` calls a `std::function<float()>`. Outbound paths route element values each frame: `write(id, ShaderProcessor, offset)` stages into push constants; `write(id, target_buffer, shader_path, descriptor_name, ...)` attaches or reuses a `FormaBindingsProcessor` on the target buffer; `write(id, AudioWriteProcessor)` routes to audio; `write(id, DataWriteProcessor)` routes to a vertex buffer; `write(id, Constant)` updates a node graph constant; `write(id, sink)` routes to a caller-supplied span sink for coefficient arrays.

`Inspector` provides live views into the NodeGraphManager and BufferManager as Forma surfaces. `Portal::Forma::inspect_node_graph()` and `inspect_buffer_manager()` create or show dedicated inspection windows on first call.

`Plot` is the live data plotting subsystem inside Forma. `Plot::source()` constructs a `PlotContainer`; `Plot::series()` builds a `SeriesSpec` describing axis mapping and styling; `Forma::plot(title, width, height, container, spec)` constructs a full plot window and returns a `Mapped<shared_ptr<PlotContainer>>`. `Plot::place_label` and `Plot::place_rect` are lower-level placement helpers for custom layouts.

`Collapsible` is a foldable header strip primitive built on `FormaBuffer` and `Layer`. `make_collapsible` constructs one; `attach` relates body elements to it.

`LayoutCursor` is a simple NDC-space layout accumulator threading through element placement calls to advance vertical position.

`Portal::Forma::initialize` stores engine-level references (`BufferManager`, `TaskScheduler`, `EventManager`, `WindowManager`, `NodeGraphManager`) for all subsequent factory calls. The `create_element`, `create_buffer`, and `create_surface` free functions are the primary user-facing API.

**Portal::System** provides native OS integration.

`Portal::System::initialize()` initializes the system backend. `Portal::System::Dialog` exposes file chooser operations: `open_file(callback, filters, start_dir)` presents a native open-file dialog (XDG Portal on Linux, `COM IFileOpenDialog` on Windows, `NSOpenPanel` on macOS); `save_file` does the same for save. Templated `open_file<T>(on_success, on_error, ...)` overloads block until completion and return the result of applying `on_success` to the chosen path. The `Depot` API layer builds on these with typed convenience functions: `choose_audio`, `choose_video`, `choose_image`, `choose_mesh`, `choose_mesh_network`, `save_audio`, `save_image`.

**Portal::Network** provides the facade for network endpoint management. `MessageUtils` and `NetworkSink` handle message serialization and dispatch. `StreamForge` and `PacketFlow` manage TCP and UDP endpoint lifecycles.

### Registry

Registry and Portal are often confused because both relate to backend access. They are not the same thing at all.

`BackendRegistry` is a type-indexed map of factory functions. A backend registers a pointer to its service implementation under an interface type. A consumer queries by interface type and receives a pointer. Neither knows about the other. There is no abstraction over the service itself: the registry returns exactly what the backend registered. Its job is decoupling, not capability.

Portal builds complete, opinionated systems on top of whatever the registry returns. Portal knows what to do with a `DisplayService` or a `BufferService`; it constructs stateful facades with lifecycle, configuration, and rich API. A consumer using Portal never touches the registry. A consumer building new Portal-level systems queries the registry directly to get the backend handle they need.

Registry is the seam. Portal is the surface. The distinction matters because both are entry points but to completely different levels of the system.

### Core and Subsystems

`Engine` owns the component graph and manages the initialization sequence. `SubsystemManager` coordinates `ISubsystem` instances.

`AudioSubsystem` wraps the native audio backend (PipeWire on Linux, WASAPI on Windows, CoreAudio on macOS), registers callbacks, and drives the audio processing cycle.

`GraphicsSubsystem` wraps `VulkanBackend`, manages the swapchain lifecycle, and drives frame rendering.

`InputSubsystem` owns the native input backend and MIDI backend and delivers events to `InputManager`.

`NetworkSubsystem` owns TCP and UDP backends and registers a `NetworkService`.

Each subsystem receives a `SubsystemProcessingHandle` at initialization: a token-scoped handle that provides controlled access to Buffers, Nodes, and the task scheduler without exposing manager internals. Subsystems operate through their handle; they do not hold direct references to managers.

`BackendRegistry` sits across all of this as the service discovery mechanism. Backends write into it on startup; Portal and other consumers read from it. The registry is the reason Yantra can dispatch GPU compute without knowing which backend is running.

---

## Nexus: Spatial Entity Simulation

Nexus is the spatial simulation and orchestration layer. It connects to the real-time core through the buffer and rendering systems but does not depend on any of them for its own logic.

`Fabric` is the simulation container. `Fabric::commit()` runs one simulation step. On each commit, registered entities are evaluated: `Emitter` entities fire an influence function with an `InfluenceContext`; `Sensor` entities perceive their spatial neighborhood via a `PerceptionContext`; `Agent` entities both perceive and influence.

`Locus` is an `Agent` subclass that also carries a `NavigationState` for camera-like movement through the world. `Presence` is an `Agent` subclass with a `falloff_curve` and `falloff_radius` controlling how its influence attenuates over distance.

`Expanse` is a named spatial region carrying a `ContainsFn` predicate and optional `on_enter` and `on_exit` crossing callbacks. Expanses are registered on a `Fabric` and evaluated as part of each commit cycle. `Tapestry` manages a collection of Fabrics and owns named Expanses that can be active across multiple Fabrics.

`StateEncoder` serializes Fabric and Tapestry state to a paired EXR (numeric data) and JSON (schema) file set. `StateDecoder` patches or reconstructs entity state from that pair. The current schema is version 5, with an EXR layout of five RGBA32F rows per entity: position/intensity, color/size, radius/query_radius/type, trigger/timing/sink bits, and Locus navigation parameters. `StateDecoder::reconstruct` can construct missing entities from schema records rather than only patching existing ones.

`Wiring` is the builder interface for connecting entities to MayaFlux systems. The `on(window, key, bool held, on_release)` API attaches keyboard triggers; mouse drag support is available. `wire_player` is a free function that wires a Locus to standard movement controls. `Regime` is a user struct pattern for holding atomic state shared across routines.

`Sinks` connect entity outputs to the real-time cycle. `AudioSink` owns an `AudioBuffer` and `AudioWriteProcessor`. `RenderSink` owns a `VKBuffer`, `GeometryWriteProcessor`, and `RenderProcessor`.

`SpatialIndex3D` answers radius and k-nearest queries. `HitTest` handles ray-entity and volume-entity intersection tests.

The function registry on `Fabric` maps string names to `InfluenceFn`, `ContainsFn`, and crossing functions, enabling `StateDecoder` to resolve callable names back to live function objects.

---

## Kinesis: The Mathematical Substrate

Kinesis is MayaFlux's mathematical library. No processing concepts, no schedulers, no buffers. Algorithms over data, expressed as free functions and lightweight types, available to every other layer.

`Kinesis::Discrete` covers algorithms over `std::span`: convolution, spectral transforms, sorting, onset detection, zero-crossing analysis, taper application (Hann, trapezoid, Blackman), extraction of peaks and troughs, quantization.

`Kinesis::Tendency` covers parameterized curves and field functions. `Tendency<D,R>` is a pure callable from domain to range; composition is through free functions with domain-specific factories in separate files. `ForceFields` defines spatial influence functions. `UVProjection` maps 3D positions into 2D parameter spaces.

`Kinesis::Stochastic` covers probability: distributions (Gaussian, uniform, Poisson, exponential), noise generators (white, pink, Perlin, simplex), random walk systems.

`Kinesis::Spatial` covers geometric queries: `SpatialIndex3D` with radius and k-nearest search, `ProximityGraphs` for KNN, Delaunay, and minimum spanning tree construction, `HitTest` for intersection.

Top-level Kinesis provides: `GeometryPrimitives` (platonic solids, subdivided surfaces, parametric shapes; `filled_rect` and related NDC helpers used by Forma); `MatrixTransforms`; `MotionCurves`; `NavigationState`; `VertexSampler`; `ViewTransform` (the camera matrix type consumed by the engine-reserved UBO at `set=0, binding=0`); `BasisMatrices`; `ndc_size_to_pixels` and related coordinate conversion helpers used by Portal::Forma layout.

---

## IO: Reading and Writing the World

IO loads external data into Kakshya containers and writes data back out. It is bidirectional.

**Reading:** `SoundFileReader` decodes audio via the FFmpeg/libav stack into `SoundFileContainer`. `VideoFileReader` and `VideoStreamContext` decode video frames into `VideoFileContainer` and `VideoStreamContainer`. `ImageReader` loads still images via STB into `TextureBuffer`. `ModelReader` loads mesh data via assimp. `CameraReader` captures from camera hardware. `AudioStreamContext` wraps live audio input. `IOManager` coordinates all readers and provides the unified loading interface.

**Writing:** `SoundFileWriter` encodes audio to any FFmpeg-supported container format via a worker thread and lock-free queue. It accepts interleaved `std::span<double>`, planar `vector<DataVariant>`, `AudioBuffer`, or `SoundStreamContainer`. Internally it owns a `FFmpegMuxContext` and `AudioEncodeContext`. `VideoFileWriter` encodes video frames via `VideoEncodeContext` and `FFmpegMuxContext` on a worker thread; it accepts raw pixel data or `VKBuffer` download commands, and supports continuous window capture via `IOManager::capture_window`. `FFmpegMuxContext` owns the `AVFormatContext` on the write path; `AudioEncodeContext` and `VideoEncodeContext` each add one stream to it. `IOManager::capture_output` hooks into the `AudioBackendService` output observer to continuously feed an `AudioOutputContainer` into a `SoundFileWriter`. `IOManager::write` encodes a `SoundStreamContainer` to disk synchronously with async worker drain.

`FileReader`, `FileWriter`, `TextFileWriter`: generic file IO interfaces. `JSONSerializer` handles structured JSON read/write for engine config, Nexus schemas, and other serialization tasks. `Keys` maps platform key codes to `IO::Keys`.

IO has no real-time dependency for loading: load, construct a container, return. Writing is asynchronous via worker threads; the caller submits frames and the worker encodes at its own pace.

---

## Journal: Structured Logging

`Journal::Archivist` is the singleton logging system. `scribe` logs at arbitrary severity with source location; `scribe_rt` is the real-time safe path backed by a lock-free ring buffer and a worker thread that drains to registered `Sink` instances. `ConsoleSink` and `FileSink` are the standard sinks. The logging system filters by `Component` (Audio, Buffers, Core, Graphics, IO, Kakshya, Kinesis, Kriya, Nexus, Nodes, Portal, Registry, Transitive, Vruta, Yantra, API, Lila) and `Context` (AudioProcessing, NodeProcessing, ContainerProcessing, FileIO, ShaderCompilation, Networking, Init, Shutdown, Configuration, UI, UserCode, Runtime, API, and others). `MF_INFO`, `MF_WARN`, `MF_ERROR`, `MF_DEBUG`, `MF_TRACE` are the standard macros; `MF_RT_*` variants route through the lock-free path. `MF_ASSERT` logs and aborts on failure.

---

## Transitive: Framework-Independent Utilities

Transitive contains code with no MayaFlux dependency. It exists because these utilities are useful throughout the codebase and because keeping them independent preserves the option to use them elsewhere.

`Transitive::Memory` provides `RingBuffer` (the history buffer underlying `FeedbackBuffer` and used throughout DSP code where temporal indexing is needed) and `Persist` (a simple persistence helper).

`Transitive::Parallel` provides `Dispatch` (thread pool dispatch) and `Execution` (parallel STL policy wrappers used by Yantra for parallelized operations).

`Transitive::Reflect` provides `EnumReflect`, a wrapper over `magic_enum` used throughout for enum-to-string conversion in logging and serialization.

`Transitive::Platform` provides `HostEnvironment` (platform detection, path resolution) and `FontDiscovery` (system font enumeration used by `Portal::Text`).

`Transitive::Protocol` provides `BinaryBuffer`, a lightweight binary serialization type used in network message construction.


---

## Lila: The Live Layer

Lila is the JIT environment. It uses Clang's `IncrementalCompilerBuilder` and LLVM ORC JIT to compile and install C++23 source fragments at runtime with latency bounded to one buffer cycle.

A Lila server listens on a TCP socket managed by ASIO. Incoming source arrives via `async_read_some` with accumulation until a complete fragment is received. That fragment is compiled, linked against the running binary's symbols, and installed. The installed code executes in the same process and address space. There is no isolation layer, no IPC, no data conversion.

`Vega` is the factory API used from Lila sessions. `vega.Sine()`, `vega.NodeBuffer()`, `vega.ParticleNetwork()`: factory calls create and register MayaFlux objects from within live-coded fragments. A code generation tool produces explicit `shared_ptr<T>` return declarations for each factory so that clangd can resolve signature help against concrete declarations rather than variadic templates.

`LiveArena` is a bump allocator for JIT object sharing across compilation cycles. It provides stable storage for objects created in one live-coded fragment and referenced in subsequent ones, avoiding the dangling-pointer hazard that arises when the compiler reclaims frame storage between compilations.

---

## Tokens and Domain

Every real-time object carries a processing token describing how and when it runs.

`Vruta::ProcessingToken`: `SAMPLE_ACCURATE`, `FRAME_ACCURATE`, `EVENT_DRIVEN`, `MULTI_RATE`, `ON_DEMAND`, `CONDITIONAL`.

`Nodes::ProcessingToken`: `AUDIO_RATE`, `VISUAL_RATE`, `EVENT_RATE`, `CUSTOM_RATE`.

`Buffers::ProcessingToken` is a bitfield combining rate, device, and concurrency. Rate flags: `SAMPLE_RATE`, `FRAME_RATE`, `EVENT_RATE`. Device flags: `CPU_PROCESS`, `GPU_PROCESS`. Concurrency flags: `SEQUENTIAL`, `PARALLEL`. Predefined combinations: `AUDIO_BACKEND`, `GRAPHICS_BACKEND`, `AUDIO_PARALLEL`, `INPUT_BACKEND`.

`Domain` unifies all three levels into a single 64-bit value. Predefined domains: `AUDIO`, `GRAPHICS`, `AUDIO_PARALLEL`, `AUDIO_GPU`, `AUDIO_VISUAL_SYNC`, `GRAPHICS_ADAPTIVE`, `INPUT_EVENTS`. Custom domains are composed with `compose_domain()` and validated with `create_custom_domain()`.

Domain is not a property attached to data when it is created. It is a token applied to the object that processes the data, resolved at the point where a stream enters a consuming context. The same numbers flow through all domains. The token decides the timing, the device, and the concurrency model that handles them.

The `API/Proxy/Domain` layer exposes domain composition and decomposition to user code and to the Vega factory system without requiring direct access to the token enums.

---

## How the System Composes

The namespaces above intersect at defined points and those intersections are where the interesting work happens.

A `SoundFileContainer` in Kakshya becomes audio output through a `SoundContainerBuffer` in Buffers, driven by a `BufferPipeline` in Kriya, scheduled by a `TaskScheduler` in Vruta, consumed by `AudioSubsystem` in Core, whose callback writes to hardware through the native audio backend.

A `ParticleNetwork` in Nodes computes physics at visual rate, writes positions to a `NetworkGeometryBuffer` in Buffers, which a `RenderProcessor` submits as a draw call through `RenderFlow` in Portal, recorded via `ShaderFoundry` and submitted to the GPU by `VulkanBackend`.

An `InstanceNetwork` drives `InstanceNetworkBuffer` which packs per-instance transforms from an optional `InstanceFieldOperator` GPU path into an SSBO and submits a single instanced draw call each frame.

A Yantra `GranularWorkflow` analyzes a `SoundFileContainer` using `Kinesis::Discrete` for onset detection and `EnergyAnalyzer` for grain attribution, produces a sorted `RegionGroup`, which a `SamplingPipeline` in Kriya plays back through the audio subsystem.

A Nexus `Emitter` fires its influence function each commit, pushes data to its `AudioSink`, which writes to an `AudioBuffer` registered with the `BufferManager`, mixed by `RootAudioBuffer` into the audio output. `StateEncoder` snapshots the Fabric's entity state to EXR+JSON; `StateDecoder` reconstructs it on the next session without re-running setup code.

A Forma `Mapped<float>` element exposes a fader. `Bridge::bind(id, envelope_node)` drives its value each frame from a node output. `Bridge::write(id, shader_processor, offsetof(PC, cutoff))` stages the value into push constants for a compute shader. `Bridge::write(id, audio_write_proc)` simultaneously routes the same value to an audio buffer. One element, one state, two outbound paths.

The common thread in all of these paths is data moving as numbers through typed containers, transformed by operations, driven by a scheduler, consumed by subsystems. Domain is resolved at the consumption point. Nothing before that moment needs to know whether the numbers will become sound, light, geometry, or control.

That is the premise, and it holds throughout.
