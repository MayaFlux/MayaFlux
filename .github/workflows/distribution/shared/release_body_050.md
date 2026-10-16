# MayaFlux 0.5.0

**Commit**: {{COMMIT_SHA}}

---

0.4 turned MayaFlux from a framework into a substrate: native platform backends, a standalone JIT, a GPU compute pipeline, a symmetric write layer. 0.5 is where that substrate is put to work. Roughly 920 commits separate the two, and four threads run through nearly all of them: simulation, IO, fluent authoring, and the numerical toolkit underneath.

**Simulation** is now a first-class domain. Fields written as ordinary C++ lambdas compile to compute shaders and drive particles, meshes, volumes and spatial agents. A GPU-resident spatial hash with claim, population and cluster dynamics, relaxation grids for cellular and diffusion systems, and a complete stable-fluids volume chain with a raymarcher all land together, and each is usable alone. **IO** no longer stops at the edge of the process: camera and video streams, GPU image capture, Alembic, OpenVDB, Assimp meshes, EXR, CSV and TSV all load and save through one `IOManager` and `Depot`, and live GPU state can be captured to disk as it runs. **Authoring** is the largest change of feel. `Creator`, config structs with designated initializers, generated overloads, `vega.mint` and `vega.evolve` turn multi-step sequences into single expressions. **Kinesis** grew into a broad numerical library, from filter design and polynomial kernels to estimation, motion channels and boolean logic, all dependency-free and reusable by nodes, buffers and controllers alike.

Computer vision runs end to end on the GPU, from a camera frame to contours, tracked keypoints and dense optical flow, and feeds back into rendering and sound through the same buffers and containers as everything else. Interactive surfaces gain scrolling, editable text and window lifecycle handling, and a tap-based sampler and a texture-array compositor round out the release.

---

## Platform

**macOS**: a native Cocoa window backend (`CocoaWindow`, an `NSWindow` with a `CAMetalLayer` content view) is now the windowing backend, with key repeat generated from `KeyRepeatConfig` to match Wayland and Win32. GLFW is removed on every platform, including the build, packaging and CI scripts. macOS support targets Apple Silicon with Clang 17 or newer, `std::format` replaces `fmt`, and the native libc++ parallel algorithms replace oneDPL.

**Windowing**: text input events (`WindowEventType::TEXT_INPUT`) arrive on Wayland, Win32 and Cocoa with their own repeat timing. Interactive resize signals once per completed gesture on Wayland and Win32. Closing a window propagates through Kriya and Forma, cancels its handlers, and detaches its render processors while the buffers survive. The Wayland backend now reads its display socket in `poll()` rather than depending on the WSI to do it.

**Graphics devices**: GPU selection is configurable through `GraphicsBackendInfo` (`device_index`, `device_uuid`, `device_name`, `device_preference`, `require_presentation`, `strict_device_selection`), with presentation families resolved at device selection. User supplied device extensions are validated before `createDevice`, with every missing name reported.

**Input**: a cross-platform tablet backend over HIDAPI with a HID 1.11 report descriptor parser delivers a 12 slot frame (position, pressure, tilt, rotation and more) with no window required. `TabletNode` projects one slot as a smoothed scalar while carrying the whole sample into every callback through `TabletContext`.

**MIDI**: the PipeWire backend gains an ALSA sequencer virtual input port that other applications can subscribe to.

**Build**: `shaderc` and `SPIRV-Tools` are required dependencies, Alembic is added for spatial IO, and `mold` is used on Linux when installed. `MAYAFLUX_ENABLE_LTO` is controlled through presets and enabled for every shipping preset. `unix-dev` is a `RelWithDebInfo` preset and `unix-test` covers Debug testing. GLSL source installs to `share/MayaFlux/src/shaders` beside SPIR-V under `runtime/shaders`, and the user guides install to `share/MayaFlux/guides`. `Config::set_project_root` resolves data, output and logs against a project's own root, and a report configuration (`data/report/mayaflux.report.json`) turns on validation layers and a trace log for bug reports.

---

## Vision

Vision is a full GPU-resident analysis stack described in `docs/Vision.md`. Operations are declared as a `VisionSequence` through a fluent builder that takes parameter structs and designated initializers, and run by `VisionGpuExecutor` (GPU, the default) or `VisionExecutor` (CPU, selected through `VisionBackend`). Both share one op signature through `VisionPass`.

**Operations**: global, adaptive and Otsu thresholds (multichannel, so a hue range and a saturation floor compose), colour conversion, Gaussian and arbitrary separable filters, Sobel and Scharr with gradient angle, Canny, morphology including Open and Close, Harris response with region masks, peak extraction, connected components, contour tracing, sparse Lucas-Kanade tracking with a forward-backward check, dense optical flow over a pyramid, `Downsample2x`, `Confine` for region restricted pipelines, and `Snapshot`. The CPU path carries AVX2/FMA and NEON SIMD in the filter, pixel, Harris and downsample kernels.

**GPU execution**: sequences dispatch as dependency stages with explicit hazards, so Otsu, Open and Close, connected components and contours each cost one submission and one fence wait. Descriptor writes are batched per dispatch, intermediate images are reused through `TextureLoom`, and flow and label buffers live in device-local memory. Connected components use block-based labelling with GPU-resident compaction. Contour selection is a K-round top-K reduction. Steps marked `deferred` submit asynchronously and resume on a later call, and results report `is_ready()`. An op-result memo keyed by `hash_vision_step` shares identical Sobel and blur work inside a walk. Tracked keypoints export to device-resident buffers without a host round trip. A `vision_ingest.comp` prelude converts the seed frame to `rgba32f` storage so the pipeline runs on both RADV and the NVIDIA proprietary driver.

**Query layer**: `VisionQuery` names what an analysis should produce through composable `VisionIntent` flags (`FindElements`, `TrackObjects`, `DetectFeatures`, `DetectEdges`, `EstimateMotion`, `MeasureAppearance`) with per-intent parameter structs, including colour segmentation and region confinement. Resolution shares leading GPU work across intents. `VisionAnalyzer` is a Yantra analyzer that reduces results on the GPU into centroids, bounds, shape statistics, appearance histograms and motion energy, runs each lane on its own executor, and can advance one sequence per call. `VisionExtractor` is a Yantra operation that turns an analysis into pixels: `crops`, `sample_regions`, `silhouettes`, `select_element`, `mask` and `patches`. `VisionMatrix` owns an analyzer and extractor for a query, keeps the last frame, analysis, extraction and per-lane results, and exposes `result(VisionIntent)`.

**Workflows**: `VisionWorkflow` (from `create_vision_workflow`) runs a query live on any `VideoStreamContainer`, cameras, video files and dynamic streams alike, delivering extractions into a second buffer (`WorkflowBufferConfig`), a live `TextureCollection` (`WorkflowContainerConfig`), or the layers of a `Chimera` (`process_to_stream`). `frame_results` broadcasts per-frame results through a `BroadcastSource`, and `subscribe` attaches coroutine consumers to any source. `ImageCVProcessor` and `VisionProcessor` run sequences directly on GPU images and pixel containers. The workflow holds the engine's managers weakly and stops its runs when destroyed.

---

## Shader generation and fields

**ShaderSpec**: `ShaderSpec::Assemble` describes a compute kernel declaratively (SSBO, sampled and storage image bindings, push constant fields, a `KernelTemplate` and typed `KernelOp`s) and `ShaderFoundry::load_shader` emits SPIR-V assembly directly, caching by content hash with no GLSL toolchain involved. `KernelOp` covers the arithmetic set, the `GLSL.std.450` math set, `CompareGE`, `ChannelDot` and reductions, and `Convolve2D` is a kernel template. `MF_KERNEL` captures a lambda body as source for kernels that need conditionals or loops, and standalone functions can precede `main`. Generated kernels bind into any executor through `config_from_spec` and `bindings_from_spec`, and into `RuleBuilder` through `with_gpu_backend`.

**Fields**: `MF_FIELD` stringifies a lambda and compiles it twice, once for the host as a `Tendency` and once as a GLSL function through `FieldSource`, so the two halves come from the same characters and cannot drift (`DualField`). `MF_FIELD_T` builds a `TemporalField` of position and time, and `MF_FIELD_P` a `ParametricField` that also receives a `vec4` parameter block. `FieldTarget` is an open bitmask, so one bind can drive position and normal together. `MF_BITMASK_OPERATORS` generates the operators for any scoped enum. `GpuFieldOperator` declares field deformation as a generated compute pass, `VertexFieldProcessor` dispatches it over a vertex range, and `ComputeMeshBuffer` accepts fields on either backend. The SDF include library gains line, plane, anisotropic Lp ball, triply periodic surface and metaball kernel families plus rotate, mirror, repeat, radial repeat and twist operators.

**Per-cycle feeds**: `ShaderProcessor` inputs can be supplied by a callable pulled every cycle, landing in the push constant block or a storage descriptor, and the feed now lands after a processor's own push constant write.

---

## Simulation

Simulation in 0.5 is built from small GPU-resident parts that share one vocabulary: fields, lattices, processors and config structs. The same field expression can deform a mesh, push particles, seed a volume or place a spatial agent.

**Particles and spatial hashing**: `SpatialFieldConfig` on `GpuFieldOperator` builds a GPU-resident counting-sort spatial hash over any network's vertices (`HashClearProcessor`, `HashCountProcessor`, `HashScanProcessor`, `HashScatterProcessor`) with density colouring and a deterministic pairwise claim protocol for absorption (`atomicMin` over the 27 cell neighbourhood). Persistent accreted mass is tracked in `PhysicsOperator`. Population dynamics run entirely on the GPU over an over-allocated reserve region: destroy on absorption, spawn on crowding. Cluster ids derive from `PhysicsOperator::get_collections()`, field bindings can be cluster scoped, and with `transfer_on_claim` a cross-cluster claim hops a vertex between clusters instead of consuming it. The pipeline reaches `PointCloudNetwork` and any `GraphicsOperator`, `NetworkGeometryBuffer` declares named auxiliary state fields alongside its vertices, and live setters tune thresholds and growth while it runs. Proximity graphs are parallelised and incremental geometry upload skips whole-buffer re-aggregation.

**Relaxation grids**: `RelaxationGridBuffer` is a GPU-resident double-buffered grid for any synchronous local rule (cellular automata, relaxation, diffusion, reaction systems), configured through `GridConfig` with a stage per compute pass and a single generation owner. Its state can be snapshotted, captured to disk through `RelaxationGridCapture`, and emitted as geometry through a rule and emit contract.

**Fluids and volumes**: `VolumeGridBuffer` holds named fields on a `Lattice3D` and ships the stable-fluids chain as processors on a shared `VolumeFieldProcessor`: `AdvectProcessor`, `DivergenceProcessor`, `PressureProcessor`, `SolenoidalProcessor`, `DiffuseProcessor` (implicit backward Euler by Jacobi), `BuoyancyProcessor`, `WallProcessor` and `InfluxProcessor`. Typed field refs from `declare_scalar`, `declare_vector` and `declare_scratch` remove string repetition, `setup_flow` builds the incompressible stage set from a config, and `accumulate` and `influx` inject authored fields. `RaymarchBuffer` and `RaymarchProcessor` render a scalar field front to back with early termination over a proxy box, configured through `MarchConfig`, and isosurfaces extract on the GPU through `ComputeMeshBuffer`.

**Lattices and assemblies**: `Lattice1D`, `Lattice2D` and `Lattice3D` in Kinesis pair resolution with bounds and derive cell geometry once for every consumer. `AssemblyNetwork` holds an unordered set of independent geometry items, each added, removed or updated on its own, under one milled draw. `RelationNetwork` hosts a growable set of slots and operators (see Kinesis and controllers). `generate_explicit_surface` builds a mesh from a table of heights, for blocks of sound, image brightness or spectrogram columns, and curve evaluation (`MotionCurves`) runs on a planar SIMD core and emits tangents alongside positions.

---

## Camera, video and Chimera

Cameras are enumerated through libavdevice (v4l2 and dshow) with preset capture modes and a fallback open path, behind a pluggable `CameraSource`. `Depot::choose_camera` lists devices and modes in a Forma picker, `choose_video` returns the video and audio pair with `VideoReadOptions::EXTRACT_AUDIO`, and `Creator::read_camera` and `read_volume` join the loader family. `container | Graphics` hooks any `VideoStreamContainer` and returns it typed, audio included. `get_associated_buffer` finds what a container was hooked to. Containers are format aware: depth modalities (`DEPTH_MAP`, `VIDEO_DEPTH`), `ValueRange`, and the requested pixel format carry through the FFmpeg read path.

`TextureArrayBuffer` is a texture buffer whose image is a stack of layers, bound as `sampler2DArray`. `Chimera` assembles those layers from many sources: stills and files, GPU images copied every frame, and moments of a recording ring or a growable GPU-held `TextureCollection`, read at a lag, at a lag that follows a `TimeMap`, or played freely forward, held or backward. `SamplerForge::plan_fit` handles stretch, contain, cover, center and tile placement, and the blit and per-layer upload paths avoid host downloads. `BufferPipeline` now runs at frame rate and takes graphics sources and targets, with `capture_to_stream`, `end()` for cancellation and render configuration on any operation.

---

## Sampling and time

`TimeMap` is a tendency from seconds on a local clock to a position in recorded material, with `linear`, `quadratic`, `triangle`, `piecewise_linear`, `lagged` and `integrated` factories. `StreamSlice::time_map` replaces speed with a map evaluated per block and read with linear interpolation, giving scrub, reverse, acceleration and ping-pong, and `StreamSlice::repeat` restarts the map at a changing repeat length. `TapSet` plays several independent taps on one stream, each with its own entry time, speed, direction, level and channel, with live speed control and lookback taps that trail the write head of a circular stream (`create_ring`, `TapSetBuilder::lag`). `DynamicSoundStream` rings are writable from `BufferPipeline` and can `snapshot` the most recent frames into an independent stream. Channel mix normalisation is switchable, and `record_into` accepts buffers and several sources at once.

Kriya `pattern` now takes durations as a list, a function of the pass number, or a condition, with typed generators and callbacks and no `std::any`. `Routine::on_complete` fires once at final suspend or on cancellation for every routine kind, and `EventChain` completion is built on it. `SweepList` and `AsyncGroup` provide lock-free async task tracking used by `IOManager` and `ComputeMatrix`.

---

## Kinesis and controllers

Kinesis is the numerical layer, free of node, buffer and Vulkan types, and 0.5 expands it across every domain it serves.

**Filter and kernel design**: `Discrete::Coefficients` provides the eight RBJ biquads, windowed-sinc FIR (lowpass, highpass, bandpass), direct pole placement, biquads by value for live rewriting, and ring-time math with no Q ceiling. `Kernels` adds `polynomial_lags`, `fourier_series`, `breakpoint_curve`, `pattern_lookup`, `dilate`, `erode`, `quantile`, `zscore`, `goertzel` and `modular_recurrence`, plus state-bearing `kuramoto` and coupled systems, all convertible to node buffer functions. `Dynamics` adds `pulse_response` and `lotka_volterra_step`, and window helpers gain boxcar support regions and exponential ramps. Lagrange weights are divide-free.

**Boolean logic**: `Discrete::Boolean` provides bit-valued kernels that keep their memory in the coefficient array (`truth_table`, `set_reset_latch`, `comparator` with a dead band, `window_comparator`, `signals_at_least`, `input_truth_table`, `clocked_register`), so behaviour is the numbers and survives clone, save, restore and live retuning.

**Coefficient-driven nodes**: `Polynomial`, `Logic` and `DeriveOperator` accept functions that receive the node's own coefficient array and keep state in it. `Logic` reads slots from other nodes, and `LogicProcessor` gains keys that read other buffers as input slots, bit planes that run every bit through its own clone of the node, and a neighbourhood automaton with memory across blocks. `FeedbackProcessor` is rebuilt around taps and combiner lambdas with linear or Lagrange reads up to order 7, and `BlockFeedbackProcessor` joins it. `ResonatorNetwork` retunes and sets decay in place, so a mapped parameter keeps its ring. `edit_feedback_coefs` and `edit_feedforward_coefs` give filters allocation-free spans, and IIR and FIR cache post-gain output per channel and normalise `a[0]` at the setter.

**Estimation and motion channels**: `Stochastic::Estimate` tracks the statistical character of an arriving stream (rolling variance, median absolute deviation, exponentially weighted variance and more), the analytical counterpart to `Stochastic`. `Differential` provides finite differences through named kinematic orders, central variants, curvature, heading and angular delta, and windowed path measures (length, displacement, straightness, turning, centroid, spread). `HampelFilter` screens isolated spikes, and `MotionChannel` wires `Estimate`, `Differential` and `HampelFilter` per stream, with per-order filtering and self-calibrating curvature. `SymbolicTrajectory` tracks cell sequences of a moving point through a `Lattice1D`, `Lattice2D` or `Lattice3D` partition, so a stroke, a gesture or a scalar stream becomes a symbolic sequence. `FeatureExtent` supplies fitted box and ellipsoid extents with graded membership and `ThresholdLatch` hysteresis, and `PathShape` supplies shape descriptors. Together with `TabletNode`, `InputNode` callbacks and `Perception`, these are the controller building blocks: a denoised, differentiated, classified view of any incoming stream.

**Relations and routing**: `RelationNetwork` is a dynamically growable slot-and-operator host with `AdvanceOperator`, `CombineOperator` and `WalkOperator`. `Weights` is a lock-free stochastic transition and routing table with `weighted_pick`, and `WalkOperator` moves a single active slot along its rows. `Covariation.hpp` provides multi-stream correlation and coherence measures.

---

## Nexus

`Agent::attach`, `Emitter::attach` and `Attachment` place an existing buffer on a principal, anchored on the surface centroid or a chosen vertex, with the offset expressed in the Agent's frame. Placement works through a geometry transform published alongside view and projection in the render UBO (`Kinesis::RenderTransform`), applied by the point, line, triangle, mesh, milled and volume shaders, and GPU fields evaluate through it. `Influence<T>` and `Perception<T>` let a producer or source build a value of any type from the context and hand it to targets; header-only field targets (`bind_field`, `bind_force`, `bind_position`, `bind_transform`, `bind_params`, `bind_push_constants`, `bind_influence_block`) feed fields, operators and shaders. Lights are an `InfluenceBlock` delivered through push constants. `Agent`, `Emitter` and `Sensor` no longer need functions when typed influences and perceptions supply their behaviour.

`Expanse` gains `AABB3D` bounds and typed crossing actions, `Hull` takes its region from a buffer's bounds and follows the buffer wherever it is placed, `Mantle` places a buffer to fill a box under `FitMode`, and an `Expanse` can perceive its occupants. `State` encoding and decoding (schema 6) cover the current classes.

---

## Portal

**Forma**: `Scrollable` (CPU scroll state, optional per-buffer scissor clipping, draggable indicator), an editable `TextField` with caret, up/down movement, click and wheel hit testing, `Tend<T>`, `Collapsible` labels composed with a background, `Portal::Forma::pick` for a generic list chooser, `Context::to_ndc_rect` for pixel-exact placement, and `bring_to_front` and `send_to_back` through `draw_priority`. Factories return `Form<T>` carrying topology, capacity and wiring, and `Surface` construction is unified through `SurfaceConfig`. `Layer::remove` cascades through `relate()` closures, and aggregated mapped ranges write into processors through `Bridge`.

**Text**: UTF-8 stepping and control-codepoint handling with tab stops and kerning, byte offsets and per-range style spans in layout and ink, `x_at` and `index_at` hit testing, range extraction with layout quads, `EditableText` operations, and a solid background composited under live text.

**Graphics**: `PrimitiveMill` reduces mixed-topology spans to triangles (points to quads, segments to mitred ribbons) so a buffer opts in through `RenderConfig::triangulate`, and also replaces the CPU line expansion on macOS. `ComputePress::record_sequence` records several compute pipelines in one submission, and `TextureLoom` reuses images across dispatches.

---

## IO

MayaFlux is no longer an island. Everything the engine produces or consumes can cross the process boundary, live or offline, through `IOManager`, with `Depot` adding native dialog-backed variants of every loader and saver. Each write path is asynchronous, tracked through `AsyncGroup`, and flushed on shutdown.

**Streams and capture**: cameras (v4l2 and dshow through libavdevice, behind a pluggable `CameraSource`), video files with their audio track, window and swapchain capture, GPU image readback (`TextureLoom::download_image` returns typed `ImageData`), and per-frame host snapshots of buffers, geometry and images through `DataReadProcessor`. `DataWriteProcessor` gains `write_*` routes for fields, slots, meshes and pixels in the other direction.

**Geometry and volumes**: `ModelWriter` exports meshes through Assimp from `MeshBuffer`, `MeshNetwork` and GPU-extracted `ComputeMeshBuffer` isosurfaces. `VolumeReader`, `VDBWriter` and `VolumeCapture` import and export OpenVDB grids, numbered sequences included, with `LatticeSemantics` stored as grid metadata. `SpatialCache` and `SpatialReader` write and read time-sampled points, curves and per-vertex attributes as Alembic, with `capture_spatial` recording one or more network geometry buffers, particles and point clouds included, frame by frame onto a shared clock, and `RelaxationGridCapture` recording the cell state of a relaxation grid. Image writers cover PNG, JPG, BMP, TGA, HDR and full-precision EXR.

**Tabular and structured data**: `Composite`, `CompositeLayout` and `CompositeContainer` hold heterogeneous named records (arithmetic, UTF-8 text, dynamic NULL/INTEGER/REAL/TEXT/BLOB values, fixed BLOBs). `DelimitedTextReader` and `DelimitedTextWriter` load and save CSV and TSV through `IOManager`, `Depot::choose_composite` and `vega.read_file`. `DataFieldTraversal` gives windowed access over any element type, and Yantra admits composites as `ComputeData`.

**Layout**: `IO/` is reorganised into subfolders by backend coupling, `IOManager` mechanics live in `IO::Detail`, writer registries share one `WriterRegistry<Writer>` template, and bare filenames write to `data/out` and `data/log`.

---

## Fluent authoring

The biggest change of feel in 0.5 is how little code a complete idea needs.

- **Creator** factory overloads are generated from constructors through Clang's AST, so the language server shows real parameter lists and defaults instead of forwarding templates. New members cover `create_chimera`, `create_texture_array`, `create_vision_workflow`, `create_ring`, `create_sampler`, `create_samplers` and `create_tap_set`, plus `read_camera`, `read_volume` and `read_file`.
- **`vega.mint`** takes a `StructureConfig` and builds a whole visible structure: it resolves the source, builds any network, registers in the required order, attaches textures and configures rendering, returning the real buffer. Model components can parent under earlier components or network slots.
- **`vega.evolve`** takes a config and wires state that advances on its own each cycle, starting with `RelaxationGridBuffer`.
- **Config structs everywhere**: `GridConfig`, `MarchConfig`, `Spot`, `SurfaceConfig`, `WorkflowBufferConfig`, `WorkflowContainerConfig`, `ComputeMeshBuffer::Config`, `AttachConfig` and the vision parameter structs take designated initializers. `EnergyAnalyzer`, `StatisticalAnalyzer`, `FeatureExtractor`, `StandardSorter` and the transformers gain aggregate configuration constructors.
- **Pipes and chains**: `container | Graphics` hooks a video or camera container in one expression. Fluent `Scrollable` placement, `ChimeraBuilder`, `TapSetBuilder`, `VisionSequence::Builder` and Forma factories that return `Form<T>` keep multi-step setup in one expression.
- **Typed scheduling**: `schedule_pattern` forwards to the typed `Kriya::pattern` forms, and `schedule_task` returns a `shared_ptr`.
- **Lambdas to kernels**: `MF_KERNEL`, `MF_FIELD`, `MF_FIELD_T` and `MF_FIELD_P` turn an ordinary C++ lambda into a compute kernel without leaving C++.
- **Includes**: `MayaFlux.hpp` includes `Convenience.hpp` unless `MAYAFLUX_NO_CONVENIENCE` is defined, workflow groups are opt-out, and filter coefficients and Nexus field targets are visible to user code by default.
- **Diagnostics**: journal output can be filtered by component and context, and `Config::set_project_root` anchors data and logs to a project.

---

## Concurrency

Every Kakshya container (`SoundStreamContainer`, `TextureContainer`, `WindowContainer`, `VideoStreamContainer`, `PlotContainer`) now synchronises with purpose-scoped `Seqlock`s and `SeqlockArray`s, so readers never take a mutex. `NDDataContainer<T>` is templated on its element type, and frame and value access are typed through `FrameView` and `DataSpanVariant` instead of widening to `double`.

---

## Upgrading from 0.4

- GLFW is removed. Native windowing is used on every platform, and `WindowingBackend::GLFW` and `GLFW_BACKEND` are gone. Intel macOS is no longer supported.
- `API/Yantra` is removed. Use Yantra operations and Kinesis algorithms directly.
- `IO::LoadConfig` is `IO::VideoLoadConfig`, and `load_video` and `choose_video` return `std::pair<video, audio>`.
- `ComputeMeshBuffer` is constructed from `ComputeMeshBuffer::Config` only. The two positional constructors are removed.
- Mock processing is `Node::exclude_output_from_graph`.
- `MAYASIMPLE` and the `MAYAFLUX_ALL_*` gate macros are replaced by explicit includes, `Convenience.hpp` and opt-out groups.
- Lit fragment shaders read the `Influence` block from push constants rather than a set 1 uniform buffer.
- `get_frame` and `value_at` on containers are typed through `FrameView` and `get_value_at<T>`. The container `lock`, `unlock` and `try_lock` triad is removed.
- `MAYAFLUX_USE_SHADERC`, `MAYAFLUX_USE_FMT` and `MAYAFLUX_USE_JTHREAD` are removed. `Execution.hpp` is removed in favour of `std::execution`.
- `WindowGenerator`, `CycleCoordinator`, `VKRenderPass` and `VKFramebuffer` are removed.

---

**Source**: [github.com/MayaFlux/MayaFlux](https://github.com/MayaFlux/MayaFlux)
**Docs**: [mayaflux.org](https://mayaflux.org)
**License**: GPLv3
