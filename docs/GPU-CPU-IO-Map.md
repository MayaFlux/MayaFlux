# CPU-GPU IO in MayaFlux

A map of every way data crosses the CPU-GPU boundary in MayaFlux, current as of 0.5. It describes what exists today, not a target design. Each section names the exact call, what it requires, and what it does not do. Gaps are named as gaps.

## Pick a layer

| Layer | Readback | Reach for it when |
|---|---|---|
| Yantra (section 2) | Automatic by default | You work through nodes and `Datum<T>`. This is the default choice. |
| `VKBuffer` processors (section 3) | Only when you ask | You drive a shader directly against a `VKBuffer`, every audio buffer or every frame. |
| Portal (section 4) | Not applicable | You write a new processor class, or work outside the buffer chain. |
| `IO` (section 8) | One-shot, blocking | You save, load or record data to or from files. |

## Contents

1. The three tiers of readback
2. Yantra: the Datum-driven GPGPU layer
3. VKBuffer processors: the real-time-friendly layer
4. The raw layer underneath: Portal
5. Binding data by name, and choosing an upload path
6. Reading and writing raw storage
7. Specialized buffer classes
8. Files and images: the export and import layer
9. Decision table
10. Design boundaries and genuine gaps

---

## 1. The three tiers of readback

Every readback path is one of three tiers. Decide which one you are in before touching any API.

| Tier | Meaning | Cost |
|---|---|---|
| **0: never leave the GPU** | Compute writes a buffer, render reads the same buffer. No CPU involvement. | None. The default whenever the CPU does not need the numbers. |
| **1: host-visible, direct read** | The buffer has a coherent `mapped_ptr` (`HOST_STORAGE`, `UNIFORM`, or similar). Read it with a pointer dereference or memcpy. | No staging buffer, no fence, no `StagingUtils` call. |
| **2: device-local, staged transfer** | The buffer is in device-local memory the CPU cannot see. Copy to staging, wait on a fence, read the staging memory. | A real fence round trip. Gate it behind an explicit request, never every frame. |

`ComputeMeshBuffer` rendering is tier 0. A counter read through `mapped_ptr` is tier 1. `RelaxationGridBuffer` is request-gated and, because its state is host-visible, tier 1 in practice.

---

## 2. Yantra: the Datum-driven GPGPU layer

This is the layer node authors touch. `Datum<T>` goes in, `Datum<T>` comes out, and GPU residency is not yours to manage.

### Storage

`GpuResourceManager` owns two kinds of SSBO:

| | Per-unit | Shared |
|---|---|---|
| Keyed by | `(pipeline_unit_key, index)` | `(set, binding_index)` |
| Allocate | `ensure_buffer` | `ensure_shared_buffer` |
| Upload | `upload`, `upload_raw` | `upload_shared_raw` |
| Download | `download` | `download_shared` |
| Bind | `bind_descriptor`, `bind_descriptors_batch` | `bind_shared_descriptor` |

Shared slots let several pipeline units use the same GPU state with no CPU round trip. Downloads are a direct memcpy from `mapped_ptr`, so they assume host-visible memory (tier 1). There is no staging path at this layer.

### GpuExecutionContext

On every synchronous path (`dispatch_core` and the chained variants), `execute` runs `readback_primary` and `readback_aux` before it returns. You get your data without asking.

- **Opt out per auxiliary output** with `ShaderExecutionContext::set_skip_readback(binding, true)`. Use it for results another GPU stage consumes. The primary result is always read back on this path.

- **`ExecutionMode::DEPENDENCY`** runs its stages with no readback.

- **Asynchronous use:** `ShaderExecutionContext::dispatch_async(input)` returns a `FenceID`. Poll it with `ShaderFoundry::is_fence_signaled`, then call `collect_result()` to read back, which returns a `GpuChannelResult` (`primary` floats, `aux` bytes keyed by binding). Release the fence afterward with `ShaderFoundry::release_fence`, or it leaks a `VkFence` and a command buffer slot per dispatch.

- **Typed auxiliary output:** `ShaderExecutionContext::read_output<T>(result, binding)` reads an auxiliary output from the `Datum` that `execute()` returns. It does not apply to the `GpuChannelResult` from `collect_result()`.

This default suits a node graph, where correctness by default matters more than avoiding the odd extra transfer. Section 3 makes the opposite choice on purpose.

### TextureExecutionContext

`OutputMode` picks the exit path per node:

| Mode | Behavior |
|---|---|
| `CONTAINER` | Downloads into a `TextureContainer`. Full round trip. |
| `SCALAR` | Skips the image download and returns SSBO readback only. |
| `IMAGE` | No CPU readback. The result stays on the GPU for the next shader, retrieved with `get_output_image(0)`. Tier 0 for textures. |

---

## 3. VKBuffer processors: the real-time-friendly layer

`ComputeProcessor`, `RenderProcessor` and their relatives attach directly to a `VKBuffer` through a `BufferProcessingChain`. This layer is separate from Yantra: it does not sit under it, above it, or beside it in a pipeline. It is meant to be safe to call every audio buffer or every frame, so **readback is never automatic**. That is the tradeoff it makes for staying real-time-friendly.

### Constants and bindings (CPU to GPU only)

**Push constants** have no readback, since there is nothing to get back.

- Direct: `set_push_constant_data` on `ShaderProcessor`, so on `ComputeProcessor` and `RenderProcessor`.

- From a node: `NodeBindingsProcessor::bind_node(name, node, offset, size)` reads a node's output into a push constant offset every cycle.

**UBO and SSBO descriptors** have two entry points:

- Direct: `ShaderProcessor::bind_buffer(descriptor_name, vk_buffer)` registers a buffer under a name in `m_bound_buffers`. A name with no binding config gets one auto-created at `set=1` with the next free binding index.

- From nodes: `DescriptorBindingsProcessor::bind_scalar_node`, `bind_vector_node`, `bind_matrix_node`, `bind_structured_node`, `bind_audio_buffer` and `bind_network`. Each allocates its own backing `VKBuffer` and calls `bind_buffer` for you, so the name resolves through the same map.

Texture creation, upload and sampler binding belong to `TextureLoom` and are outside this document.

### Getting bytes out

`ComputeProcessor::execute_shader` ends each dispatch with a `buffer_barrier` to `eShaderRead | eTransferRead`, then submits. The buffer is left ready for a transfer read that the processor never issues. You add that step, with one of:

| Option | What it is | Use it for |
|---|---|---|
| **`DataReadProcessor`** | Each `read_*` call arms one request for the next graphics cycle. `resolve_*` finishes it on the consuming thread, waiting for a device-local copy if one is in flight. | The general case. Reach for it first. Section 6 lists what it reads. |
| **`StagingUtils`** | `download_from_gpu` detects host-visible versus device-local. `download_host_visible` and `download_device_local` are the specific forms. `download_from_gpu_async` does not stall the graphics queue. | Direct calls against a `VKBuffer`, off the graphics thread. |
| **`BufferDownloadProcessor`** | Copies a source `VKBuffer` into a CPU-side target buffer every cycle. | A continuous per-cycle copy. Section 6 covers its `back_buffers` form. |

Rendered pixels are a different case. `RenderProcessor` draws to an image, so none of the above applies:

- `DisplayService::readback_swapchain_region` and `get_last_frame` read the presented swapchain image. `get_last_frame` is lock-free: a per-window readback thread publishes it and callers read it through an atomic `shared_ptr`.

- `DisplayService::register_frame_observer` calls you back on that thread once per captured frame, for windows with capture enabled. The observer must not block. `unregister_frame_observer` removes it.

- `IOManager::capture_window` records those frames to a video file (section 8).

Vertex data written by compute and consumed again as geometry goes through `GeometryReadbackNode` (in `Nodes::GpuSync`). It downloads with `download_from_gpu_async` and re-emits the result as a `GeometryWriterNode`. The transfer is synchronous and submits to the queue itself, so its `process_sample` does nothing. Drive `compute_frame()` yourself from a `FRAME_ACCURATE` metro, never from the graphics thread.

### Getting data in

| Option | What it does |
|---|---|
| **`DataWriteProcessor`** | For a plain `VKBuffer`. `set_data` and `set_vertices` are thread-safe, dirty-gated (nothing re-uploads unless you supplied new data) and applied on the graphics thread through a lock-free double buffer. Device-local targets get a persistent staging buffer at attach time. Its `write_*` requests reach other targets (section 6). |
| **`BufferUploadProcessor`** | The mirror of `BufferDownloadProcessor`. Copies a CPU-side source buffer into a target `VKBuffer` every cycle. |
| **`TransferProcessor`** | Moves audio between an `AudioBuffer` and a `VKBuffer`: `connect_audio_to_gpu`, `connect_gpu_to_audio`, and `setup_staging` for device-local targets. `TransferDirection::BIDIRECTIONAL` is declared but marked future. |

---

## 4. The raw layer underneath: Portal

Underneath `ComputeProcessor` and `RenderProcessor` is Portal: `ComputePress`, `RenderFlow`, `ShaderFoundry`, `TextureLoom` and `SamplerForge`. These are thin wrappers over Vulkan, with no buffer chain, no `VKBuffer` abstraction, no processing token and no `Datum<T>`.

Every processor in section 3 is an opinionated way of driving them. `ComputeProcessor::execute_shader` calls `ComputePress::bind_all`, `push_constants` and `dispatch`, then closes the command buffer with `ShaderFoundry::buffer_barrier` and `submit_and_wait`. `RenderProcessor` does the same through `RenderFlow` and `ShaderFoundry`'s command recording (`begin_secondary_commands`, `end_commands`). `TextureLoom` is what both `ShaderProcessor`'s texture handling and Yantra's `TextureExecutionContext` call for image upload and sampler binding.

Almost no CPU-GPU IO decision is made at this level, so Portal is not mapped in detail. You reach it directly only when writing a new processor class or working outside the buffer chain. If you are debugging barriers, descriptor binding or submission behavior in a `ComputeProcessor`, read this layer next, not Yantra.

---

## 5. Binding data by name, and choosing an upload path

### Name-keyed maps

Every binding is reached through one of three maps. Which one yours lives in tells you what you can do with it later.

| Map | Owner | Key | Holds |
|---|---|---|---|
| `m_bound_buffers` | `ShaderProcessor` | descriptor name | A `VKBuffer` bound to a UBO/SSBO slot |
| `m_bindings` | `DescriptorBindingsProcessor` | logical name | A node, `AudioBuffer` or network source, plus its target descriptor name |
| `m_bindings` | `NodeBindingsProcessor` | logical name | A node source plus a push constant byte offset |

`ShaderProcessor::add_binding(descriptor_name, ShaderBinding)` configures set and binding index for a name before any buffer is bound to it. `ShaderBinding` carries `set`, `binding` and `type`.

**The asymmetry to remember:** the maps only hold what was routed through them. A processor that writes descriptors directly with `foundry.update_descriptor_buffer`, bypassing `bind_buffer`, is invisible to name-based lookup. `RelaxationStepProcessor` does this for its ping-pong state (section 7). If a buffer was never named through `bind_buffer` or a `DescriptorBindingsProcessor::bind_*` call, there is no name to find it by.

### Choosing an upload path

The right path depends on where the data comes from and whether it must move every cycle, not on which map is nearest.

| Source of the data | Every cycle? | Use |
|---|---|---|
| A node's output, to a push constant | Yes | `NodeBindingsProcessor::bind_node` |
| A node, `AudioBuffer` or `NodeNetwork`, to a UBO/SSBO | Yes | `DescriptorBindingsProcessor::bind_*` |
| Raw data in hand, to a push constant | Yes | `ShaderProcessor::set_push_constant_data` |
| Raw data in hand, to a named UBO/SSBO | Yes | `ShaderProcessor::bind_buffer` against a `VKBuffer` you populate |
| Data from another thread into a plain `VKBuffer`, only when new | Yes | `DataWriteProcessor::set_data` / `set_vertices` |
| A full CPU-side buffer onto a specific `VKBuffer` | Yes | `BufferUploadProcessor::configure_source` (section 6 for `back_buffers`) |
| Data assembled once or rarely, outside the chain, for Yantra | No | Extract the bytes, then Yantra `upload` / `upload_raw` / `upload_shared_raw` |

The line that matters is real-time chain versus not. Every row but the last lives in a `BufferProcessingChain` and is built to run every buffer or frame. Yantra's upload calls are the opposite world: you already have the bytes and push them in, usually once at setup or from a non-real-time thread. Using `bind_buffer` to seed a Yantra buffer once is the most common wrong turn. Using Yantra's `upload` for something that must update every frame from a live node is the opposite mistake, since nothing re-calls it for you.

Inside the chain, push constant versus UBO/SSBO is a size and frequency question. Push constants are capped small (`set_push_constant_data` static-asserts 128 bytes, the typical hardware limit) and are the cheapest way to move a few scalars every cycle. Anything larger, array-shaped, or needing to persist as addressable GPU memory across dispatches belongs in a named UBO/SSBO.

---

## 6. Reading and writing raw storage

Everything that is not a named descriptor goes through one of three families: the request processors (`DataReadProcessor`, `DataWriteProcessor`), the `StagingUtils` primitives they are built on, and the per-cycle `BufferDownloadProcessor` / `BufferUploadProcessor` pair.

### DataReadProcessor and DataWriteProcessor

Both attach to a `VKBuffer`. Each request is one-shot and applied on the next graphics cycle, in the order it was made.

| Target | Read | Write |
|---|---|---|
| The attached buffer's primary storage | `read_bytes(byte_count)` | `set_data`, `set_vertices` |
| Another `VKBuffer` (descriptor-bound, instance buffer, ...) | `read_buffer(source, byte_count)` | `write_buffer(target, data, dst_offset)` |
| A named field (`VolumeGridBuffer`, `NetworkGeometryBuffer` state) | `read_field(name)` | `write_field(name, data)` |
| A raw `back_buffers` slot | `read_back_buffer(index, byte_count)` | `write_back_buffer(index, data)` |
| Vertices | `read_vertices()` | `set_vertices` |
| Mesh geometry | `read_mesh()` | `write_mesh(mesh, slot)` |
| Pixels, host copy a `TextureBuffer` retains | `read_host_pixels()` | `write_pixels(image)` |
| Pixels, the GPU image | `read_texture_pixels()` | `write_texture_pixels(image)` |

```cpp
auto reader = std::make_shared<Buffers::DataReadProcessor>();
chain->add_processor(reader, buffer);

reader->read_mesh();

auto mesh = reader->resolve_mesh();
```

**Resolving a read.** Finish with `resolve_bytes()`, `resolve_vertices()`, `resolve_mesh()` or `resolve_pixels()`. Call it on the consuming thread, outside the graphics processing thread, because a device-local result waits for its transfer. A resolver returns `nullopt` and leaves the result pending if the pending read was a different kind. `has_result()` and `is_pending()` report state. A newer `read_*` replaces an armed one that has not run yet. `set_skip_while_pending(true)` drops the newer call instead, so a caller that re-arms every tick never displaces a read in flight.

**Easy to get wrong:**

- `read_texture_pixels()` blocks the graphics cycle for the transfer. `read_host_pixels()` does not, but only returns pixels the `TextureBuffer` kept on the host.

- `read_mesh()` on a `MeshBuffer`, a `MeshWriterNode`-driven `GeometryBuffer` or a `MeshNetworkBuffer` copies CPU-authoritative geometry with no transfer. On a `ComputeMeshBuffer`, or a `VolumeGridBuffer` with a surface, it reads the live vertices, sized by the live counter and not the capacity.

- Writes go to the authority of their target: meshes to the owning `MeshData` or node, retained pixels to the `TextureBuffer`, the rest to the GPU storage their owners read from. Never to a copy its owner regenerates.

- A `back_buffers` slot carries no size, so `byte_count` must not exceed what the owner allocated.

- Attach `DataWriteProcessor` before the producers that consume what it writes. Requests come from one supplier thread.

### By descriptor name: `ShaderProcessor::download_bound`

Resolves a name through `get_bound_buffer` and downloads through `download_from_gpu` in one call.

```cpp
std::vector<float> result;
compute_processor->download_bound("output", result);

compute_processor->download_bound("output", data_ptr, byte_count);
```

It returns `false` for an unbound name rather than throwing, since that is a normal query-time condition. It only sees names bound through `bind_buffer`, directly or via a `DescriptorBindingsProcessor::bind_*` call.

### By raw slot: the `StagingUtils` back_buffers family

For a slot never routed through `bind_buffer`, such as one entry of `RelaxationGridBuffer`'s `back_buffers`. A `VKBufferResources::GenerationSlot` is three handles (`buffer`, `memory`, `mapped_ptr`) with no size, so every call takes the byte count from you.

| Direction | Blocking | Without waiting |
|---|---|---|
| Slot to host | `download_back_buffer(slot, data, size, staging)` | `download_back_buffer_async`, finished by `resolve_back_buffer_read` |
| Host to slot | `upload_back_buffer(slot, data, size, staging)` | `upload_back_buffer_async`, finished by `resolve_transfer` |

```cpp
const auto& slot = grid->get_buffer_resources().back_buffers[grid->front_index()];
std::shared_ptr<VKBuffer> staging;
download_back_buffer(slot, data_ptr, byte_count, staging);
```

Both directions use `slot.mapped_ptr` directly when the slot is host-visible, which is true for every current `back_buffers` user (tier 1, no transfer). Otherwise they copy through the staging buffer with a fence. Keep `staging` alive across calls, or a device-local slot reallocates every time. For the async forms the staging buffer is busy until the returned handle is resolved, so resolve before reusing it.

### Whole back_buffers sets, every cycle

`BufferDownloadProcessor::configure_back_buffers(source, targets)` and `BufferUploadProcessor::configure_back_buffers(target, sources)` move an entire `back_buffers` vector, one CPU buffer per entry, every cycle. `remove_back_buffers` undoes it.

```cpp
download_processor->configure_back_buffers(grid, { target_slot_0, target_slot_1 });
chain->add_processor(download_processor, grid);

upload_processor->configure_back_buffers(grid, { source_slot_0, source_slot_1 });
chain->add_processor(upload_processor, grid);
```

- The target or source count must equal `back_buffers.size()` at process time. It is checked every cycle, and a mismatch skips the call with an error rather than partially applying it.

- Entries move strictly in `back_buffers` order. Which one is "front" or "current" is the caller's concern (`front_index()` or equivalent). These processors only move bytes.

- Download writes each entry into a raw byte buffer with `download_back_buffer`, then `VKBuffer::set_data`. Upload memcpys into `entry.mapped_ptr`, so it supports host-visible entries only (section 10).

- `configure_target` and `configure_source` are separate. `back_buffers` registration lives in a parallel map checked first in `processing_function`, falling through to the original path when a buffer is not registered there.

---

## 7. Specialized buffer classes

Each of these is built the way it is for a specific reason. Choosing a tier for a new buffer class comes down to three questions: can it skip CPU visibility entirely, is a host-visible pointer read enough, or does it truly need a gated device-to-host transfer?

### ComputeMeshBuffer: tier 0

Compute writes vertices straight into the `VKBuffer` that `RenderProcessor` reads. Live rendering never reads vertex data back.

The one scalar that does cross back every dirty frame, a vertex count from an atomic counter, is tier 1. `SDFMeshProcessor::on_after_execute` dereferences the counter buffer's `get_mapped_ptr()` directly, because that buffer is host-visible and coherent. No `download_from_gpu`, no staging. For one scalar from a host-visible buffer, read the pointer.

Reading the geometry is an explicit one-shot: `DataReadProcessor::read_mesh()`, or `download_compute_mesh(buffer)` (in `StagingUtils` and in `IO`'s `ModelExport`). It downloads exactly `get_live_vertex_count()` vertices as non-indexed triangles, not the worst-case allocation. It blocks, so call it off the graphics thread. `IO::save_mesh` and `save_mesh_snapshot` have `ComputeMeshBuffer` overloads that wrap it.

### RelaxationGridBuffer: request-gated

The canonical shape for "give me a copy on demand without paying every frame".

- State lives as raw handle pairs in `back_buffers`, ping-ponged by index. It is never wrapped as a normal `VKBuffer`, so it is invisible to `bind_buffer`, `get_bound_buffer` and the primary-handle path of `BufferDownloadProcessor`.

- `request_snapshot()` sets an atomic flag. It is wait-free and callable from any thread.

- `RelaxationStepProcessor::serve_snapshot()` serves it after a dispatch completes: immediately under synchronous submission, from `on_dispatch_complete` under deferred submission. Only on a pending request does it call `download_back_buffer` on the newly-front slot.

- Delivery is a `BroadcastSource<std::vector<uint8_t>>` from `snapshot_source()`, signaled once per fulfilled request and consumed with `co_await` or `Kriya::on_signal()`. The bytes are rule-defined (a float, a `uint32` automaton state, a `vec2` pair, anything a rule shader defines), so only the caller can interpret them. `get_state_bytes()` gives the total size.

- `seed_state(data, size)` writes initial state into the current front generation with a host-visible memcpy. It is valid before the chain has run, or between generations for a runtime reset, and does not touch the back generation or swap.

The slots are `HostVisible | HostCoherent`, so the transfer is tier 1: a direct `mapped_ptr` memcpy with no fence. The processor still passes a `VKBuffer` (`m_snapshot_staging`) as a fallback that is never touched today, so a device-local `back_buffers` would degrade to the fenced path automatically. Cycles where nobody asked cost nothing, and delivery is event-driven, not polled.

### VolumeGridBuffer: named fields, one-shot

Fields are addressed by name, not descriptor. `read_field(name, data, size)` and `read_fields(...)` read the current read slot. `seed_raw(name, data, size)` writes a field. These are blocking transfers that need command queue access, so they suit setup, export and occasional reads. `IOManager::capture_volume` does a full-field read per captured frame on the graphics thread on purpose, and its cost there is the transfer alone because encoding goes to a worker. The `IO` volume functions in section 8 are built on these.

### NetworkGeometryBuffer and claim readback

State fields live in `back_buffers`. `read_state_slot(name)` returns the whole `GenerationSlot`, not just the Vulkan handle, because a CPU readback through `download_back_buffer` needs `mapped_ptr` for the host-visible fast path.

`ClaimAccumulateProcessor` is the example of conditional readback for GPU simulation state that a CPU simulation also needs:

- Each cycle it reads one counter word, `mutation_claim_events`.

- Only if that is nonzero does it download the full `claimed_by` array and hand it to the `PhysicsOperator` bond table. A zero count clears bonds instead.

- It uploads the physics operator's accreted mass into `mutation_accreted_mass` every cycle, unconditionally. That is one float per particle, already computed on the CPU, with no "nothing to report" case to gate on.

- A null `physics_op` disables the readback while GPU claim and swallow still run. When `live_count` is set, the CPU-facing side is truncated to exactly that many entries.

---

## 8. Files and images: the export and import layer

The `IO` namespace moves data between files and GPU storage. Almost all of it is one-shot and blocking, so it is for save, load and recording, not per-frame data flow. Most operations come in two forms:

- **Free functions** in `IO` are synchronous. The download and the write both finish before they return. Use them for shutdown flushes and tests.

- **`IOManager` methods** download on the calling thread (which needs command queue access) and queue the encode and disk write on a worker. They return once the work is queued, and write failures are logged, not returned.

| Data | Download or upload | Save or capture |
|---|---|---|
| Image (`VKImage`, `TextureBuffer`, `TextBuffer`) | `IO::download_image`, `IO::download_texture_buffer` | `IO::save_image`, `save_texture_buffer`, `save_text_buffer`; `IOManager::save_image` |
| Volume (`VolumeGridBuffer`) | `IO::download_volume(volume, field_names)`, `IO::upload_volume`; `IOManager::load_volume` reads a `.vdb` into a new, seeded buffer | `IO::save_volume`; `IOManager::save_volume`; `IO::VolumeCapture` and `IOManager::capture_volume` write one numbered file per frame |
| Mesh (`MeshBuffer`, `MeshNetworkBuffer`, mesh nodes) | None, the data is CPU-authoritative | `IO::save_mesh`, `save_mesh_snapshot` (timestamped path); `IOManager::save_mesh` |
| Mesh (`ComputeMeshBuffer`) | `download_compute_mesh`, a blocking one-shot | `IO::save_mesh` and `save_mesh_snapshot` overloads |
| Spatial streams (`NetworkGeometryBuffer`, operators, `RelaxationGridBuffer`) | `make_network_geometry_source`, `write_operator_sample`, `write_network_geometry_buffer_sample`; grid state via `request_snapshot` | `IO::SpatialCapture`, `IOManager::capture_spatial`, `IO::RelaxationGridCapture`, `IO::save_spatial_snapshot` |
| Window frames | `DisplayService` readback (section 3) | `IOManager::capture_window`, `VideoFileWriter::record` |

Notes on specific paths:

- **Volumes.** A scalar field (stride `sizeof(float)`) lands as a scalar. A `vec4`-stride field is gathered into `glm::vec3` on download and expanded back on upload, each costing a second allocation at four thirds the size. Any other stride is skipped with an error, as is a field that was never declared. `IOManager::load_volume` leaves `setup_processors()` and simulation wiring to the caller.

- **Spatial.** One `SpatialCapture` ticks several named sources into one Alembic archive on one clock, and `IOManager::capture_spatial` joins a capture already open on the same path. `RelaxationGridCapture` is driven from the grid's `snapshot_source()` listener and calls `write_snapshot()` inside it, so its cost is the snapshot's. `SpatialReader` reads an archive into CPU `SpatialSample`s and never touches the GPU.

- **Images.** Only layer 0, mip 0 downloads. `IO::download_image` restores `eShaderReadOnlyOptimal` afterward unless you pass another restore layout.

- **Dialogs.** `API/Depot.hpp` wraps several of these (`save_image`, `save_mesh`, `save_volume`) behind a file dialog that blocks until the user confirms or cancels.

---

## 9. Decision table

| I need to... | Use |
|---|---|
| Move a node graph value to the GPU and back | Yantra `ensure_buffer` / `upload` / `download` |
| Share GPU state across pipeline units without a CPU hop | Yantra `ensure_shared_buffer` / `upload_shared_raw` / `download_shared` |
| Dispatch a Yantra shader without blocking, then collect | `ShaderExecutionContext::dispatch_async`, poll `is_fence_signaled`, `collect_result`, then `release_fence` |
| Keep a Yantra auxiliary output on the GPU | `ShaderExecutionContext::set_skip_readback(binding, true)` |
| Keep a texture result GPU-resident for the next shader | `TextureExecutionContext::OutputMode::IMAGE` |
| Auto-download a texture result | `TextureExecutionContext::OutputMode::CONTAINER` |
| Send a node's scalar to a shader every cycle | `NodeBindingsProcessor::bind_node` (push constant) or `DescriptorBindingsProcessor::bind_scalar_node` (UBO/SSBO) |
| Supply data from another thread into a plain `VKBuffer` | `DataWriteProcessor::set_data` / `set_vertices` |
| Upload a CPU buffer onto a specific `VKBuffer` every cycle | `BufferUploadProcessor::configure_source` |
| Upload once or rarely, outside the chain, into Yantra | Extract the bytes, then Yantra `upload` / `upload_raw` / `upload_shared_raw` |
| Bind an SSBO/UBO under a name I can look up | `ShaderProcessor::bind_buffer`, then `get_bound_buffer` |
| Download a named descriptor's contents | `ShaderProcessor::download_bound` |
| Keep compute output on the GPU for render | The same `VKBuffer` in both processors' chain slots |
| Read one scalar from a host-visible buffer | Dereference `mapped_ptr` directly |
| Read a buffer, field, vertices, mesh or pixels once | `DataReadProcessor::read_*`, then `resolve_*` |
| Write into a buffer, field, `back_buffers` slot, mesh or pixels | `DataWriteProcessor::write_*` |
| Copy a source buffer to a CPU target every cycle | `BufferDownloadProcessor::configure_target` |
| Get occasional full-state snapshots without a per-frame cost | A request flag served after the dispatch and delivered by `BroadcastSource`, as in `RelaxationGridBuffer::request_snapshot` |
| Move a raw `back_buffers` slot once | `StagingUtils::download_back_buffer` / `upload_back_buffer`, or the `_async` forms with `resolve_*` |
| Move a whole `back_buffers` set every cycle | `BufferDownloadProcessor` / `BufferUploadProcessor::configure_back_buffers` |
| Read presented pixels from a window | `DisplayService::readback_swapchain_region` / `get_last_frame` |
| React to every captured window frame | `DisplayService::register_frame_observer` |
| Pull GPU-written vertices back into the node graph | `GeometryReadbackNode`, driven manually |
| Move audio between an `AudioBuffer` and a `VKBuffer` | `TransferProcessor` |
| Read the live geometry of a `ComputeMeshBuffer` | `DataReadProcessor::read_mesh` or `download_compute_mesh` |
| Save an image, volume or mesh to disk | `IO::save_*` (synchronous) or `IOManager::save_*` (queued encode) |
| Record a volume, spatial stream or window over time | `IOManager::capture_volume` / `capture_spatial` / `capture_window` |

---

## 10. Design boundaries and genuine gaps

A boundary is a place two concerns were deliberately kept apart, so asking why they do not merge is the wrong question. A gap is something genuinely unfinished, worth closing when a real caller needs it.

### Design boundaries, not gaps

- **Descriptor names and raw handles are separate worlds, on purpose.** `ShaderProcessor`'s name maps (section 5) and the `back_buffers` primitives (`download_back_buffer`, `upload_back_buffer`, `configure_back_buffers`) serve different concerns. One lets a shader author look something up by the name they gave it. The other moves bytes for state that was never named because it was never meant to go through descriptor binding. `BufferDownloadProcessor` and `BufferUploadProcessor` therefore never touch `ShaderBinding::set` / `binding` or `m_config.bindings`, on either path. That belongs to `ShaderProcessor` and its subclasses, which keeps the pair each other's mirror image.

- **`ComputeProcessor` and `RenderProcessor` do not read back their own output.** A tier 2 transfer costs a fence round trip, and paying it every cycle whether or not anyone asked is the wrong default for a layer called every buffer or frame. `RelaxationGridBuffer` is the model: a request flag, so the cost is paid only when a snapshot is wanted. `download_bound`, `DataReadProcessor` and `configure_back_buffers` make the manual step cheaper to write, not automatic. Yantra's `GpuExecutionContext` chooses the opposite, readback by default, because a node graph values correctness by default.

- **`configure_target` / `configure_source` take only a `VKBuffer`, and `back_buffers` has its own parallel entry point.** One target versus a vector that must match `back_buffers.size()` at process time is different enough that one overloaded signature would make the common case harder to read for no benefit.

- **`MeshData` has no GPU download step.** For `MeshBuffer` and its relatives every mutation writes the CPU copy first and the GPU is upload-only, so mesh export and `read_mesh()` copy CPU data. Only `ComputeMeshBuffer` and a `VolumeGridBuffer` surface need a real readback.

### Genuine gaps

- **`BufferUploadProcessor::configure_back_buffers` supports host-visible entries only.** It memcpys into `mapped_ptr` and logs an error for an entry without one. The fenced primitive it needs now exists (`upload_back_buffer` and `upload_back_buffer_async`), and `DataWriteProcessor::write_back_buffer` already uses it, so the one-shot path handles device-local slots and the per-cycle processor does not. Worth closing when a device-local `back_buffers` user appears. Nothing exercises it today.

- **`back_buffers` transfers cannot validate size.** `GenerationSlot` has no size field, so `upload_back_buffer`, `read_back_buffer` and the per-cycle upload trust the byte count they are given, and a misconfigured source silently short-writes instead of erroring. Callers get the right size out of band (`RelaxationGridBuffer::get_state_bytes()`). Fixing it properly means adding a size to `GenerationSlot`, a struct change, to be weighed against how many call sites assume its current shape.

- **The blocking one-shots have no asynchronous form.** `read_texture_pixels`, `download_compute_mesh`, `VolumeGridBuffer::read_field` and `GeometryReadbackNode::compute_frame` block the calling thread. `DataReadProcessor`'s other reads and the `StagingUtils` `_async` pairs avoid stalling the graphics queue, but there is no non-blocking texture, mesh or volume read.
