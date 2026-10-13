# OpenCL to Yantra: Concept Map and Migration

Both systems dispatch compute kernels on the GPU. The structural difference is that OpenCL exposes buffer handles, queue management and kernel arguments as explicit objects you manage. Yantra wraps those concerns inside executors and operations, so you describe what the computation is rather than how to stage and synchronise it.

## Why Yantra over OpenCL

The raw dispatch capability is comparable. Everything else is not.

- **Vulkan 1.3 directly.** Yantra sits on MayaFlux's Vulkan backend, not a compatibility layer that reduces to the lowest common denominator across vendors. Compute and rendering use the same command infrastructure, so they share synchronisation primitives, memory and pipeline barriers without translation.

- **The rest of MayaFlux is already there.** A compute result is a `Datum`. It can go straight to a node graph, into a buffer that drives a `GraphicsRoutine`, to a `SoundRoutine` on the next buffer cycle, or to `IOManager` for file output. There is no serialisation step, no format conversion and no glue code. An OpenCL program lives outside this: it reads and writes memory that you then thread back into whatever runtime embeds it.

- **Kinesis is already written.** Kinesis is MayaFlux's pure-math library: discrete algorithms (spectral transforms, convolution, sorting, tapering, extraction), spatial indexing (`SpatialIndex`, proximity graphs, hit testing), geometry (marching cubes, motion curves, morphology, 2D geometry, primitives), stochastic functions and more. All of it is callable from an operation's CPU path. You compose from what exists instead of implementing and maintaining an FFT or a sort.

- **Expression scales with the problem.** A single shader that transforms a buffer is a few lines with the fluent executor API. A multi-pass pipeline with grammar-driven operation selection, async reconstruction and overlap-add output is the same types composed differently. The grammar system lets you describe decision logic as data rather than nested conditionals, so it can be inspected, changed and tested apart from the operations it selects.

- **Direction of travel.** OpenCL is in maintenance mode, and the ecosystem has fragmented across vendor extensions, SYCL, HIP and Metal Compute. Vulkan compute is where the industry has consolidated, and MayaFlux builds on it directly. Mesh and task shader stages are already recognised by the shader loader.

- **Lila.** The JIT live coding environment evaluates C++ that Yantra can express: executor configuration, operation construction, grammar rule registration and matrix chain composition. You can change a processing pipeline at runtime with no recompile and no restart.

---

## Concept map

| OpenCL | Yantra | Notes |
|---|---|---|
| `cl_program` / kernel source | `.comp` GLSL file compiled to SPIR-V | `GpuComputeConfig::shader_path`, or a `shader_id` from `load_shader`. A `ShaderSpec` generates one without a file. |
| `cl_command_queue` | Implicit in `GpuDispatchCore` | Managed by `GpuResourceManager` |
| `clCreateBuffer` | `exec->input(data)` / `output(bytes)` / `in_out(data)` | Staging is automatic on `execute()` |
| `clSetKernelArg` | Fluent binding calls on the executor | Bindings by direction and index |
| Push constants / kernel scalars | `exec->push(struct)` | Copies the struct into the push constant block |
| `NDRange` / work-group size | `GpuComputeConfig::workgroup_size` plus automatic dispatch sizing | Override `calculate_dispatch_size()` for non-1D grids |
| `clEnqueueNDRangeKernel` | `exec->execute(datum, ctx)` | Synchronous: stages, dispatches and reads back |
| `clEnqueueNDRangeKernel` (non-blocking) | `exec->dispatch_async(datum)` | Returns a `FenceID` |
| Events / fences | `ShaderFoundry::is_fence_signaled(fence)`, then `release_fence(fence)` | Poll until signaled, call `collect_result()`, release the fence |
| `clEnqueueReadBuffer` | Automatic in `execute()`; `collect_result()` after `dispatch_async` | `execute()`: primary output in `result.data`, named bindings via `read_output<T>(result, binding)`. `collect_result()`: `GpuChannelResult` with `primary` floats and `aux` bytes keyed by binding. |
| `CL_MEM_READ_ONLY` | `GpuBufferBinding::Direction::INPUT` | |
| `CL_MEM_WRITE_ONLY` | `GpuBufferBinding::Direction::OUTPUT` | |
| `CL_MEM_READ_WRITE` | `GpuBufferBinding::Direction::INPUT_OUTPUT` | |
| `clCreateImage2D` / image kernel args | `TextureExecutionContext` | Manages `VkImage` lifecycle, staging and layout transitions |
| Multiple kernels in sequence | `ComputationPipeline` or the `ComputeMatrix` fluent chain | |
| Several different kernels, one submission, with barriers | `ExecutionMode::DEPENDENCY` with `DependencyParams` stages | Each `DependencyStage` carries its own config and hazard list; `async` returns a fence |
| Multi-pass loop with per-pass kernel arg updates | `set_multipass(count, updater)` | Batched through `GpuResourceManager::dispatch_batched`; `CHAINED_INDIRECT` takes pass counts from a GPU buffer |
| Event dependencies between kernels | `ComputeMatrix` async chain | `with_async(input, chain_lambda, on_complete)` |
| `clReleaseMemObject` / `clReleaseKernel` | RAII, nothing to release by hand | Fences from `dispatch_async` are the exception |

---

## Step 1: Write the shader in GLSL

Yantra uses Vulkan-style GLSL compute shaders compiled to SPIR-V. All buffer bindings use `set = 0`. Small per-dispatch scalars go in push constants.

**OpenCL kernel:**
```opencl
__kernel void multiply(__global float* in, __global float* out, float scale, uint n) {
    uint idx = get_global_id(0);
    if (idx < n) out[idx] = in[idx] * scale;
}
```

**Yantra GLSL (`multiply.comp`):**
```glsl
#version 450
layout(local_size_x = 256) in;

layout(set = 0, binding = 0) readonly  buffer InBuf  { float data[]; } in_buf;
layout(set = 0, binding = 1) writeonly buffer OutBuf { float data[]; } out_buf;

layout(push_constant) uniform PC {
    float scale;
    uint  n;
} pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= pc.n) return;
    out_buf.data[idx] = in_buf.data[idx] * pc.scale;
}
```

Key substitutions:

- `__global` becomes `layout(set=0, binding=N) buffer`.
- `get_global_id(0)` becomes `gl_GlobalInvocationID.x`.
- Kernel scalar arguments become push constant fields.

### Other ways to create the shader

A `.comp` file is one of four routes to a compiled shader. `ShaderFoundry::load_shader` returns a `ShaderID` for all of them, and an executor can take that ID directly through `GpuComputeConfig::shader_id`, which wins over `shader_path` when both are set.

| Route | You write | Compiled with |
|---|---|---|
| GLSL file | `foo.comp` (`.spv` also loads) | `shaderc`, or loaded as-is for `.spv` |
| GLSL source string | `foundry.load_shader("#version 460 ...", ShaderStage::COMPUTE)` | `shaderc` |
| `ShaderSpec` with a `KernelOp` | A declarative description: bindings, push constants, a named operation | SPIR-V assembly via SPIRV-Tools, no GLSL involved |
| `ShaderSpec` with `MF_KERNEL` | The same description plus a C++-looking lambda body | Generated GLSL, then `shaderc` |

**A shader name is a path, so the search order applies.** A bare filename is resolved against the project and working-directory shader folders first, then the library's. A project shader of the same name overrides a shipped one, with a warning. Use `add_include_directory` and `add_define` on the foundry for `#include` paths and preprocessor definitions, and `hot_reload` to recompile a file.

#### ShaderSpec: shaders without GLSL

`ShaderSpec` is a plain value type that describes a compute shader. `ShaderFoundry::load_shader(spec)` generates, compiles and caches it by content hash, so the same spec returns the same `ShaderID` at no cost. The result is always a compute shader with entry point `main`. It is the closest thing to OpenCL's built-in functions: you name the operation instead of writing the kernel.

```cpp
using namespace Portal::Graphics;

auto spec = ShaderSpec::Assemble {}
    .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
    .pc("gain")
    .pc("offset")
    .op(KernelOp::ScaleOffset)
    .build();
```

The builder takes:

- **`ssbo(name, direction, format)`** for a storage buffer, **`texture(name)`** for a sampled image and **`storage_image(name, direction)`** for a storage image. Bindings are numbered in declaration order.
- **`pc(name)`** or **`pc(name, format)`** for a push constant field. The default format is float, and fields are consumed in declaration order as operands.
- **`tmpl(KernelTemplate)`** for the shape of the kernel: `Elementwise` (the default), `Reduction`, `Stencil`, `GeometryEmit`, `BitonicSort`, `Convolve2D` or `Scan`.
- **`op(KernelOp)`** for the operation. The families are arithmetic (`Scale`, `ScaleOffset`, `Fma`, `Clip`, `Abs`, `Sqrt`, ...), transcendental (`Sin`, `Exp`, `Log`, `Tanh`, ...), two-buffer (`Add`, `Multiply`, `Mix`, `Pow`, `SmoothStep`, ...), reductions and scans (`Sum`, `Max`, `MaxIndex`, `ScanSum`), image operations (`CompareGE`, `ChannelDot`, ...) and the stencil blend `WeightedBlend`.
- **`workgroup(x, y, z)`** to override the default `{ 256, 1, 1 }`.

To run a spec through an executor, `Yantra/Executors/ShaderSpecBinding.hpp` derives both halves for you, so the executor's binding indices always match the spec:

```cpp
auto cfg      = config_from_spec(spec);
auto bindings = bindings_from_spec(spec);

auto exec = std::make_shared<ShaderExecutionContext<>>(cfg, bindings);
```

`config_from_spec` loads the shader and fills in `shader_id`, `workgroup_size` and `push_constant_size`.

#### ShaderSpec with MF_KERNEL: your own body

When no named operation fits, supply the kernel body as a lambda. `MF_KERNEL` captures the lambda's text. It is never executed as C++, and its body is placed into generated GLSL.

```cpp
auto spec = ShaderSpec::Assemble {}
    .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
    .pc("gain")
    .kernel(MF_KERNEL([](float* sig, float gain, uint32_t i) {
        sig[i] = sig[i] * gain;
    }))
    .build();
```

The lambda parameter names must match the declared bindings first, then the push constant fields, in declaration order. An optional trailing `uint32_t i` is the invocation index. When a kernel is set, `op` is ignored. `.function(return_type, name, params, body)` adds helper functions before `main()`, and needs a kernel because only the GLSL path supports them.

#### Choosing a route

- A shader you will read, edit and keep in version control is a `.comp` file.
- A pointwise, reduction or scan operation that already has a `KernelOp` is a `ShaderSpec`. It needs no GLSL.
- A short custom body over a few buffers is `MF_KERNEL`.
- Anything that needs shared-memory choreography beyond the built-in templates belongs in a `.comp` file.

---

## Step 2: Configure the executor

`GpuComputeConfig` and `ShaderExecutionContext` replace `clCreateContext`, `clCreateProgramWithSource` and `clBuildProgram`. The engine initialises the GPU context, so you never manage a device or queue.

```cpp
#include <MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp>

GpuComputeConfig cfg;
cfg.shader_path        = "multiply.comp";
cfg.workgroup_size     = { 256, 1, 1 };
cfg.push_constant_size = sizeof(MyPC);

auto exec = std::make_shared<ShaderExecutionContext<>>(cfg);
```

---

## Step 3: Bind buffers

The fluent binding API replaces `clSetKernelArg`. Indices increment sequentially unless you give one explicitly.

```cpp
std::vector<float> input_data = ...;

struct MyPC { float scale; uint32_t n; } pc { 2.0f, (uint32_t)input_data.size() };

exec->input(input_data, GpuBufferBinding::ElementType::FLOAT32)
     .output(input_data.size() * sizeof(float), GpuBufferBinding::ElementType::FLOAT32)
     .push(pc);
```

`ElementType` controls how the staging layer interprets the data before upload: `FLOAT32`, `UINT32`, `INT32`, `PASSTHROUGH`, `IMAGE_STORAGE` and `IMAGE_SAMPLED`. There is no vec-typed `ElementType`. For `glm::vec3` positions or other structured types, use `PASSTHROUGH` and pre-stage the raw bytes directly to bypass the double-conversion path.

For in-place modification, replace an `input` and `output` pair with `in_out`:

```cpp
exec->in_out(data, GpuBufferBinding::ElementType::FLOAT32);
```

---

## Step 4: Execute

The default `ShaderExecutionContext<>` takes and returns a `DataIO`, which is a `Datum` of `DataVariant`s.

**Synchronous** (replaces `clEnqueueNDRangeKernel` plus `clFinish`):

```cpp
DataIO input_datum(std::vector<Kakshya::DataVariant> { std::move(input_data) });
auto result = exec->execute(input_datum, ExecutionContext {});
```

`execute()` handles staging, dispatch sizing from the element count, submission and readback in one call.

**Asynchronous** (replaces enqueue plus event polling):

```cpp
auto& foundry = Portal::Graphics::get_shader_foundry();

auto fence = exec->dispatch_async(input_datum);
// ... do other work ...
while (!foundry.is_fence_signaled(fence)) { }

auto raw = exec->collect_result();
foundry.release_fence(fence);
```

`release_fence` is required. Each fence from `dispatch_async` holds a `VkFence` and a command buffer slot until it is released.

---

## Step 5: Read results

After `execute()`, the primary output is `result.data[0]`, a `DataVariant`. Float, `uint8`, `uint16` and `uint32` input comes back in its native vector type. Anything else comes back as `std::vector<double>`.

```cpp
auto floats = std::get<std::vector<float>>(result.data[0]);
```

Auxiliary output bindings, when you declare several output buffers:

```cpp
auto edge_data = ShaderExecutionContext<>::read_output<float>(result, 2);
auto count     = ShaderExecutionContext<>::read_output<uint32_t>(result, 3)[0];
```

After `dispatch_async`, `collect_result()` returns a `GpuChannelResult` instead: `raw.primary` is the primary output as floats and `raw.aux[binding]` is the raw bytes of an auxiliary output. `read_output` applies to the `Datum` that `execute()` returns, not to this struct.

---

## Pattern translations

### In-place modification

```opencl
// OpenCL: same cl_mem as both input and output
clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf);
```

```cpp
// Yantra:
exec->in_out(data, GpuBufferBinding::ElementType::FLOAT32);
```

### Multiple kernels in sequence

```opencl
// OpenCL: manual event chain
clEnqueueNDRangeKernel(queue, kernel_a, ..., nullptr, nullptr, &event_a);
clEnqueueNDRangeKernel(queue, kernel_b, ..., 1, &event_a, &event_b);
```

```cpp
// Yantra: ComputationPipeline (sequential, fixed order)
auto pipeline = std::make_shared<ComputationPipeline<>>();
pipeline->create_operation<GpuTransformer<>>("pass_a")
         .create_operation<GpuTransformer<>>("pass_b");
auto result = pipeline->process(input, ExecutionContext {});

// Or: ComputeMatrix fluent chain (composable, async-capable)
auto result = matrix->with(input)
                     .then<PassAOp>("pass_a")
                     .then<PassBOp>("pass_b")
                     .to_io();
```

When the passes are different shaders that should run back to back in one submission, with barriers between them, use `ExecutionMode::DEPENDENCY` instead. It takes a list of `DependencyStage`s, each with its own `GpuComputeConfig`, a `stage_fn` that sets up bindings and data, and a `hazard_fn` that names the resources the next stage depends on.

### Multi-pass dispatch (such as bitonic sort)

```opencl
// OpenCL: loop over clEnqueueNDRangeKernel with updated kernel args
for (uint32_t pass = 0; pass < n_passes; ++pass) {
    clSetKernelArg(kernel, 0, sizeof(uint32_t), &pass);
    clEnqueueNDRangeKernel(queue, kernel, ...);
}
```

```cpp
// Yantra: set_multipass on the executor
exec->in_out(data, GpuBufferBinding::ElementType::FLOAT32)
     .set_multipass(n_passes, [](uint32_t pass, void* pc_ptr) {
         static_cast<MyPC*>(pc_ptr)->pass_index = pass;
     });
auto result = exec->execute(input_datum, ExecutionContext {});
```

`set_multipass` switches the context to `ExecutionMode::CHAINED`, which dispatches through `GpuResourceManager::dispatch_batched`. Passes are grouped according to `ChainedParams::passes_per_batch` rather than issued one at a time from your loop. `CHAINED_INDIRECT` is the variant that takes its pass count from a GPU-side buffer.

### Image processing

```opencl
// OpenCL: cl_image2D creation, image kernel args, explicit format
cl_image_format fmt = { CL_RGBA, CL_FLOAT };
cl_mem img_in  = clCreateImage2D(ctx, CL_MEM_READ_ONLY, &fmt, w, h, 0, nullptr, &err);
cl_mem img_out = clCreateImage2D(ctx, CL_MEM_WRITE_ONLY, &fmt, w, h, 0, nullptr, &err);
```

```cpp
// Yantra: TextureExecutionContext manages VkImage lifecycle
TextureExecutionContext tex_ctx(
    GpuComputeConfig { "image_pass.comp", { 16, 16, 1 }, sizeof(MyPC) },
    Portal::Graphics::ImageFormat::RGBA8);

tex_ctx.stage_container(my_texture_container, /*layer=*/0);
auto result = tex_ctx.execute(DataIO {}, ExecutionContext {});
// result.container holds the output TextureContainer
```

Binding 0 is the output storage image and the input sampled image lands at `image_binding` (default 1). `OutputMode::IMAGE` keeps the result on the GPU for the next shader instead of downloading it.

### Processing a sub-range

OpenCL passes an offset and a count as kernel arguments and does the arithmetic itself. Yantra carries slice intent on the data instead. A `Region` such as `Region::time_span(1000, 2000)` describes the slice, a `Datum<Region>` (`RegionIO`) or a `RegionGroup` carries it between operations, and a container-backed `Datum` keeps the source it points into. Operations that work on regions are covered in the [Yantra Compute Guide](GPGPU-Guide.md). A raw executor, by contrast, sizes its dispatch from the element count of what you give it, so a sub-range there means giving it the sub-range.

---

## Raw GPU access: below the Yantra layer

Yantra's executor API assumes data flows through `Datum` and that you want staging, dispatch sizing and readback managed for you. When that does not fit, MayaFlux exposes two lower levels. Reach for them for custom struct layouts, outputs that never come back to the CPU, geometry written straight into a vertex buffer, or dispatch tightly coupled to the frame loop.

### ComputeProcessor: buffer-attached compute

`ComputeProcessor` (in `MayaFlux::Buffers`) extends `ShaderProcessor` and attaches to a `VKBuffer` as a processor in a `BufferProcessingChain`. It owns a compute pipeline, a descriptor set layout and the dispatch logic. Bindings are declared by name in `ShaderConfig::bindings` and resolved against named `VKBuffer`s. The descriptor set is rebuilt when a buffer or binding changes, and dispatch is triggered by the chain's process cycle instead of a `Datum` call.

This is the right level when the compute output is the buffer's own content: the GPU writes into the vertex buffer directly, with no readback and no `Datum` envelope. `SDFMeshProcessor` is the canonical example.

- It dispatches `mc_emit.comp`, which writes triangle vertices directly into a `Usage::VERTEX` buffer.
- It then reads back only the atomic vertex counter, from a host-visible buffer, to learn the vertex count.
- The corner grid it reads is either evaluated on the CPU (`Kinesis::SpatialField`) or, in GPU-field mode, by `SDFFieldProcessor` running as a preprocessor on the GPU.
- Either way, no CPU copy of vertex data exists.

```cpp
// SDFMeshProcessor bindings, declared once at construction:
m_config.bindings["sdf_grid"]   = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
m_config.bindings["edge_table"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
m_config.bindings["tri_table"]  = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
m_config.bindings["vertices"]   = ShaderBinding(0, 3, vk::DescriptorType::eStorageBuffer);
m_config.bindings["counter"]    = ShaderBinding(0, 4, vk::DescriptorType::eStorageBuffer);

set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
```

For a custom processor, subclass `ComputeProcessor` and configure it in the constructor:

```cpp
class MyFieldProcessor : public ComputeProcessor {
public:
    MyFieldProcessor()
        : ComputeProcessor("my_field.comp", 64)
    {
        m_config.push_constant_size = sizeof(MyPC);
        m_config.bindings["input_ssbo"]  = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
        m_config.bindings["output_ssbo"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
        set_workgroup_size(64, 1, 1);
        set_dispatch_mode(ShaderDispatchConfig::DispatchMode::ELEMENT_COUNT);
    }

protected:
    std::array<uint32_t, 3> calculate_dispatch_size(
        const std::shared_ptr<VKBuffer>& buffer) override
    {
        auto n = buffer->get_dimensions()[0].size;
        return { (uint32_t)((n + 63) / 64), 1, 1 };
    }
};

auto processor = std::make_shared<MyFieldProcessor>();
processor->bind_buffer("input_ssbo",  input_vk_buf);
processor->bind_buffer("output_ssbo", output_vk_buf);
output_vk_buf->set_default_processor(processor);
```

`calculate_dispatch_size` is the override point for a 3D grid, a custom stride or any sizing that is not the element count. The default `ELEMENT_COUNT` mode derives its count from `get_dimensions()[0].size`.

`DescriptorBindingsProcessor` goes further. It binds node outputs (scalar, vector, matrix or structured) directly to UBO and SSBO descriptors, so node graph values reach shader uniforms every frame with no manual staging.

```cpp
using Portal::Graphics::DescriptorRole;

auto dbp = std::make_shared<DescriptorBindingsProcessor>(shader_config);
dbp->bind_scalar_node("freq",     freq_node,     "params",    0, DescriptorRole::UNIFORM);
dbp->bind_vector_node("spectrum", spectrum_node, "spec_data", 0, DescriptorRole::STORAGE);
```

Dispatch modes available on `ComputeProcessor`:

| Mode | Behaviour |
|---|---|
| `ELEMENT_COUNT` | Groups = ceil(element count / workgroup_x). The default. |
| `BUFFER_SIZE` | Groups = ceil(byte size / workgroup_x). |
| `MANUAL` | Fixed group counts set with `set_manual_dispatch(x, y, z)`. |
| `CUSTOM` | A lambda: `(shared_ptr<VKBuffer>) -> array<uint32_t, 3>`. |

### ComputePress: raw Vulkan compute

`Portal::Graphics::ComputePress` is the compute-specific command layer. It wraps `ShaderFoundry` command buffers and handles pipeline creation (including auto-reflection from SPIR-V), descriptor set allocation, push constant upload, and `vkCmdDispatch` and `vkCmdDispatchIndirect`. Use it for full control over command recording, barriers, or indirect dispatch driven by a GPU-side buffer.

```cpp
#include <MayaFlux/Portal/Graphics/ComputePress.hpp>

using namespace Portal::Graphics;

auto& press   = get_compute_press();
auto& foundry = get_shader_foundry();

// Create a pipeline with an explicit descriptor layout:
auto shader_id   = foundry.load_shader("my_kernel.comp");
auto pipeline_id = press.create_pipeline(
    shader_id,
    { { DescriptorBindingInfo { .set = 0, .binding = 0, .type = vk::DescriptorType::eStorageBuffer },
        DescriptorBindingInfo { .set = 0, .binding = 1, .type = vk::DescriptorType::eStorageBuffer } } },
    sizeof(MyPC));

// Or auto-reflect from SPIR-V:
auto pipeline_id = press.create_pipeline_auto(shader_id, sizeof(MyPC));

// Allocate descriptors for this pipeline:
auto ds_ids = press.allocate_pipeline_descriptors(pipeline_id);

// Point them at real buffers:
foundry.update_descriptor_buffer(ds_ids[0], 0, vk::DescriptorType::eStorageBuffer,
    input_vk_buf->get_buffer(), 0, input_vk_buf->get_size());
foundry.update_descriptor_buffer(ds_ids[0], 1, vk::DescriptorType::eStorageBuffer,
    output_vk_buf->get_buffer(), 0, output_vk_buf->get_size());

// Record and dispatch:
auto cmd_id = foundry.begin_commands(ShaderFoundry::CommandBufferType::COMPUTE);

MyPC pc { .n_elements = count, .scale = 2.0f };
press.bind_all(cmd_id, pipeline_id, ds_ids, &pc, sizeof(pc));
press.dispatch(cmd_id, (count + 255) / 256, 1, 1);

foundry.end_commands(cmd_id);
foundry.submit_and_wait(cmd_id);
```

`bind_all()` chains `bind_pipeline`, `bind_descriptor_sets` and `push_constants` in one call. `dispatch_indirect` takes its dimensions from a GPU-side buffer. `update_descriptor_buffers` writes several slots in one Vulkan call, and `record_sequence` records several pipeline dispatches into one open command buffer with a barrier between consecutive stages. For work that should not block, `submit_async` returns a fence, subject to the same `release_fence` rule as `dispatch_async`.

### Which level to use

| Situation | Level |
|---|---|
| Data in a `Datum`, result back to the CPU or into a container | Yantra `ShaderExecutionContext` |
| Output stays GPU-resident in a `VKBuffer` (vertex data, field buffers) | `ComputeProcessor` subclass |
| Node outputs must drive shader uniforms every frame | `DescriptorBindingsProcessor` |
| Full command control, indirect dispatch, or barriers between passes | `ComputePress` directly |

The levels are not isolated. A `ComputeProcessor` can share `VKBuffer`s with a Yantra executor that reads them back into a `Datum` for analysis. A `ComputePress` dispatch can write into a buffer that a `GraphicsRoutine` binds as a vertex input in the same frame. The infrastructure is shared and only the abstraction level differs.

For how data moves between the CPU and GPU at each level, see the [CPU-GPU IO map](GPU-CPU-IO-Map.md).

---

## What Yantra does not replace

Yantra covers offline and ahead-of-time compute, plus async dispatch you drive yourself. It does not cover:

- **Real-time dispatch tied to the audio or graphics clock.** Use `GraphicsRoutine`, `GpuComputeNode` or direct Vulkan from a scheduled coroutine.

- **Rendering pipelines.** The graphics side is `Portal::Graphics` with `RenderFlow`, vertex and fragment shaders, and the buffer rendering subsystem.

- **Streaming data that updates every buffer cycle.** Containers like `AudioOutputContainer` and `VideoStreamContainer` feed live data into the `Datum::container` field, but Vruta drives the dispatch timing, not `ComputeMatrix`.

For these cases, Yantra's data types (`Datum`, containers, `DataVariant`) cross the boundary freely. The executor and matrix layers stay on the offline and async side.
