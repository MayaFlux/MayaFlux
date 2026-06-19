# OpenCL to Yantra: Concept Map and Migration

Both systems dispatch compute kernels on the GPU. The structural difference is that OpenCL exposes buffer handles, queue management, and kernel arguments as explicit objects you manage. Yantra wraps those concerns inside executors and operations so you describe what the computation is rather than how to stage and synchronise it.

## Why Yantra Over OpenCL

The raw dispatch capability is comparable. Everything else is not.

**Vulkan 1.3+ directly.** Yantra sits on top of MayaFlux's Vulkan backend, which uses dynamic rendering, descriptor indexing, and timeline semaphores throughout. You are not working through a compatibility layer or an abstraction that must reduce to the lowest common denominator across vendors. You have direct access to the same command infrastructure that drives the graphics and geometry pipelines, which means compute and rendering share synchronisation primitives, memory, and pipeline barriers without translation.

**The rest of MayaFlux is already there.** A compute result in Yantra is a `Datum`. That `Datum` can be handed directly to a node graph, written into a buffer that drives a `GraphicsRoutine`, consumed by a `SoundRoutine` on the next buffer cycle, or passed to `IOManager` for file output. There is no serialisation step, no format conversion, and no glue code between GPU compute and the rest of the system. Audio, video, geometry, and control data are all the same type of thing and can interact freely. An OpenCL program lives outside this; it reads and writes memory that then has to be manually threaded back into whatever runtime it was embedded in.

**Kinesis is already written.** Kinesis is MayaFlux's pure-math library: discrete algorithms (FFT, convolution, sorting, tapering, spectral analysis, extraction), spatial indexing (proximity graphs, `SpatialIndex`, hit testing), geometry (marching cubes, motion curves, morphology, 2D geometry, primitives), stochastic functions, and more. These are callable from any operation's CPU path. You do not have to implement, test, or maintain an FFT or a radix sort to get GPU-accelerated granular synthesis off the ground; you compose from what exists.

**Expression scales with the problem.** A single shader that transforms a buffer is three lines of C++ with the fluent executor API. A multi-pass pipeline with grammar-driven operation selection, async reconstruction, and overlap-add output is still the same types composed differently. The abstraction does not cap out at simple cases or become unwieldy at complex ones. The grammar system in particular lets you describe decision logic as data rather than nested conditionals, and that logic is inspectable, modifiable, and testable independently of the operations it selects.

**Future-proofing.** OpenCL is effectively in maintenance mode. The ecosystem has fragmented across vendor extensions, SYCL, HIP, and Metal Compute. Vulkan Compute is the direction the industry has consolidated around, and MayaFlux tracks it directly. New Vulkan features - mesh shaders, ray tracing compute, cooperative matrices - are accessible through the same infrastructure Yantra already uses. Porting an OpenCL program to Yantra is also a migration away from a shrinking ecosystem toward one with active driver and hardware investment from every major GPU vendor.

**Lila.** The JIT live coding environment can evaluate any C++ that Yantra can express, including executor configuration, operation construction, grammar rule registration, and matrix chain composition. You can modify a processing pipeline at runtime with no recompile and no restart. That is not a thing OpenCL programs can do.

---

---

## Concept Map

| OpenCL | Yantra | Notes |
|---|---|---|
| `cl_program` / kernel source | `.comp` GLSL file compiled to SPIR-V | `GpuShaderConfig::shader_path` |
| `cl_command_queue` | Implicit in `GpuDispatchCore` | Managed by `GpuResourceManager` |
| `clCreateBuffer` | `exec->input(data)` / `output(bytes)` / `in_out(data)` | Staging is automatic on `execute()` |
| `clSetKernelArg` | Fluent binding calls on the executor | Bindings by direction and index |
| Push constants / kernel scalars | `exec->push(struct)` | Copies struct into push constant block |
| `NDRange` / work-group size | `GpuShaderConfig::workgroup_size` + automatic dispatch sizing | Override `calculate_dispatch_size()` for non-1D grids |
| `clEnqueueNDRangeKernel` | `exec->execute(datum, ctx)` (sync) | Drives staging, dispatch, and readback |
| `clEnqueueNDRangeKernel` (non-blocking) | `exec->dispatch_async(datum)` | Returns `FenceID` |
| Events / fences | `Portal::Graphics::FenceID` + `ShaderFoundry::is_fence_signaled(fence)` | Poll until signaled, then call `collect_result()` |
| `clEnqueueReadBuffer` | Automatic in `execute()`; explicit via `collect_result()` | Primary buffer in `result.data`; named bindings via `read_output<T>(result, binding)` |
| `CL_MEM_READ_ONLY` | `GpuBufferBinding::Direction::INPUT` | |
| `CL_MEM_WRITE_ONLY` | `GpuBufferBinding::Direction::OUTPUT` | |
| `CL_MEM_READ_WRITE` | `GpuBufferBinding::Direction::INPUT_OUTPUT` | |
| `clCreateImage2D` / image kernel args | `TextureExecutionContext` | Manages `VkImage` lifecycle, staging, and layout transitions |
| Multiple kernels in sequence | `ComputationPipeline` or `ComputeMatrix` fluent chain | |
| Event dependencies between kernels | `ComputeMatrix` async chain | `with_async(input, chain_lambda, on_complete)` |
| `clReleaseMemObject` / `clReleaseKernel` | RAII; nothing to release manually | |

---

## Step 1: Write the Shader in GLSL

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

Key substitutions: `__global` becomes `layout(set=0, binding=N) buffer`; `get_global_id(0)` becomes `gl_GlobalInvocationID.x`; kernel scalar arguments become push constant fields.

---

## Step 2: Configure the Executor

Replace `clCreateContext` / `clCreateProgramWithSource` / `clBuildProgram` with `GpuShaderConfig` + `ShaderExecutionContext`. The GPU context is initialised by the engine; you never manage a device or queue.

```cpp
#include <MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp>

GpuShaderConfig cfg;
cfg.shader_path        = "multiply.comp";
cfg.workgroup_size     = { 256, 1, 1 };
cfg.push_constant_size = sizeof(MyPC);

auto exec = std::make_shared<ShaderExecutionContext<>>(cfg);
```

---

## Step 3: Bind Buffers

Replace `clSetKernelArg` with the fluent binding API. Indices increment sequentially unless specified explicitly.

```cpp
std::vector<float> input_data = ...;

struct MyPC { float scale; uint32_t n; } pc { 2.0f, (uint32_t)input_data.size() };

exec->input(input_data, GpuBufferBinding::ElementType::FLOAT32)
     .output(input_data.size() * sizeof(float), GpuBufferBinding::ElementType::FLOAT32)
     .push(pc);
```

`ElementType` controls how the staging layer interprets the data before upload. For `glm::vec3` positions or other structured types, use `VEC3_F32`, `VEC4_F32`, etc., to bypass the double-conversion path.

For in-place modification, replace an `input` + `output` pair with `in_out`:
```cpp
exec->in_out(data, GpuBufferBinding::ElementType::FLOAT32);
```

---

## Step 4: Execute

**Synchronous (replaces `clEnqueueNDRangeKernel` + `clFinish`):**
```cpp
Datum<std::vector<float>> input_datum(std::move(input_data));
auto result = exec->execute(input_datum, ExecutionContext{});
// result.data: primary buffer readback as std::vector<double>
```

`execute()` handles staging, dispatch sizing from element count, kernel submission, and readback in one call.

**Asynchronous (replaces enqueue + event polling):**
```cpp
auto fence = exec->dispatch_async(input_datum);
// ... do other work ...
while (!Portal::Graphics::ShaderFoundry::is_fence_signaled(fence)) {}
auto raw = exec->collect_result();
```

---

## Step 5: Read Results

Primary output buffer:
```cpp
auto floats = std::get<std::vector<float>>(result.data.data[0]);
// or access result.data directly as std::vector<double> (the default readback type)
```

Named auxiliary output bindings (when multiple output SSBOs are declared):
```cpp
auto edge_data = ShaderExecutionContext<>::read_output<float>(result, 2);
auto count     = ShaderExecutionContext<>::read_output<uint32_t>(result, 3)[0];
```

---

## Pattern Translations

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
auto result = pipeline->process(input, ExecutionContext{});

// Or: ComputeMatrix fluent chain (composable, async-capable)
auto result = matrix->with(input)
                     .then<PassAOp>("pass_a")
                     .then<PassBOp>("pass_b")
                     .to_io();
```

### Multi-pass dispatch (e.g. bitonic sort)

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
auto result = exec->execute(input_datum, ExecutionContext{});
```

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
    GpuShaderConfig { "image_pass.comp", { 16, 16, 1 }, sizeof(MyPC) },
    Portal::Graphics::ImageFormat::RGBA8);

tex_ctx.stage_container(my_texture_container, /*layer=*/0);
auto result = tex_ctx.execute(DataIO{}, ExecutionContext{});
// result.container holds the output TextureContainer
```

### Processing a sub-range

```opencl
// OpenCL: manual offset + size arithmetic passed as kernel args
uint32_t offset = 1000, count = 1000;
clSetKernelArg(kernel, 2, sizeof(uint32_t), &offset);
clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_work, ...);
```

```cpp
// Yantra: Region system carries slice semantics in the Datum
Datum<std::vector<double>> full(data);
Region slice = Region::time_span(1000, 2000);
// Pass slice context to an operation or extract via container
```

---

## Raw GPU Access: Below the Yantra Layer

Yantra's executor API assumes data flows through `Datum` and that you want staging, dispatch sizing, and readback managed for you. When those assumptions do not fit - custom struct layouts, buffer-resident outputs that never come back to CPU, geometry written directly into a vertex buffer, or dispatch that is tightly coupled to the frame loop - MayaFlux exposes two lower levels.

### ComputeProcessor: Buffer-attached Compute

`ComputeProcessor` (in `Buffers::Shaders`) extends `ShaderProcessor` and attaches to a `VKBuffer` as a processor in a `DataProcessingChain`. It owns a compute pipeline, a descriptor set layout, and dispatch logic. Bindings are declared by name in `ShaderConfig::bindings` and resolved against named `VKBuffer` instances. The descriptor set is rebuilt when a buffer or binding changes; dispatch is triggered by the chain's process cycle rather than by a `Datum` call.

This is the right level for cases where the compute output is the buffer's own content - the GPU writes into the vertex buffer directly, no readback, no `Datum` envelope. `SDFMeshProcessor` is the canonical example: it evaluates a `Kinesis::SpatialField` into a corner grid buffer on CPU, dispatches `mc_emit.comp` which writes triangle vertices directly into a `Usage::VERTEX` buffer, then reads back only the atomic counter to get the vertex count. No CPU copy of vertex data at any point.

```cpp
// SDFMeshProcessor bindings - declared once at construction:
m_config.bindings["sdf_grid"]  = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
m_config.bindings["edge_table"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
m_config.bindings["tri_table"]  = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
m_config.bindings["vertices"]   = ShaderBinding(0, 3, vk::DescriptorType::eStorageBuffer);
m_config.bindings["counter"]    = ShaderBinding(0, 4, vk::DescriptorType::eStorageBuffer);

set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
```

For a custom processor, subclass `ComputeProcessor` and configure in the constructor:

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
        // Override for 3D grid, custom stride, or non-element-count sizing.
        auto n = buffer->get_element_count();
        return { (uint32_t)((n + 63) / 64), 1, 1 };
    }
};

// Attach:
auto processor = std::make_shared<MyFieldProcessor>();
processor->bind_buffer("input_ssbo",  input_vk_buf);
processor->bind_buffer("output_ssbo", output_vk_buf);
output_vk_buf->set_default_processor(processor);
```

`DescriptorBindingsProcessor` extends this further: it can bind node outputs (scalar, vector, matrix, or structured) directly to UBO/SSBO descriptors, so node graph values flow into shader uniforms per frame without manual staging.

```cpp
auto dbp = std::make_shared<DescriptorBindingsProcessor>(shader_config);
dbp->bind_scalar_node("freq",     freq_node,     "params",    0, DescriptorRole::UNIFORM);
dbp->bind_vector_node("spectrum", spectrum_node, "spec_data", 0, DescriptorRole::STORAGE);
```

Dispatch modes available on `ComputeProcessor`:

| Mode | Behaviour |
|---|---|
| `ELEMENT_COUNT` | Groups = ceil(element_count / workgroup_x). Default. |
| `BUFFER_SIZE` | Groups = ceil(byte_size / workgroup_x). |
| `MANUAL` | Fixed `group_count_x/y/z` set via `set_manual_dispatch()`. |
| `CUSTOM` | Lambda: `(shared_ptr<VKBuffer>) -> array<uint32_t, 3>`. |

### ComputePress: Raw Vulkan Compute

`Portal::Graphics::ComputePress` is the compute-specific command orchestration layer. It wraps `ShaderFoundry` command buffers and handles pipeline creation (including auto-reflection from SPIR-V), descriptor set allocation, push constant upload, and `vkCmdDispatch` / `vkCmdDispatchIndirect`. This is the level at which you work when you need full control over command recording, synchronisation barriers, or indirect dispatch driven by a GPU-side buffer.

```cpp
#include <MayaFlux/Portal/Graphics/ComputePress.hpp>

auto& press   = Portal::Graphics::get_compute_press();
auto& foundry = Portal::Graphics::get_shader_foundry();

// Create pipeline with explicit descriptor layout:
auto shader_id   = foundry.load_shader("my_kernel.comp");
auto pipeline_id = press.create_pipeline(
    shader_id,
    { { DescriptorBindingInfo { 0, vk::DescriptorType::eStorageBuffer, 1 },
        DescriptorBindingInfo { 1, vk::DescriptorType::eStorageBuffer, 1 } } },
    sizeof(MyPC));

// Or auto-reflect from SPIR-V:
auto pipeline_id = press.create_pipeline_auto(shader_id, sizeof(MyPC));

// Allocate descriptors for this pipeline:
auto ds_ids = press.allocate_pipeline_descriptors(pipeline_id);

// Update descriptors with actual buffers:
foundry.update_descriptor_buffer(ds_ids[0], 0, vk::DescriptorType::eStorageBuffer,
    input_vk_buf->get_buffer(), 0, input_vk_buf->get_size());
foundry.update_descriptor_buffer(ds_ids[0], 1, vk::DescriptorType::eStorageBuffer,
    output_vk_buf->get_buffer(), 0, output_vk_buf->get_size());

// Record and dispatch:
auto cmd_id = foundry.begin_commands(ShaderFoundry::CommandBufferType::COMPUTE);

MyPC pc { .n_elements = count, .scale = 2.0f };
press.bind_all(cmd_id, pipeline_id, ds_ids, &pc, sizeof(pc));
press.dispatch(cmd_id, (count + 255) / 256, 1, 1);

foundry.end_and_submit(cmd_id);
```

`bind_all()` is a convenience that chains `bind_pipeline`, `bind_descriptor_sets`, and `push_constants` in one call. `dispatch_indirect` is also available when dispatch dimensions come from a GPU-side buffer.

### Which Level to Use

| Situation | Level |
|---|---|
| Data in `Datum`, result back to CPU or into a container | Yantra `ShaderExecutionContext` |
| Output stays GPU-resident in a `VKBuffer` (vertex data, field buffers) | `ComputeProcessor` subclass |
| Node outputs need to drive shader uniforms per frame | `DescriptorBindingsProcessor` |
| Full command control, indirect dispatch, or pipeline barriers between passes | `ComputePress` directly |

The levels are not isolated. A `ComputeProcessor` can share `VKBuffer` objects with a Yantra executor that reads them back into a `Datum` for analysis. A `ComputePress` dispatch can write into a buffer that a `GraphicsRoutine` then binds as a vertex input the same frame. The infrastructure is shared; only the abstraction level differs.

---

## What Yantra Does Not Replace

Yantra handles offline and ahead-of-time compute. It does not cover:

- Real-time per-buffer or per-frame GPU dispatch tied to the audio or graphics clock. For that, use `GraphicsRoutine`, `GpuComputeNode`, or direct Vulkan from a scheduled coroutine.
- Rendering pipelines. The graphics side is `Portal::Graphics` with `RenderFlow`, vertex/fragment shaders, and the buffer rendering subsystem.
- Streaming data that updates every buffer cycle. Containers like `AudioOutputContainer` and `VideoStreamContainer` feed live data into the `Datum::container` field, but the dispatch timing is driven by Vruta, not by `ComputeMatrix`.

For those cases, Yantra data types (`Datum`, containers, `DataVariant`) cross the boundary freely; the executor and matrix layers stay on the offline/async side.
