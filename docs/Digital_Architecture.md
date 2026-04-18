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

These four namespaces form the real-time backbone. They are what runs when the audio callback fires, when the GPU frame begins, when the network packet arrives. Everything else in the system either feeds into them or operates independently of their timing constraints.

### Nodes

A node is a moment of transformation. It takes one unit of data and produces one unit of data, evaluated as many times per second as its domain requires. That is the complete definition. There is no implicit audio-ness or visual-ness to a node. A node that computes `sin(phase)` is equally valid as a 440 Hz oscillator or as the x-coordinate of a moving point or as a brightness modulator. The node does not know which it is. The connection that routes its output decides.

The node taxonomy reflects what kinds of transformation exist, not what domain they belong to.

**Generators** produce values from internal state: `Sine`, `Phasor`, `Impulse`, `Polynomial`, `Logic`, `Random`, `WindowGenerator`. A phasor is a ramp from 0 to 1 that repeats. That ramp can drive audio synthesis, animation, or shader parameters without modification.

**Filters** reshape incoming streams: `FIR`, `IIR`. Coefficients define the response. The filter does not care whether the stream is audio or control.

**Conduit** nodes route and combine: `NodeChain`, `NodeCombine`, `Constant`, `StreamReaderNode`. These are the plumbing layer, connecting transformation stages without introducing computation of their own.

**Input** nodes bridge hardware events into the graph: `HIDNode`, `MIDINode`, `OSCNode`. A MIDI note becomes a frequency value; a HID axis becomes a double. From that point it is just a number.

**Graphics** nodes write computed values into GPU resources: geometry writers, mesh writers, topology generators, path generators, texture nodes, procedural texture nodes, point collection nodes. These are where numeric streams cross into the visual domain, not by converting them but by routing them to a buffer that a render pipeline consumes.

**NodeNetworks** are a qualitatively different tier. Where a single node transforms one unit at a time, a NodeNetwork coordinates hundreds or thousands of nodes whose relationships define the output. The relationships are the computation.

`ParticleNetwork` models entities with position, velocity, and force. Physics integrates forward each frame. The positions are numbers that flow wherever you connect them.

`PointCloudNetwork` holds spatial samples with no identity or persistence. Structure emerges through attached operators: `TopologyOperator` infers connectivity by k-nearest or radius or Delaunay; `PathOperator` interpolates curves through control points. The network itself performs no computation; operators define what the points mean.

`ModalNetwork` decomposes resonance into independent modes, each a decaying oscillator with a frequency ratio, a decay coefficient, and an amplitude. Excite the network with an impulse and the modes respond according to their spectrum: harmonic, inharmonic, or stretched. Sum them and you have a physical timbre.

`WaveguideNetwork` simulates wave propagation through delay lines. Two modes: unidirectional for string-like structures where a single loop circulates energy; bidirectional for tube-like structures where two rails travel in opposite directions with reflection at each termination. The boundary conditions determine the harmonic series.

`ResonatorNetwork` applies IIR biquad bandpass sections to whatever signal enters. Feed it noise and it becomes a formant synthesizer. Feed it a pulse and it voices a resonant space. Feed it anything and it morphs the spectrum toward its target profile.

`MeshNetwork` operates on mesh topology as a first-class computational structure.

All networks accept `NetworkOperator` instances that define behavioral layers: `FieldOperator`, `MeshFieldOperator`, `PhysicsOperator`, `MeshTransformOperator`, `GraphicsOperator`, `OperatorChain`. Operators compose. A field operator defines how a scalar or vector field evaluates across the network's positions; a physics operator integrates forces; a transform operator applies spatial modifications.

### Buffers

If a node is a moment of transformation, a buffer is a span of time held in one place until something can be done with it.

A node produces one value per evaluation. A buffer accumulates those values over a cycle, holds them, and makes them available as a block. Most useful operations on streams require a window of time: spectral analysis, convolution, onset detection, granular decomposition. None of these are possible sample by sample. The buffer creates temporal extent where none existed.

Buffers are cycle-driven. They live inside the scheduler's tick. They have tokens that identify their processing domain and rate. They are registered with a `BufferManager` that knows when to process them.

The buffer family is broad because real-time processing spans many domains.

**Audio buffers** accumulate double-precision samples for the audio subsystem. `AudioBuffer` is the base. `NodeBuffer` is an audio buffer whose data source is a node. `FeedbackBuffer` adds a `HistoryBuffer` backed by a ring buffer, enabling delay-line and recursive signal paths. `InputAudioBuffer` captures from hardware input.

**Geometry buffers** accumulate vertex data for the graphics pipeline. `GeometryBuffer`, `MeshBuffer`, `CompositeGeometryBuffer`: these hold the vertex streams that get submitted as draw calls.

**Network buffers** bridge NodeNetworks to subsystem outputs: `NetworkGeometryBuffer`, `MeshNetworkBuffer`, `NetworkTextureBuffer`, `NetworkAudioBuffer`. A `ParticleNetwork`'s positions become geometry; a `ModalNetwork`'s output becomes audio; neither network knows how it is consumed.

**Texture buffers** hold image data flowing to GPU image resources: `TextureBuffer`, `NodeTextureBuffer`.

**Container buffers** bridge Kakshya containers into the real-time cycle: `SoundContainerBuffer`, `VideoContainerBuffer`. A sound file container becomes a continuous stream through this bridge.

**Root buffers** aggregate child buffers and present a single output to a subsystem. `RootAudioBuffer` mixes its children into the audio callback output. `RootGraphicsBuffer` aggregates geometry for the frame. Adding a child buffer to a root is how you connect processing work to hardware.

**Recursive buffers** implement feedback paths. `FeedbackBuffer` is intentionally simple: one delay line, one feedback coefficient, one history length. For nonlinear feedback or filter-in-loop behavior, `PolynomialProcessor` in recursive mode or a `BufferPipeline` routing back to itself is the appropriate path.

### Processors

Processing happens to buffers, not inside them. A buffer holds data and coordinates with the scheduler. A processor defines what is done with that data each cycle. The two are inseparable in practice and distinct in concept.

Every buffer carries a processing chain. Processors attach to that chain and execute in order when the buffer's cycle fires. The buffer coordinates timing; the processor defines the operation.

`NodeFeedProcessor` evaluates a node unit by unit until the buffer is full. This is how a `NodeBuffer` drives a sine oscillator or a polynomial transform into the audio cycle. `FilterProcessor` applies IIR or FIR coefficients to the accumulated samples. `LogicProcessor` applies boolean operations. `PolynomialProcessor` applies a user-supplied polynomial function, with optional recursive mode for feedback paths. `NodeBindingsProcessor` binds node outputs to GPU descriptor slots. `AggregateBindingsProcessor` combines multiple node outputs into a single descriptor update.

On the graphics side: `GeometryWriteProcessor` writes vertex data into a geometry buffer. `MeshProcessor` handles mesh topology updates. `RenderProcessor` records draw commands into a command buffer and submits them through the Portal rendering system. `ComputeProcessor` dispatches a compute shader. `DescriptorBindingsProcessor` manages descriptor set updates for shader resources. `UVFieldProcessor` evaluates UV field operators over geometry each cycle.

For data movement between CPU and GPU: `BufferUploadProcessor` stages CPU data to a GPU buffer. `BufferDownloadProcessor` reads GPU results back to CPU memory. `TransferProcessor` coordinates buffer-to-buffer copies.

For Kakshya container access: `AccessProcessor` variants pull data from containers into audio buffers at the right rate and position. `StreamSliceProcessor` manages polyphonic slice playback within a `SamplingPipeline`.

For audio output: `AudioWriteProcessor` writes computed data into an audio buffer for the hardware callback. `MixProcessor` combines multiple child buffer outputs into a root buffer.

The node graph and the buffer pipeline connect through these processor types. Nodes produce values on demand; processors evaluate those nodes into buffers at the right rate and in the right format for each subsystem.

### Vruta

Vruta is the execution model. It defines what time-structured work is in MayaFlux.

The central coroutine types are `SoundRoutine` and `GraphicsRoutine`: C++20 coroutines that suspend at `co_await` points and are resumed by the scheduler. `SoundRoutine` ticks at audio rate; `GraphicsRoutine` ticks at frame rate. Everything time-dependent in MayaFlux is expressed as one of these, submitted to a `TaskScheduler`. The scheduler drives the system: it maintains a clock, it knows the current sample or frame position, and it resumes coroutines whose suspension conditions are met. Kriya uses `SoundRoutine` for most of its composition primitives because audio rate provides the finer timing resolution and many scheduling tasks benefit from that precision regardless of whether they produce audio output.

`DelayContext` distinguishes two timing regimes. Buffer-based timing counts in buffer cycles, appropriate for operations that align with the audio callback period. Sample-based timing counts in individual samples, appropriate for sub-cycle-accurate scheduling. Both are available as suspension points inside coroutines.

`Event` and `EventManager` hold named coroutines that respond to conditions. An event is not a callback registered to a dispatcher; it is a coroutine that runs until its condition fires, suspends, and runs again on the next occurrence.

`EventSource` and `NetworkSource` are the sources that events wait on. A `NetworkSource` wraps an incoming message stream; a coroutine suspended on it resumes whenever a message arrives.

`Clock` tracks elapsed time in samples and converts between sample counts and seconds. It is the single reference for all timing in the system.

Vruta does not know what audio or geometry or physics are. It knows tasks, time, suspension, and resumption. That is precisely its scope.

### Kriya

Kriya is the language for writing time-structured processing work using Vruta's runtime.

Where Vruta provides the primitives, Kriya provides the vocabulary. The awaiters `SampleDelay` and `BufferDelay` express time in terms the system understands. `ProcessingGate` suspends until a condition becomes true. `EventAwaiter` and `NetworkAwaiter` suspend until an event or a message fires.

`BufferPipeline` is the primary composition surface for describing sequences of buffer operations as coroutine-driven chains. Capture a buffer, process it, accumulate it, route it somewhere else, branch if a condition holds, repeat for N cycles or indefinitely. The pipeline is itself a `SoundRoutine` under the hood; its fluent interface describes what that coroutine does.

`SamplingPipeline` specializes this for polyphonic stream playback. It owns an `AudioBuffer` with a `StreamSliceProcessor`, manages voice activation and deactivation, and coordinates the buffer pipeline lifecycle around the stream's bounds.

`Chain` (the `EventChain`) sequences discrete actions with sample-accurate delays. `then()`, `wait()`, `every()`, `repeat()`, `times()`: these compose timed sequences without manual coroutine authoring.

`CycleCoordinator` synchronizes multiple buffer pipelines to a common cycle count. `Timer` and `TimedAction` provide fire-once and fire-with-cleanup scheduling over the coroutine runtime.

Input events enter the scheduling system through `InputEvents`: `any_key`, `mouse_pressed`, `mouse_released`, `mouse_move`. Each returns a named `Event` that fires as a coroutine-based listener. Network events enter through `NetworkEvents`: `on_message`, `on_message_from`, `on_message_matching`.

The relationship between Vruta and Kriya is the relationship between a runtime and the code written for it. Vruta is what makes Kriya possible; Kriya is what makes Vruta useful.

---

## Kakshya: The Data Layer

Kakshya is not real-time. This is intentional and important.

Kakshya is the repository and classification layer for data at rest. It holds data, describes data, provides zero-cost views into data, and processes data on demand. It has no scheduler dependency. Its processors are pull-based, not tick-driven. It does not know what a buffer cycle is.

At the center is `SignalSourceContainer`, the base for all typed containers. Containers hold data and know their structure: sample rate, channel count, frame count, modality. The concrete types cover the full range of digital media: `SoundFileContainer` holds decoded audio from a file; `DynamicSoundStream` holds a bounded stream that can grow; `SoundStreamContainer` wraps a live audio stream; `VideoFileContainer` holds decoded video frames; `VideoStreamContainer` wraps a live video feed; `CameraContainer` wraps camera capture; `TextureContainer` holds image data with pixel format and dimensions; `WindowContainer` wraps a windowing surface. Each container is a typed, structured home for a body of data.

`NDData` (N-dimensional data) and `DataVariant` are the carriers. `DataVariant` is the type-erased container for numeric data in motion through the system: a `std::vector<double>`, a matrix, a mesh, a texture. `NDData` provides structured N-dimensional access with typed interpretations: scalar, vec2, vec3, vec4, complex, mesh. `EigenAccess` and `EigenInsertion` bridge NDData to Eigen matrices. `MeshAccess`, `VertexAccess`, and `TextureAccess` bridge to their respective formats.

`Region` and `RegionGroup` are zero-cost markers over containers. A `Region` names a span: start frame, length, channel mask. It does not copy the data. A `RegionGroup` organizes multiple regions with metadata, creating a structured view over a body of content. `RegionSegment` subdivides regions further. `OrganizedRegion` carries attribution. These are the primitives for treating a container as compositional material: label the intro, the high-energy section, the transient, the grain pool. Access them by name. Transform them by region without touching the rest.

`DataProcessingChain` applies `DataProcessor` instances to container data on demand. Unlike the buffer processing chain which runs every cycle, this runs when you call it. Pull-based, not push-based.

The Processors subdirectory provides the access patterns: `ContiguousAccessProcessor` reads linearly; `CursorAccessProcessor` reads from a movable position; `FrameAccessProcessor` reads frame by frame; `WindowAccessProcessor` reads with overlap; `SpatialRegionProcessor` queries regions by spatial coordinates.

The Utils subdirectory provides coordinate conversion, region manipulation, surface utilities, and data format conversion.

The relationship between Kakshya and Buffers is a one-way bridge: Buffers consume from Kakshya containers through container buffers. A `SoundContainerBuffer` asks a `SoundFileContainer`'s processor for the next block each cycle. Kakshya never asks Buffers for anything.

---

## Yantra: The Offline Computation Universe

Yantra is what MayaFlux looks like when real-time constraints are removed.

If the real-time core is one half of the system, Yantra is the other half: a complete offline computation environment with its own type system, its own operation taxonomy, its own composition mechanisms, and its own GPU execution layer. It could stand alone as a library. It exists inside MayaFlux because offline and real-time computation need to share the same data layer (Kakshya) and the same mathematical substrate (Kinesis).

The type system begins with `Datum<T>`. Where the real-time side works with raw doubles flowing through buffers, Yantra wraps everything in `Datum`. A `Datum` carries the data, optional container context, and metadata about what the data is. Raw `T` enters Yantra at one point; from there it is always `Datum<T>`. This discipline enables operations to be composed without losing structural information.

`ComputeData` is the concept that constrains what `T` can be: vectors of doubles, matrices, DataVariant, RegionGroup, container pointers, and their combinations. `StructureIntrospection` infers the shape of any `ComputeData` value: its dimensions, its modality, its channel count. Operations use this to configure themselves at runtime.

The operation taxonomy covers the complete space of offline numerical computation.

**Analyzers** extract information without transforming data. `EnergyAnalyzer` computes RMS, peak, spectral centroid, and related measures over windows with configurable hop sizes. `StatisticalAnalyzer` computes moments, distributions, and cross-channel statistics. `UniversalAnalyzer` dispatches to the right implementation given the data type. `GpuAnalyzer` offloads large-scale analysis to Vulkan compute.

**Transformers** modify data. `MathematicalTransformer` applies gain, normalization, mixing, and arithmetic operations. `SpectralTransformer` applies FFT-based operations: filtering, convolution in frequency domain, spectral morphing. `TemporalTransformer` applies time-domain reshaping: resampling, interpolation, time-stretching. `ConvolutionTransformer` implements both direct and FFT convolution. `UniversalTransformer` dispatches. `GpuTransformer` offloads to compute shaders.

**Sorters** reorder data by computed criteria. `StandardSorter` sorts by single key. `UniversalSorter` handles multi-key and container-backed sorting. `GpuSorter` offloads large sorts to the GPU.

**Extractors** pull features out of data. `FeatureExtractor` identifies onset positions, spectral peaks, zero-crossings, and other perceptually meaningful events. `UniversalExtractor` dispatches. `GpuExtractor` offloads.

**Executors** are the GPU execution layer. `GpuExecutionContext` manages the full lifecycle of a Vulkan compute dispatch: descriptor allocation, buffer staging, shader execution, result readback. `TextureExecutionContext` specializes this for image shaders, bypassing numeric extraction and working directly with `VKImage` resources. `GpuDispatchCore` handles the low-level dispatch mechanics. `GpuResourceManager` manages GPU-side buffer and image lifetimes across dispatches.

**Composition** is where the system becomes expressive beyond individual operations.

`ComputeMatrix` is a self-contained execution environment that holds named operation instances and executes them with configurable strategies: synchronous, asynchronous, parallel across execution threads, or chained. Each matrix is independent. Two matrices do not share state.

`ComputationGrammar` is a rule-based production system for operation selection. Rules carry matching logic (type, context, parameter values, or arbitrary predicates), execution logic, priority, and metadata. When data arrives at a grammar-driven pipeline, matching rules are evaluated in priority order; the winning rule's executor runs. This enables adaptive computation: the same pipeline behaves differently given different input shapes, different execution contexts, or different metadata.

`ComputationPipeline` composes operations in sequence with optional grammar-driven selection at each stage. Stages can be added and configured by name at runtime. The pipeline carries the full composition from raw input to final output as a single callable object.

`GranularWorkflow` is a complete opinionated implementation of granular analysis and reconstruction built on top of the grammar and matrix systems. It segments a container into regions (grains), attributes them by energy, spectral centroid, onset density, or custom extractors, sorts them by those attributes, and reconstructs either a `RegionGroup` for further processing or a new container via overlap-add with optional per-grain tapering. Advanced users compose their own workflows from the operation primitives directly.

`OperationRegistry` maps operation types to factories for runtime discovery. Operations declare their category with a macro; the registry can find all analyzers, all transformers, all sorters compatible with a given input type. The `API/Proxy/ComputeRegistry` layer exposes this to the creator and factory system.

---

## The Visual Pipeline

The visual pipeline in MayaFlux is not a graphics mode for audio software. It is an independent, full-capability visual processing system that shares the same real-time infrastructure as audio: the same scheduler, the same buffer machinery, the same Kriya composition layer.

This section covers the namespaces that together form that pipeline.

### Portal

Portal is the glue layer between MayaFlux's internal systems and the external-facing capabilities that require coordination across backends, resource lifetimes, and platform abstractions. It is not a graphics namespace. It currently covers graphics, text, and network, and will grow as more surfaces require this kind of backend-to-user coordination. Graphics dominates in size because Vulkan is deliberately explicit: every resource, every synchronization point, every pipeline state requires a handshake between the backend and the user-facing API. Other surfaces like network are smaller today but will expand in future releases as the same coordination demands arise. The pattern is identical regardless of domain: Portal takes what Registry provides and builds a coherent, stateful API over it.

**Portal::Graphics** is the Vulkan coordination layer.

`ShaderFoundry` manages shader compilation from SPIR-V, command buffer allocation and recording, and descriptor set construction. It is the foundation everything else in Portal builds on.

`RenderFlow` orchestrates graphics pipeline creation and draw command recording using Vulkan 1.3 dynamic rendering. There are no render pass objects; `vkCmdBeginRendering` and `vkCmdEndRendering` bracket each frame. `RenderFlow` creates pipelines, binds them, records draw calls into secondary command buffers, and hands those to the present stage.

`ComputePress` manages compute pipeline dispatch: pipeline creation, descriptor binding, push constant upload, and `vkCmdDispatch` invocation. Yantra's `GpuExecutionContext` uses `ComputePress` under the hood.

`TextureLoom` creates and manages GPU textures: 2D, 3D, cubemaps, render targets. It bridges IO-loaded image data and Kakshya `TextureContainer` contents into `VKImage` resources.

`SamplerForge` manages sampler object lifecycle and provides a default linear sampler used by most texture operations.

`Portal::Text` is a complete text rendering pipeline: `FontFace` holds a loaded typeface; `GlyphAtlas` rasterizes and packs glyphs into a texture atlas; `TypeSetter` lays out text into a vertex stream; `InkPress` coordinates the full pipeline from string to rendered geometry; `TypeFaceFoundry` manages loaded faces.

**Portal::Text** is a complete text rendering pipeline: `FontFace` holds a loaded typeface; `GlyphAtlas` rasterizes and packs glyphs into a texture atlas; `TypeSetter` lays out text into a vertex stream; `InkPress` coordinates the full pipeline from string to rendered geometry; `TypeFaceFoundry` manages loaded faces.

**Portal::Network** provides the facade for network endpoint management. `MessageUtils` and `NetworkSink` handle message serialization and dispatch. The deeper endpoint management systems (StreamForge, PacketFlow) are in progress.

The descriptor layout contract with Portal is firm: `set=0, binding=0` is reserved for the `ViewTransform` UBO, which carries the camera matrices as engine-managed state. User descriptors begin at `set=1`. Push constants are fully available to user shaders.

### Registry

Registry and Portal are often confused because both relate to backend access. They are not the same thing at all.

`BackendRegistry` is a type-indexed map of factory functions. A backend registers a pointer to its service implementation under an interface type. A consumer queries by interface type and receives a pointer. Neither knows about the other. There is no abstraction over the service itself: the registry returns exactly what the backend registered. Its job is decoupling, not capability.

Portal builds complete, opinionated systems on top of whatever the registry returns. Portal knows what to do with a `DisplayService` or a `BufferService`; it constructs stateful facades with lifecycle, configuration, and rich API. A consumer using Portal never touches the registry. A consumer building new Portal-level systems queries the registry directly to get the backend handle they need.

Registry is the seam. Portal is the surface. The distinction matters because both are entry points but to completely different levels of the system.

### Core and Subsystems

`Core` is the engine layer. `Engine` owns the component graph and manages the initialization sequence. `SubsystemManager` coordinates `ISubsystem` instances, each of which encapsulates one processing domain.

`AudioSubsystem` wraps `RtAudioBackend`, registers callbacks, and drives the audio processing cycle. `GraphicsSubsystem` wraps `VulkanBackend`, manages the swapchain lifecycle, and drives frame rendering. `InputSubsystem` owns `HIDBackend` and `MIDIBackend` and delivers events to `InputManager`. `NetworkSubsystem` owns `TCPBackend` and `UDPBackend` and registers a `NetworkService` for decoupled access.

Each subsystem receives a `SubsystemProcessingHandle` at initialization: a token-scoped handle that provides controlled access to Buffers, Nodes, and the task scheduler without exposing manager internals. Subsystems operate through their handle; they do not hold direct references to managers.

`BackendRegistry` sits across all of this as the service discovery mechanism. Backends write into it on startup; Portal and other consumers read from it. The registry is the reason Yantra can dispatch GPU compute without knowing which backend is running.

---

## Nexus: Spatial Entity Simulation

Nexus is the spatial simulation and orchestration layer. It exists above the real-time core and connects to it through the buffer and rendering systems, but it does not depend on any of them for its own logic.

`Fabric` is the simulation container. It is a plain object: not a subsystem, not a registered singleton, not scheduler-aware. You drive it from whatever context owns your computation, at whatever rate makes sense for your work. `Fabric::commit()` runs one simulation step.

On each commit, every registered entity is evaluated. `Emitter` entities carry an influence function and optional position. When committed, the influence function receives an `InfluenceContext` containing the entity's current position, intensity, radius, color, and size. Whatever the influence function does with that context is entirely user-defined: write audio data to a sink, push geometry to a render sink, modify a shader parameter, trigger a Kriya event.

`Sensor` entities perceive their spatial neighborhood. On each commit, the spatial index is queried for entities within the sensor's radius, and the results arrive in a `PerceptionContext`. The sensor responds to what it finds.

`Agent` entities both perceive and influence. They receive a `PerceptionContext` first, then fire their influence function. An agent that hears nearby emitters and adjusts its own output accordingly is expressed naturally as a single entity type.

`Wiring` is the builder interface for connecting entities to MayaFlux systems without Nexus depending on those systems directly. Through `Wiring` you can attach interval scheduling, key or mouse triggers, network triggers, event triggers, position animation sequences, audio sink registration, and render sink registration. `Wiring` resolves these into Kriya coroutines or direct buffer registrations as appropriate. Nexus never holds a reference to a scheduler, a buffer manager, or a window; `Wiring` accepts those dependencies at configuration time and hands them off.

`Sinks` are the plumbing that connects entity outputs to the real-time cycle. An `AudioSink` owns an `AudioBuffer` registered with a `BufferManager`, and an `AudioWriteProcessor` that writes data into it each dispatch. A `RenderSink` owns a `VKBuffer`, a `GeometryWriteProcessor`, and a `RenderProcessor`. Adding a sink to an entity makes its output appear in the real-time audio mix or the rendered frame without the entity holding any reference to the subsystem.

`SpatialIndex3D` answers radius and k-nearest queries over entity positions using the `QueryResult` type from `Kinesis::Spatial`. `HitTest` handles intersection queries for ray-entity and volume-entity tests.

`InfluenceContext` carries the state of an emitter or agent at the moment of commit. `PerceptionContext` carries the spatial query results for a sensor or agent.

---

## Kinesis: The Mathematical Substrate

Kinesis is MayaFlux's mathematical library. It has no processing concepts, no schedulers, no buffers. It is algorithms over data, expressed as free functions and lightweight types, available to every other layer in the system.

`Kinesis::Discrete` covers algorithms over `std::span`: convolution, spectral transforms, sorting, onset detection, zero-crossing analysis, taper application (Hann, trapezoid, Blackman), extraction of peaks and troughs, quantization. These are the building blocks that `Kinesis::Discrete::Analysis` assembles into higher-level measurements and that Yantra's analyzers call internally.

`Kinesis::Tendency` covers parameterized curves and field functions. `Tendency` produces shaped values over a normalized parameter: not ADSR, but arbitrary polynomial and spline relationships between input and output. `ForceFields` defines spatial influence functions. `UVProjection` maps 3D positions into 2D parameter spaces.

`Kinesis::Stochastic` covers probability: distributions (Gaussian, uniform, Poisson, exponential), noise generators (white, pink, Perlin, simplex), random walk systems. These are used by nodes, networks, and workflows wherever controlled randomness appears.

`Kinesis::Spatial` covers geometric queries: `SpatialIndex3D` with radius and k-nearest search, `ProximityGraphs` for KNN, Delaunay, and minimum spanning tree construction, `HitTest` for intersection.

Top-level Kinesis provides: `GeometryPrimitives` (platonic solids, subdivided surfaces, parametric shapes); `MatrixTransforms` (rotation, scale, projection, coordinate system conversion); `MotionCurves` (arc-length parameterized interpolation, Catmull-Rom, B-spline); `NavigationState` (camera and viewport state); `VertexSampler` (point sampling over mesh surfaces and volumes); `ViewTransform` (the camera matrix type consumed by the engine-reserved UBO at `set=0, binding=0`); `BasisMatrices` (change-of-basis utilities for harmonic and modal computation).

---

## IO: Reading the World

IO loads external data into Kakshya containers. It is a one-way bridge: data enters MayaFlux here and becomes a typed container that everything else can work with.

`SoundFileReader` decodes audio files into `SoundFileContainer` via libsndfile. `VideoFileReader` and `VideoStreamContext` decode video frames via FFmpeg into `VideoFileContainer` and `VideoStreamContainer`. `ImageReader` loads still images via STB into `TextureContainer`. `ModelReader` loads mesh data via assimp into the Kakshya mesh structures. `CameraReader` captures from camera hardware into `CameraContainer`. `AudioStreamContext` wraps live audio input. `IOManager` coordinates these readers and provides the unified loading interface used by the API layer.

`FileReader` and `FileWriter` are the generic file IO interfaces. `TextFileWriter` handles text output.

`Keys` maps platform key codes to the `IO::Keys` enum consumed by input nodes and Kriya input events.

IO has no real-time dependency. It loads, it constructs a container, it returns. From that point the data is Kakshya's.

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

A Lila server listens on a TCP socket managed by ASIO. Incoming source arrives via `async_read_some` with accumulation until a complete fragment is received. That fragment is compiled, linked against the running binary's symbols, and installed. The installed code executes in the same process and address space as the rest of MayaFlux. There is no isolation layer, no IPC, no data conversion: the compiled code is simply more code in the running program.

This means Lila supports the full C++23 language including coroutines, templates, concepts, and structured bindings. Live-coded coroutines run on the same scheduler as everything else. Live-coded nodes join the same graph. Live-coded buffer pipelines read from the same containers.

`Vega` is the factory API used from Lila sessions. `vega.Sine()`, `vega.NodeBuffer()`, `vega.ParticleNetwork()`: these factory calls create and register MayaFlux objects from within live-coded fragments. A code generation tool (`gen_creator_signatures.cpp`) produces explicit `shared_ptr<T>` return declarations for each factory so that clangd can resolve signature help against concrete declarations rather than variadic templates, preserving IDE support inside Lila sessions.

---

## Tokens and Domain

Every real-time object in MayaFlux carries a processing token that describes how and when it runs. Tokens exist at three levels, one per real-time namespace.

`Vruta::ProcessingToken` describes the scheduling contract of a coroutine: `SAMPLE_ACCURATE` for audio-rate precision, `FRAME_ACCURATE` for frame-rate precision, `EVENT_DRIVEN` for coroutines that resume on incoming events rather than on a clock, `MULTI_RATE` for routines that can adapt, `ON_DEMAND` for routines that run only when explicitly triggered.

`Nodes::ProcessingToken` describes the evaluation rate of a node: `AUDIO_RATE`, `VISUAL_RATE`, `EVENT_RATE`, `CUSTOM_RATE`. A node registered under `AUDIO_RATE` is evaluated once per sample; a node under `VISUAL_RATE` is evaluated once per frame.

`Buffers::ProcessingToken` is a bitfield that combines rate, device, and concurrency characteristics. Rate flags (`SAMPLE_RATE`, `FRAME_RATE`, `EVENT_RATE`) describe when the buffer cycles. Device flags (`CPU_PROCESS`, `GPU_PROCESS`) describe where processing runs. Concurrency flags (`SEQUENTIAL`, `PARALLEL`) describe how processors are applied. Predefined combinations cover the common cases: `AUDIO_BACKEND`, `GRAPHICS_BACKEND`, `AUDIO_PARALLEL`, `INPUT_BACKEND`.

`Domain` unifies all three levels into a single 64-bit value. The high bits carry the node token, the middle bits carry the buffer token, and the low bits carry the Vruta token. Predefined domains cover the practical cases: `AUDIO`, `GRAPHICS`, `AUDIO_PARALLEL`, `AUDIO_GPU`, `AUDIO_VISUAL_SYNC`, `GRAPHICS_ADAPTIVE`, `INPUT_EVENTS`. Custom domains can be composed from individual tokens with `compose_domain()` and validated with `create_custom_domain()`, which enforces compatibility constraints such as `AUDIO_RATE` nodes being incompatible with `FRAME_RATE` buffers.

This is the mechanism behind the premise stated at the opening. Domain is not a property attached to data when it is created. It is a token applied to the object that processes the data, resolved at the point where a stream enters a consuming context. The same numbers flow through all domains. The token decides the timing, the device, and the concurrency model that handles them.

The `API/Proxy/Domain` layer exposes domain composition and decomposition to user code and to the Vega factory system without requiring direct access to the token enums.

---

## How the System Composes

The namespaces above are not independent layers stacked vertically. They intersect at defined points and those intersections are where the interesting work happens.

A `SoundFileContainer` in Kakshya becomes audio output through a `SoundContainerBuffer` in Buffers, driven by a `BufferPipeline` in Kriya, scheduled by a `TaskScheduler` in Vruta, consumed by an `AudioSubsystem` in Core, whose callback writes to hardware through `RtAudioBackend`.

A `ParticleNetwork` in Nodes computes physics at visual rate, writes positions to a `NetworkGeometryBuffer` in Buffers, which a `RenderProcessor` submits as a draw call through `RenderFlow` in Portal, which records into a command buffer via `ShaderFoundry`, submitted to the GPU by `VulkanBackend`.

A Yantra `GranularWorkflow` analyzes a `SoundFileContainer` in Kakshya using `Kinesis::Discrete` for onset detection and `EnergyAnalyzer` for grain attribution, produces a sorted `RegionGroup`, which a `SamplingPipeline` in Kriya plays back through the audio subsystem.

A Nexus `Emitter` with a position in world space fires its influence function each commit, pushes data to its `AudioSink`, which writes to an `AudioBuffer` registered with the `BufferManager`, mixed by `RootAudioBuffer` into the audio output.

The common thread in all of these paths is data moving as numbers through typed containers, transformed by operations, driven by a scheduler, consumed by subsystems. Domain is resolved at the consumption point. Nothing before that moment needs to know whether the numbers will become sound or light or control.

That is the premise, and it holds throughout.
