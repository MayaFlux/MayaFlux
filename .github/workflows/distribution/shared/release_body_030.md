Version: 0.3.0
**Commit**: {{COMMIT_SHA}}

---

0.2.0 expanded the framework's reach into the physical world: cameras, MIDI,
HID, physical modelling networks, a mature coroutine scheduler. 0.3.0 has two
primary focus areas. The first is spatial entity lifecycle: objects that exist
in space, perceive each other, and produce audio and visual output as a unified
consequence of that awareness. The second is offline computation: Yantra is now
a complete offline compute framework with a generic GPU execution layer,
CPU-or-GPU operation dispatch, true async pipelines, and the grammar-driven
GranularWorkflow as its first public-facing usage, performed live at TOPLAP
Bengaluru on a Steam Deck. Alongside these: the GPU-CPU boundary dissolves in
both directions, Tendency fields wire the node graph directly into geometry,
post-analog polyphonic stream playback lands as a new primitive, and text
rendering enters the same routing system as everything else.

---

## Compute and Yantra
- Yantra offline compute framework complete: full GPU execution layer with
  pipeline, descriptor, and resource management; CPU implementation remains
  automatic fallback when no GPU backend is attached
- GpuExecutionContext: base GPU backend slot on any ComputeOperation;
  set_gpu_backend() attaches a configured executor; CPU path is the fallback
- ShaderExecutionContext: concrete GpuExecutionContext for fixed-shader dispatch;
  fluent binding API (input, output, inout, push); multipass (CHAINED) dispatch
  via set_multipass(); typed output readback via read_output<T>()
- TextureExecutionContext: GpuExecutionContext variant for image binding staging
  and layout transitions; compute shaders operating on textures directly
- GpuResourceManager: Vulkan pipeline, descriptor set, and buffer lifecycle for
  GPU compute; dispatch and dispatch_batched; image binding and layout transition
  support
- GpuDispatchCore extracted from GpuExecutionContext; input extraction made
  overridable
- GpuTransformer, GpuAnalyzer, GpuExtractor, GpuSorter: GPU-native operation
  types; CPU path is a hard error; for CPU-with-GPU-acceleration prefer
  set_gpu_backend() on an existing concrete operation
- CHAINED execution mode for multi-pass batched dispatch
- GranularWorkflow: first named workflow in the grammar system; segment_grains,
  attribute_grains, sort_grains, reconstruct_grains, reconstruct_grains_additive
  as named grammar rule executors selected by the matrix from data shape and
  ExecutionContext metadata
- SortOp: CPU path by default; GPU path via sort_by_attribute.comp and
  GpuSorter when grain count exceeds gpu_sort_threshold in GranularConfig
- STREAM and STREAM_ADDITIVE output modes writing reconstructed audio directly
  into a running DynamicSoundStream asynchronously
- process_to_stream and process_to_stream_async overloads
- process_to_container_async overloads (AnalysisType and AttributeExecutor paths)
- GrainTaper: span lambda applied in-place per grain before accumulation
- GranularConfig consolidating pipeline scalar parameters across all entry points
- AttributeOp attribution resolved in priority order: span lambda,
  pre-configured analyzer, analysis type with default construction
- make_granular_matrix factory configuring grammar rules per output type
- ComputeMatrix::with_async for deferred chain execution
- SegmentOp frame count corrected for interleaved containers

## Audio
- SamplingPipeline: polyphonic playback of bounded audio streams; multiple
  independent CursorAccessProcessor instances over a shared DynamicSoundStream,
  each with its own loop region, loop count, and fractional speed accumulator
- StreamSlice: region descriptor for cursor-based stream access; separates
  region definition from cursor state
- CursorAccessProcessor: independent cursor with variable speed via fractional
  frame accumulation, loop wrapping, loop count, on_end callback; allocates
  dynamic slot on DynamicSoundStream
- StreamSliceProcessor: slot pool mixing into a single AudioBuffer per cycle;
  inactive slots contribute silence with no read overhead
- create_sampler and create_samplers factory helpers in Rigs.hpp
- create_sampler_from_stream for multichannel shared-stream samplers
- load_audio_bounded: single-step bounded DynamicSoundStream loading on IOManager
- BufferPipeline::on_complete callback and pipeline timing infrastructure
- SamplingPipeline::build_for and rebuild_for for duration-bounded playback

## Graphics
- Nexus spatial entity layer: Fabric, Wiring, Emitter, Sensor, Agent
- Fabric: plain object orchestrator owning SpatialIndex3D with lock-free snapshot
  publication; not a subsystem, not registered with Engine
- Wiring: fluent builder connecting entities to scheduler infrastructure; fire on
  interval, key event, mouse event, OSC/network message, or choreographed
  move_to steps via EventChain
- Agent: two-phase commit execution; perception first (PerceptionContext from
  spatial snapshot), influence second (InfluenceContext acting on render
  processors and audio sinks simultaneously)
- Emitter: acts without querying spatial index
- Sensor: queries without acting
- InfluenceContext: intensity, radius, color, size, render_proc fields;
  influence UBO binding to target RenderProcessor
- SpatialIndex3D with lock-free snapshot publication
- HitTest ray casting and SpatialIndex::all()
- FieldOperator: Tendency field evaluation against vertex attributes in strict
  order; POSITION, COLOR, NORMAL, TANGENT, SCALAR, UV targets; ABSOLUTE and
  ACCUMULATE modes; works with PointVertex, LineVertex, MeshVertex
- Tendency<D,R>: composable field primitive with typed domain and range;
  force field factories and UV projection factories in Kinesis
- MeshFieldOperator: per-slot FieldOperator instances for MeshNetwork;
  writes results back via set_mesh_vertices
- UVFieldProcessor and uv_field.comp: UV field evaluation feeding into
  compute shader pipeline
- NetworkTextureBuffer: NetworkGeometryBuffer subclass routing particle
  system UV coordinates into a texture pipeline
- TextureContainer: SignalSourceContainer for GPU texture pixel data;
  first-class Yantra compute input
- WindowContainer GPU bridge and interface completeness; BGRA swapchain
  format mapping corrected
- TextureLoom: storage image creation and layout transition support
- TextureBuffer::set_gpu_texture
- MeshNetwork: MeshSlot, MeshNetworkProcessor, MeshNetworkBuffer;
  per-slot SSBO transform path and per-slot texture array path
- MeshOperator base, MeshTransformOperator, MeshFieldOperator
- mesh_network vert/frag shader pair for SSBO transform path
- ViewTransform moved from push constant to UBO at set=0, binding=0;
  set=0 permanently engine-reserved; user descriptors start at set=1;
  push constants fully free for user shader parameters
- UNIFORM_BDA and STORAGE_BDA buffer usage types
- get_buffer_device_address on BufferService and BackendResourceManager
- bufferDeviceAddress enabled via vk::StructureChain in VKDevice
- NavigationState: first-person fly-navigation primitives
- ViewportPreset window-level fly-navigation binding
- Frame-rate independent movement in compute_view_transform
- View transform getters on RenderProcessor
- line_lit shader variant; lit shader variants for Nexus influence
- GeometryWriteProcessor: raw vertex upload path, MESH write mode
- GeometryBuffer::setup_rendering inherits topology from node
- Indexed draw path in RenderProcessor; index buffer upload in
  GeometryBindingsProcessor
- MeshVertex: universal 60-byte layout
- MeshWriterNode for indexed triangle mesh geometry
- ModelReader with Assimp backend; TextureResolver; load_mesh_network
  and load_mesh on IOManager; read_mesh_network and read_mesh on vega
- OperatorChain on NodeNetwork: ordered sequence of secondary
  NetworkOperator instances run after the primary operator each
  process_batch(); add, emplace<T>, remove, clear, get, find<T>,
  operators(); null by default, initialized by subclasses that require
  multi-operator sequencing
- Counter generator node
- TypedHook<ContextT> for typed callback dispatch
- Frame rate propagation through node graph for visual-rate timing

## Text
- Portal::Text subsystem integrated into GraphicsSubsystem
- InkPress: press, repress, impress API with RedrawPolicy (Clip/Fit)
- impress: incremental append into pre-allocated budget with automatic
  wrapping, budget growth, and Overflow detection
- TextBuffer: TextureBuffer subclass with streaming mode and alpha blending
  enabled at construction; budget dimensions, cursor tracking, accumulated text
- GlyphAtlas, FontFace, FreeTypeContext
- TypeFaceFoundry singleton owning default font state
- set_default_font with platform font discovery; get_default_atlas returning
  reference
- create_layout, ink_quads, rasterize_quads on public Text API
- codepoint field on GlyphQuad
- FreeType and utf8proc as dependencies; fontconfig on Linux

## IO and Containers
- TextureContainer layer-aware
- source_path and source_format on FileContainer
- Persistent store and safe teardown on Transitive layer
- make_persistent<T>: construct in-place, process-lifetime retention,
  returns T&; make_persistent_shared<T>: returns shared_ptr<T>
- Cross-platform file path normalization and relative path resolution
  against cwd and source root
- AudioBuffer channel inference from get_channel_id() when context omits
  channel

## API
- DomainSpec replacing bare constexpr Domain constants; operator[] for
  channel binding syntax: object | Audio[ch]
- Audio[ch] as canonical multichannel routing syntax; docs migrated
- CreationProxy replacing CreationHandle for 0.3 transition;
  CreationContext overload for shared_ptr pipe operator
- operator| on CreationHandle for DomainSpec and CreationContext
- prepare_audio_buffers for deferred playback registration
- create_line using retain flag instead of loop
- ViewportPresetMode enum for bind_viewport_preset
- remove processor shorthands
- MF_ASSERT macro

## Lila
- async_read_some accumulation replacing async_read_until; preserves
  multi-line blocks correctly

## Breaking Changes
- CreationHandle replaced by CreationProxy (removal target 0.3.1)
- set=0 permanently engine-reserved; user descriptors at set=0 must
  move to set=1+
- ViewTransform push constant slot removed; shaders relying on push
  constant ViewTransform require update to UBO path at set=0, binding=0

Source: github.com/MayaFlux/MayaFlux
Docs: mayaflux.org
License: GPLv3
