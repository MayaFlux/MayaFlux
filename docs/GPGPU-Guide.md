# Yantra Compute Guide

Yantra is MayaFlux's offline compute and grammar pipeline substrate. It is the layer you reach for when computation needs to be composed, orchestrated, and reasoned about as a data transformation graph rather than executed as a one-shot callback. The scope is deliberately broad: audio analysis, GPU image processing, spectral transforms, spatial sorting, granular reconstruction, and arbitrary GPGPU work all use the same types and the same execution machinery.

The central premise is that audio samples, vertex positions, image pixels, spectral bins, and region descriptors are all just numbers with metadata. Yantra does not special-case any of them. All data flows through the same `Datum<T>` envelope, crosses every operation boundary in that envelope, and is processed by the same executor, operation, and matrix types regardless of what the numbers represent. Domain (audio, visual, spatial) is a routing annotation carried by `DataModality` on the `Datum`, not an architectural boundary in the system.

The layers stack as follows. At the bottom, Kakshya provides the raw data types: `DataVariant` for concrete storage, `DataDimension` for structural metadata, `DataModality` for semantic labelling, and a family of `SignalSourceContainer` subclasses for resident or streaming data. Above that, Yantra's executor layer bridges typed `Datum` values to Vulkan SSBO and image dispatch, handling buffer staging, dispatch sizing, and readback. The operation layer sits on top of executors: operations own computational identity, the parameter system, CPU fallback logic, and GPU backend routing. Finally, the orchestration layer - `ComputeMatrix`, `ComputationPipeline`, and `ComputationGrammar` - composes operations into pipelines that can be linear, branching, async, or grammar-driven.

An operation is not a thin wrapper around a shader. It is the unit of algorithmic intent: it has a name, a category (`TransformationType`, `SortingType`, etc.), a parameter system, and optionally both a CPU implementation and a GPU backend. The CPU path is the fallback and the development target; the GPU path is attached via `set_gpu_backend()` once the algorithm is correct. Grammar rules describe when and how to instantiate and invoke operations based on what data arrives, decoupling operation selection from call-site logic.

---

## The Compute Chain

Before the individual components, it helps to see how they connect end to end.

Data enters Yantra as a `Datum<T>`, where `T` is any type satisfying the `ComputeData` concept: flat vectors, `DataVariant` collections, region descriptors, or container handles. The `Datum` carries not just the data but its dimensional structure (`DataDimension`), its semantic role (`DataModality`), and an optional `SignalSourceContainer` for cases where the data lives in a resident or streaming source rather than a flat buffer. This envelope is the only thing that crosses operation boundaries. Nothing is extracted, flattened, or converted until an executor needs to stage it for GPU dispatch.

A `ComputeOperation` consumes a `Datum<InputType>` and produces a `Datum<OutputType>`. It holds a named parameter system, a category type (`TransformationType`, `SortingType`, `ExtractionType`, `AnalysisType`), and optionally both a CPU implementation and a GPU backend. The CPU implementation (`operation_function`) is always the fallback. Attaching a `GpuExecutionContext` via `set_gpu_backend()` routes dispatch to the GPU when the device is ready, without changing the operation's external interface. The caller never needs to know which path ran.

The `ComputationContext` associated with a grammar rule is a separate concept from the operation's own type. It is a semantic tag on the rule - `TEMPORAL`, `SPECTRAL`, `MATHEMATICAL`, `STRUCTURAL`, and others - that the grammar uses as a secondary filter when matching rules to incoming data. An operation can appear under multiple contexts depending on how it is registered.

Operations are composed by the orchestration layer. Three shapes are available:

`ComputationPipeline` is a sequential chain. Operations are added by name and run in declaration order. A grammar can be attached; its rules are evaluated before the chain executes, allowing dynamic pre-processing. This is the right choice when the sequence is fixed and linear.

`ComputeMatrix` is the general engine. It supports arbitrary composition: named operations, typed execute calls, two-op sequential chains, parallel execution across multiple operations on the same input, batch execution across multiple inputs on one operation, and fluent `.with().then().then().to_io()` chains. Crucially, `with_async()` runs an entire chain on a background thread and delivers the result via a completion callback. The matrix owns the future; destruction and `drain_async()` both guarantee completion. The fluent chain is typed end to end: the compiler enforces that each `.then<Op>()` call's input type matches the previous step's output type.

`GrammarAwareComputeMatrix` extends `ComputeMatrix` with a `ComputationGrammar`. The grammar holds rules that match on data type, `ComputationContext`, priority, and arbitrary predicates. When the matrix runs `execute_with_grammar()`, it finds the highest-priority matching rule and fires its executor before any operation in the matrix runs. This enables pipelines whose operation selection is data-driven rather than hard-coded: the same matrix handles different input shapes, modalities, or sizes by dispatching to different rules. `GranularMatrix` is an alias for a `GrammarAwareComputeMatrix` parameterised on `RegionGroup`, used by the granular workflow.

The async chain pattern in `ComputeMatrix` is not just fire-and-forget. It is the primary mechanism for building offline processing pipelines that do not block the audio or graphics thread. The chain lambda captures operation handles, the context, and any reconstruction state. The completion callback delivers a fully typed `Datum` result. Because the matrix holds the future and the chain is entirely described in the lambda, the caller's only obligation is to keep the matrix alive until the result arrives.

---

## Data Layer (Kakshya)

### DataVariant

Concrete storage inside every `Datum`. Carries numeric, complex, vector, and matrix types without conversion at the boundary.

```cpp
using Kakshya::DataVariant = std::variant<
    std::vector<double>,
    std::vector<float>,
    std::vector<uint8_t>,
    std::vector<uint16_t>,
    std::vector<uint32_t>,
    std::vector<std::complex<float>>,
    std::vector<std::complex<double>>,
    std::vector<glm::vec2>,
    std::vector<glm::vec3>,
    std::vector<glm::vec4>,
    std::vector<glm::mat4>
>;
```

### DataModality

Semantic label that travels with a `Datum`. Influences how `OperationHelper` extracts channels and how grammar rules match. Set explicitly or inferred from dimension structure.

Notable values: `AUDIO_1D`, `AUDIO_MULTICHANNEL`, `IMAGE_2D`, `IMAGE_COLOR`, `IMAGE_COLOR_ARRAY`, `VIDEO_COLOR`, `VIDEO_GRAYSCALE`, `TEXTURE_2D`, `SPECTRAL_2D`, `VOLUMETRIC_3D`, `TENSOR_ND`, `VERTEX_POSITIONS_3D`, `VERTEX_NORMALS_3D`, `VERTEX_COLORS_RGB`, `VERTEX_COLORS_RGBA`, `TEXTURE_COORDS_2D`, `TRANSFORMATION_MATRIX`, `SCALAR_F32`.

### DataDimension

Single-axis descriptor with a `Role` (`TIME`, `CHANNEL`, `SPATIAL_X/Y/Z`, `FREQUENCY`, `POSITION`, `NORMAL`, `TANGENT`, `UV`, `COLOR`, `INDEX`, `MIP_LEVEL`, `CUSTOM`) and a `size`. Used by `OperationHelper` to determine dispatch element count and by grammar matchers to inspect structure.

Factory methods:
```cpp
DataDimension::time(n_samples);
DataDimension::channel(n_channels);
DataDimension::spatial(extent, 'x');
DataDimension::vertex_positions(n_verts);
```

### Datum\<T\>

The universal envelope. Every operation, executor, and grammar boundary crosses via `Datum`.

```cpp
template <ComputeData T>
struct Datum {
    T data;
    std::vector<DataDimension> dimensions;
    DataModality modality;
    std::unordered_map<std::string, std::any> metadata;
    std::optional<std::shared_ptr<SignalSourceContainer>> container;
};
```

Common aliases:

| Alias | Underlying type |
|---|---|
| `DataIO` | `Datum<std::vector<DataVariant>>` |
| `ContainerIO` | `Datum<std::shared_ptr<SignalSourceContainer>>` |
| `RegionIO` | `Datum<Kakshya::Region>` |
| `SegmentIO` | `Datum<std::vector<Kakshya::RegionSegment>>` |

Construction:
```cpp
std::vector<float> buf = ...;
Datum<std::vector<float>> d(buf);
d.set_metadata("sample_rate", 48000);
```

### Container types

Containers are pure storage with processing state and optional reader support. They sit in `Datum::container` and are resolved by executors and operations that need access to structured data beyond flat channels.

`SignalSourceContainer` - abstract base. Covers streaming, file-backed, and in-memory sources. Subclasses: `SoundFileContainer`, `SoundStreamContainer`, `AudioOutputContainer`, `DynamicSoundStream`, `VideoFileContainer`, `VideoStreamContainer`, `CameraContainer`, `PlotContainer`, `WindowContainer`, `TextureContainer`.

`TextureContainer` - layer-aware image data. Key methods: `get_layer_count()`, `to_image(layer)`, `from_image(layer, img)`, `set_pixels(...)`, `pixel_bytes()`. Used as the input/output carrier for `TextureExecutionContext` and for array shaders.

`DynamicSoundStream` - writable sample stream. Produced by `GranularWorkflow` reconstruction steps and consumed directly by `SamplingPipeline`.

---

## Executor Layer

Bridges `Datum<T>` to Vulkan SSBO and image dispatch. Executors own buffer staging, dispatch sizing, and readback. Operations delegate to executors; executors do not own computational identity.

### GpuShaderConfig

```cpp
GpuShaderConfig cfg;
cfg.shader_path        = "my_pass.comp";
cfg.workgroup_size     = { 256, 1, 1 };
cfg.push_constant_size = sizeof(MyPC);
```

### GpuExecutionContext\<InputType, OutputType\>

Abstract type-parameterised shell over `GpuDispatchCore`. Handles the two type boundaries:
- `extract_inputs()` - converts `Datum<InputType>` to flat double channels for dispatch.
- `collect_gpu_outputs()` - reconstructs `Datum<OutputType>` from `GpuChannelResult`.

Subclass this only for custom channel layout or non-numeric input (e.g. image-only shaders). All standard buffer and image shaders use the concrete subclasses.

`GpuDispatchCore` virtual hooks (override in subclasses):
- `declare_buffer_bindings()` - return the `GpuBufferBinding` list.
- `prepare_gpu_inputs()` - upload staging data before dispatch.
- `on_before_gpu_dispatch()` - per-dispatch mutation hook.
- `calculate_dispatch_size()` - override for 2D/3D grids; default derives from element count.

### ShaderExecutionContext\<InputType, OutputType\>

Concrete executor for SSBO buffer shaders. Fluent API assigns binding indices sequentially unless an explicit index is provided. Explicit and sequential calls may be mixed: explicit calls set the binding; auto-index calls append after the highest index so far.

```cpp
auto exec = std::make_shared<ShaderExecutionContext<>>(
    GpuShaderConfig { "graph_build.comp", { 256, 1, 1 }, sizeof(GraphBuildPC) });

// Sequential auto-index (0, 1, 2, 3):
exec->input(positions, GpuBufferBinding::ElementType::VEC3_F32)
     .input(attributes)
     .output(k_max_edges * 2 * sizeof(float))
     .output(sizeof(uint32_t), GpuBufferBinding::ElementType::UINT32)
     .push(pc);

// Explicit indices when slot order cannot be inferred:
exec->input(0, positions, GpuBufferBinding::ElementType::VEC3_F32)
     .in_out(1, scratch)
     .output(2, output_bytes);
```

Fluent methods:

| Method | Direction | Data |
|---|---|---|
| `input(data, type)` / `input(binding, data, type)` | INPUT | Pre-staged from vector. |
| `input(binding, type)` | INPUT | No pre-staged data; sourced from `Datum` at dispatch. |
| `in_out(data, type)` / `in_out(binding, data, type)` | INPUT_OUTPUT | Uploads and reads back. |
| `in_out(type)` / `in_out(binding, type)` | INPUT_OUTPUT | No pre-staged data. |
| `output(byte_size, type)` / `output(binding, byte_size, type)` | OUTPUT | Allocates output buffer. |
| `push(struct_or_value)` | Push constant | Copies into push constant block. |
| `set_multipass(count, updater)` | CHAINED mode | Batches `count` passes; `updater(pass_index, pc_ptr)` called per pass. |

`ElementType` values: `FLOAT32`, `FLOAT64`, `INT32`, `UINT32`, `VEC2_F32`, `VEC3_F32`, `VEC4_F32`, `VEC2_F64`, `VEC3_F64`, `VEC4_F64`, `IMAGE_STORAGE`, `IMAGE_SAMPLED`.

**Synchronous dispatch:**
```cpp
auto result = exec->execute(input_datum, ExecutionContext{});
// result.data: primary buffer readback (std::vector<double>)
```

**Output readback from named bindings:**
```cpp
auto edge_data = ShaderExecutionContext<>::read_output<float>(result, 2);
auto count     = ShaderExecutionContext<>::read_output<uint32_t>(result, 3)[0];
```

**Asynchronous dispatch:**
```cpp
auto fence = exec->dispatch_async(input_datum);
while (!Portal::Graphics::ShaderFoundry::is_fence_signaled(fence)) {}
auto raw = exec->collect_result();
```

### TextureExecutionContext

Executor for shaders operating on `VkImage`. Does not use the double-channel extraction path. Binding 0 is always the output storage image (`IMAGE_STORAGE`); the input sampled image lands at `image_binding` (default 1). Additional SSBOs may be declared via `aux_bindings`.

```cpp
TextureExecutionContext ctx(
    GpuShaderConfig { "warp.comp", { 16, 16, 1 }, sizeof(WarpPC) },
    Portal::Graphics::ImageFormat::RGBA8,
    TextureExecutionContext::OutputMode::CONTAINER,
    /*image_binding=*/1,
    /*aux_bindings=*/{ /* additional GpuBufferBindings */ });
```

Output modes:

| Mode | Description |
|---|---|
| `CONTAINER` | Downloads result into `TextureContainer`. Retrieved via `execute()` or `collect_container_result()`. |
| `SCALAR` | SSBO readback only; no image download. Retrieved via `collect_result()`. |
| `IMAGE` | Transitions output image to shader-read layout only; zero readback cost. Retrieved via `get_output_image(0)`. |

Staging input:
```cpp
// From a TextureContainer, explicit layer:
ctx.stage_container(my_container, /*layer=*/0);

// From a raw VKImage (render-pass output, camera frame):
ctx.stage_image(vk_image, sampler);  // must be in eShaderReadOnlyOptimal

// Via Datum::container field; context resolves on dispatch:
DataIO input;
input.container = texture_container;
ctx.set_input_layer(frame_index);
auto result = ctx.execute(input, ExecutionContext{});
```

Async path:
```cpp
auto fence = ctx.dispatch_async(input);
while (!Portal::Graphics::ShaderFoundry::is_fence_signaled(fence)) {}
auto result = ctx.collect_container_result();  // CONTAINER mode
auto raw    = ctx.collect_result();             // SCALAR mode
```

---

## Operation Layer

Operations own computational identity, the parameter system, CPU fallback logic, and GPU backend routing. Executors are infrastructure that operations delegate to.

### ComputeOperation\<InputType, OutputType\>

Base template. Subclass for fully custom operations.

```cpp
template <ComputeData Input, ComputeData Output>
class ComputeOperation {
protected:
    virtual output_type operation_function(const input_type& input) = 0;

public:
    void set_gpu_backend(std::shared_ptr<GpuExecutionContext<Input, Output>> backend);

    void set_pre_execution_hook(const OperationHookCallback&);
    void set_post_execution_hook(const OperationHookCallback&);

    output_type apply_operation(const input_type&);
};
```

When a GPU backend is attached and the GPU is ready, `apply_operation` delegates to the backend. When the backend is absent or GPU initialisation fails, it falls back to `operation_function`.

### Universal\* types (CPU + optional GPU)

Concrete CPU path. GPU-accelerated by calling `set_gpu_backend()` on the instance.

| Type | Override |
|---|---|
| `UniversalTransformer<In, Out>` | `transform_implementation()` |
| `UniversalSorter<In, Out>` | `sort_implementation()` |
| `UniversalExtractor<In, Out>` | `extract_implementation()` |
| `UniversalAnalyzer<In, Out>` | `analyze_implementation()` |

Built-in concrete transformers: `MathematicalTransformer`, `SpectralTransformer`, `TemporalTransformer`, `ConvolutionTransformer`. Built-in concrete analyzers: `EnergyAnalyzer`, `StatisticalAnalyzer`, `FeatureExtractor`. Built-in sorter: `StandardSorter`.

### Gpu\* types (GPU only)

Take a configured `GpuExecutionContext` at construction. CPU path throws.

```cpp
auto exec = std::make_shared<ShaderExecutionContext<>>(cfg);
exec->input(data).output(output_bytes).push(pc);

auto op = std::make_shared<GpuTransformer<>>(exec);
// get_executor() returns the context for further configuration
auto result = op->apply_operation(input_datum);
```

Equivalents exist for other categories: `GpuSorter`, `GpuExtractor`, `GpuAnalyzer`.

---

## Orchestration Layer

### ComputeMatrix

Instance-local execution engine. Create via `ComputeMatrix::create()` (returns `shared_ptr`).

**Add and retrieve operations:**
```cpp
auto matrix = ComputeMatrix::create();
matrix->add_operation("filter", my_op);
auto op = matrix->get_operation<MyOp>("filter");
auto op = matrix->create_operation<MyOp>("name", ctor_args...);
```

**Direct execution:**
```cpp
auto result   = matrix->execute<MyOp>(input);
auto future   = matrix->execute_async<MyOp>(input);
auto results  = matrix->execute_batch<MyOp>(inputs);
auto [r1, r2] = matrix->execute_parallel<InputType, OpA, OpB>(input);
```

**Two-op type-safe chain:**
```cpp
auto result = matrix->execute_chain<OpA, OpB, InputT, MidT, OutputT>(input);
auto result = matrix->execute_chain_named<OpA, OpB, In, Mid, Out>("name_a", "name_b", input);
```

**Fluent chain (synchronous):**
```cpp
auto result = matrix->with(start_data)
                     .then<OpA>("name_a")
                     .then<OpB>()
                     .to_io();
```

**Fluent chain (asynchronous):**

The matrix owns the `std::future` internally. The chain lambda runs on a background thread. `drain_async()` and the destructor guarantee completion before destruction.

```cpp
matrix->with_async(
    input_datum,
    [](auto chain) {
        return chain.then<SegmentOp>("segment")
                    .then<AttributeOp>("attribute")
                    .then<SortOp>("sort")
                    .to_io();
    },
    [&](Datum<Result> result) {
        // called on worker thread
        my_result = std::move(result);
    });
```

`GrammarAwareComputeMatrix` is a `ComputeMatrix` subclass that carries a `ComputationGrammar`. Retrieve the grammar via `get_grammar()` to add or remove rules at runtime. The `GranularMatrix` type alias is a `GrammarAwareComputeMatrix` with `RegionGroup` as its primary data type.

### ComputationPipeline\<InputType, OutputType\>

Sequential engine. Grammar rules (if any) are evaluated before the named operation chain runs.

```cpp
auto pipeline = std::make_shared<ComputationPipeline<>>();

pipeline->create_operation<MathematicalTransformer<>>("gain")
         .create_operation<SpectralTransformer<>>("spectrum");

pipeline->configure_operation<MathematicalTransformer<>>("gain", [](auto op) {
    op->set_parameter("operation", std::string("gain"));
    op->set_parameter("gain_factor", 2.0);
});

auto result = pipeline->process(input, ExecutionContext{});
```

### ComputationGrammar

Rules match by type, context, priority, and custom predicate. The highest-priority matching rule fires. Rules are sorted on insertion and indexed by `ComputationContext` for fast lookup.

`ComputationContext` values: `TEMPORAL`, `SPECTRAL`, `MATHEMATICAL`, `STRUCTURAL`, and others. Used as a secondary filter alongside the matcher predicate.

**Fluent rule builder:**
```cpp
ComputationGrammar grammar;

grammar.create_rule("normalize_doubles")
    .with_context(ComputationContext::MATHEMATICAL)
    .with_priority(100)
    .with_description("Normalise double-precision channels")
    .matches_type<std::vector<double>>()          // type-based matcher
    .targets_operation<MathematicalTransformer<>>() // for type-based queries
    .with_tags({ "audio", "normalise" })
    .executes([](const std::any& input, const ExecutionContext& ctx) -> std::any {
        auto d = std::any_cast<DataIO>(input);
        // ... transform d ...
        return d;
    })
    .build();
```

**Custom matcher:**
```cpp
grammar.create_rule("large_buffer_gpu")
    .with_context(ComputationContext::MATHEMATICAL)
    .with_priority(80)
    .matches_custom([](const std::any& input, const ExecutionContext& ctx) {
        auto d = safe_any_cast<DataIO>(input);
        return d && d->data.size() > 65536;
    })
    .executes(gpu_dispatch_lambda)
    .build();
```

**`add_operation_rule` shorthand:**

Instantiates, configures, and executes a `ComputeOperation` subclass inside the rule executor automatically.

```cpp
grammar.add_operation_rule<SpectralTransformer<>>(
    "pitch_shift",
    ComputationContext::SPECTRAL,
    UniversalMatcher::create_type_matcher<DataIO>(),
    { { "operation", std::string("pitch_shift") }, { "pitch_ratio", 1.5 } },
    /*priority=*/80);
```

**Direct rule invocation and query:**
```cpp
auto match  = grammar.find_best_match(input_any, ctx);      // -> std::optional<Rule>
auto result = grammar.execute_rule("my_rule", input_any, ctx); // -> std::optional<std::any>
auto names  = grammar.get_rules_by_context(ComputationContext::TEMPORAL);
bool exists = grammar.has_rule("my_rule");
grammar.remove_rule("my_rule");
```

---

## Workflows

Workflows are complete pipelines built on `ComputeMatrix` + `ComputationGrammar`. They are not the only way to use Yantra; they are the reference pattern for how to compose the layers into a self-contained processing unit.

### GranularWorkflow

Offline granular analysis and reconstruction. The pipeline is: segment grains from a `SignalSourceContainer` into a `RegionGroup`, attribute each grain with a scalar feature, sort by that feature, then optionally reconstruct into a `SoundFileContainer` or `DynamicSoundStream`. Each stage is a grammar rule; the matrix executes them in priority order.

**Config:**
```cpp
GranularConfig config {
    .grain_size         = 1024,
    .hop_size           = 512,
    .feature_key        = "spectral_centroid",
    .channel            = 0,
    .ascending          = true,
    .gpu_sort_threshold = 65536,  // 0 = always CPU
    .attribution_context = ComputationContext::SPECTRAL,
    .taper              = {}      // optional per-grain taper for OLA
};
```

**Attribution variants:**

```cpp
// Built-in AnalysisType:
auto ctx = make_granular_context(config, AnalysisType::FEATURE, "spectral_centroid");

// Pre-configured analyzer instance:
auto analyzer = std::make_shared<EnergyAnalyzer<>>();
auto ctx = make_granular_context(config, analyzer, "rms");

// Custom span-level lambda:
AttributeExecutor my_attr = [](std::span<const double> samples, const ExecutionContext&) {
    return *std::max_element(samples.begin(), samples.end());
};
auto ctx = make_granular_context(config, std::move(my_attr));
```

**Output modes:**

| `GranularOutput` | Terminal type |
|---|---|
| `REGION_GROUP` | `Datum<RegionGroup>` - sorted grain regions, no reconstruction. |
| `CONTAINER` | `SoundFileContainer` - concatenative stitch. |
| `CONTAINER_ADDITIVE` | `SoundFileContainer` - overlap-add reconstruction. |
| `STREAM` | `DynamicSoundStream` - concatenative, ready for `SamplingPipeline`. |
| `STREAM_ADDITIVE` | `DynamicSoundStream` - OLA. |

**Synchronous:**
```cpp
// Terminates in RegionGroup (segment + attribute + sort only):
GranularDatum grains = process(container, AnalysisType::FEATURE, config, "centroid");

// Terminates in SoundFileContainer:
auto result = process_to_container(container, AnalysisType::FEATURE, config, "centroid",
                                   GranularOutput::CONTAINER);

// Terminates in DynamicSoundStream:
auto stream = process_to_stream(container, my_attr_lambda, config,
                                GranularOutput::STREAM_ADDITIVE);
```

**Asynchronous:**
```cpp
auto matrix = make_granular_matrix(config.attribution_context,
                                   GranularOutput::STREAM,
                                   config.taper);

process_to_stream_async(
    matrix, container,
    AnalysisType::FEATURE,
    [&](std::shared_ptr<DynamicSoundStream> stream) {
        // called on worker thread; install stream into SamplingPipeline here
        my_sampler = create_sampler_from_stream(stream, 0);
    },
    config, "centroid",
    GranularOutput::STREAM);
```

The matrix owns the async future. Keep it alive until `drain_async()` or destruction.

**Building a custom matrix for the same pattern:**
```cpp
// make_granular_matrix constructs and registers the grammar rules:
auto matrix = make_granular_matrix(
    ComputationContext::SPECTRAL,
    GranularOutput::CONTAINER_ADDITIVE,
    my_taper);

// The grammar can be extended or modified after construction:
matrix->get_grammar()->create_rule("my_extra_step")
    .with_context(ComputationContext::STRUCTURAL)
    .with_priority(60)
    .matches_type<Kakshya::RegionGroup>()
    .executes(my_rule_fn)
    .build();
```

---

## Writing a Compute Shader for Yantra

SSBO shaders: `set = 0` for all compute bindings. Small per-dispatch scalars go in push constants.

```glsl
#version 450
layout(local_size_x = 256) in;

layout(set = 0, binding = 0) readonly  buffer InBuf  { float data[]; } in_buf;
layout(set = 0, binding = 1) writeonly buffer OutBuf { float data[]; } out_buf;

layout(push_constant) uniform PC {
    uint  n_elements;
    float scale;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= pc.n_elements) return;
    out_buf.data[idx] = in_buf.data[idx] * pc.scale;
}
```

Image shaders consumed by `TextureExecutionContext`: binding 0 is the output storage image, binding 1 (or `image_binding`) is the input sampled image.

```glsl
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D out_image;
layout(set = 0, binding = 1)        uniform sampler2D  in_image;

layout(push_constant) uniform PC { float strength; } pc;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4  src   = texelFetch(in_image, coord, 0);
    imageStore(out_image, coord, src * pc.strength);
}
```

---

## Decision Reference

**Which executor?**

| Data | Executor |
|---|---|
| `float`/`double`/`int`/`glm::vec*` SSBOs | `ShaderExecutionContext` |
| `VkImage` (sampled or storage) | `TextureExecutionContext` |
| Custom AoS/SoA buffer layout | Subclass `GpuExecutionContext`; override `extract_inputs()` and `collect_gpu_outputs()`. |
| 2D/3D dispatch grid | Override `calculate_dispatch_size()` in the subclass. |

**Which operation?**

| Situation | Type |
|---|---|
| CPU fallback exists or is wanted during development | Subclass `UniversalTransformer`; attach executor via `set_gpu_backend()`. |
| GPU-only; hard error on CPU path | `GpuTransformer` (or `GpuSorter`, `GpuExtractor`, `GpuAnalyzer`). |
| Multiple passes over the same dispatch (bitonic sort, iterative refinement) | Any operation + `set_multipass(count, updater)` on the executor. |

**Which orchestrator?**

| Situation | Orchestrator |
|---|---|
| Linear fixed sequence | `ComputationPipeline` |
| Branching, parallelism, fluent chaining, async | `ComputeMatrix` |
| Dynamic operation selection on data properties | `GrammarAwareComputeMatrix` + `ComputationGrammar` |
| Single shader; no operation abstraction needed | `exec->execute(datum, ctx)` directly |
| Self-contained domain pipeline (the granular pattern) | `GrammarAwareComputeMatrix` with grammar rules per stage; wrap entry points as free functions |

**When to subclass vs. configure:**

Subclass `GpuExecutionContext` only for custom data extraction or reconstruction. Subclass `GpuDispatchCore` only for custom dispatch sizing or staging not covered by the fluent API. Everything else is configuration.
