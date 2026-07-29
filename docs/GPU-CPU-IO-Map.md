## CPU-GPU IO in MayaFlux

Reference map of every way data crosses the CPU-GPU boundary in MayaFlux, current as of the 0.4 development line. This document describes what exists today, not a target design.

## How to use this document

Find your situation in the table of contents. Each section names the exact call, states what it requires, and states what it does not do. Where a gap exists, it is named as a gap, not silently worked around.

## Contents

1. The three tiers of readback
2. Yantra: the Datum-driven GPGPU layer
3. VKBuffer processors: the real-time-friendly layer
4. The raw layer underneath: Portal
5. Binding data by name: descriptors, push constants, nodes, and choosing an upload path
6. Getting data out by descriptor name, and moving whole back_buffers sets
7. Specialized buffer classes and why they differ
8. Decision table
9. Design boundaries and genuine gaps

---

## 1. The three tiers of readback

Before touching any API, understand which tier your situation falls into. Every readback path in MayaFlux is one of these three:

**Tier 0: never leave the GPU.** Compute writes a buffer, render reads the same buffer, no CPU involvement at any point. Fastest, and the default you should reach for whenever the CPU does not actually need the numbers.

**Tier 1: host-visible, direct read.** The buffer is allocated with `HOST_STORAGE`, `UNIFORM`, or another host-visible usage, meaning it has a coherent `mapped_ptr`. Read it with a plain pointer dereference or memcpy. No staging buffer, no fence, no `StagingUtils` call needed.

**Tier 2: device-local, staged transfer.** The buffer lives in fast device-local memory the CPU cannot see directly. Getting bytes out requires a copy to a staging buffer, a fence wait, then a read from the staging buffer's mapped memory. This is the only tier that does real transfer work, and it is the one you should gate behind an explicit request rather than running every frame.

Every class described below picks one of these three deliberately. `ComputeMeshBuffer` picks tier 0. A push-constant scalar might use tier 1. `RelaxationGridBuffer` uses tier 2, gated behind an atomic request flag.

---

## 2. Yantra: the Datum-driven GPGPU layer

This is the layer node authors touch. `Datum<T>` goes in, `Datum<T>` comes out, GPU residency is an implementation detail you do not manage by hand.

### Regular per-unit SSBO

Owned by `GpuResourceManager`, keyed by `(pipeline_unit_key, index)`.

- `ensure_buffer` allocates or resizes.
- `upload` / `upload_raw` write CPU data to the GPU buffer.
- `download` reads back via direct memcpy from `mapped_ptr`. This assumes host-visible memory (tier 1). There is no staging path at this layer.
- `bind_descriptor` wires the buffer into a descriptor slot.

### Shared SSBO

Same shape, but keyed by `(set, binding_index)` in `m_shared->slots` rather than per-unit. Multiple pipeline units binding the same slot share GPU state without a CPU round trip.

- `ensure_shared_buffer`
- `upload_shared_raw`
- `download_shared`
- `bind_shared_descriptor`

### GpuExecutionContext

Wraps the above with `dispatch_async` / `collect_result`. **Readback here is unconditional**: every `dispatch_core*` call runs `readback_primary` and `readback_aux` before returning. You do not opt in to getting your data back, you would have to opt out, and there is no opt-out. This is the correct default for the node-graph world; the `VKBuffer` processor layer (section 3) makes the opposite tradeoff on purpose, for reasons unrelated to Yantra.

### TextureExecutionContext

For image data, `OutputMode` picks the exit path per node:

- `CONTAINER`: auto-downloads into a `TextureContainer`. Full round trip, no manual step.
- `SCALAR`: skips image download, returns SSBO readback only.
- `IMAGE`: skips CPU readback entirely, result stays GPU-resident for the next shader in the chain. This is tier 0 for textures.

**When to use this layer:** any time you are working through nodes, `Datum<T>`, or the standard node-graph pipeline. This is the default and should remain your first choice.

---

## 3. VKBuffer processors: the real-time-friendly layer

`ComputeProcessor`, `RenderProcessor`, and their relatives, attached directly to a `VKBuffer` via a `BufferProcessingChain`. This layer has no relationship to Yantra; it does not sit under it, above it, or beside it in any pipeline sense. Yantra is the `Datum<T>`-driven node-graph layer (section 2). This layer is the direct, chain-attached way to drive a shader against a `VKBuffer`, meant to be safe to call every audio buffer or every frame without surprising cost. Readback is never automatic here, you have to ask for it explicitly, which is the deliberate tradeoff this layer makes for staying real-time-friendly rather than always-correct-by-default the way Yantra is.

### Push constants

One-directional, CPU to GPU only. No readback concept applies; there is nothing to get back.

- Direct: `set_push_constant_data` on `ComputeProcessor` / `RenderProcessor`.
- Via node binding: `NodeBindingsProcessor::bind_node(name, node, offset, size)`, which reads a node's output into a push constant offset every cycle.

### UBO / SSBO descriptor bindings

Two entry points:

- Direct: `ShaderProcessor::bind_buffer(descriptor_name, vk_buffer)`. Registers a `VKBuffer` under a named descriptor slot, tracked in `m_bound_buffers`. If the name has no existing binding config, one is auto-created at `set=1` with the next free binding index.
- Via node interop: `DescriptorBindingsProcessor::bind_scalar_node` / `bind_vector_node` / `bind_matrix_node` / `bind_structured_node` / `bind_audio_buffer` / `bind_network`. Each of these allocates its own backing `VKBuffer` internally and calls `bind_buffer` on your behalf, so the descriptor name still resolves through the same `m_bound_buffers` map.

### Textures

`TextureLoom` owns creation, upload, and sampler binding. Not covered further here; this document is about buffer IO.

### Getting bytes out: where the manual step lives

`ComputeProcessor::execute_shader` ends every dispatch with a `buffer_barrier` transitioning the buffer to `eShaderRead | eTransferRead`, then submits. That barrier is the tell: the buffer is left ready for a transfer read, but the processor never issues one. You must add that step yourself, by one of:

- **`StagingUtils::download_from_gpu`** (or the specific `download_host_visible` / `download_device_local` variants), called directly against the `VKBuffer`. `download_from_gpu` auto-detects which variant applies. `download_from_gpu_async` records a fenced copy and waits on the fence rather than `queue.waitIdle`, safe to call off the graphics thread.
- **`BufferDownloadProcessor`**, a `VKBufferProcessor` attached to the chain alongside your compute or render processor. Its original `configure_target(source, target)` still only accepts a `VKBuffer` shared_ptr as source. It now also has `configure_back_buffers(source, targets)`, covering the case where the source is a `VKBuffer` whose actual live state sits in `back_buffers` rather than its primary handle. See section 6 for exact mechanics on both.

For rendered pixels rather than a buffer, `RenderProcessor` draws to an image, not a buffer, so neither of the above applies:

- `DisplayService::readback_swapchain_region` / `get_last_frame` reads the presented swapchain image. Lock-free, published by a per-window readback thread, read via atomic `shared_ptr`.

For vertex data written by compute and consumed again as geometry:

- `GeometryReadbackNode` wraps `download_from_gpu_async` and re-emits the result as a `GeometryWriterNode`. Flagged as needing manual `compute_frame()` invocation off the graphics thread, since it performs its own queue submission.

**When to use this layer:** when you need direct control over a `ComputeProcessor` or `RenderProcessor` outside the node graph, or when working with specialized buffer classes that manage their own GPU state.

---

## 4. The raw layer underneath: Portal

`ComputeProcessor` and `RenderProcessor` (section 3) are not themselves the bottom. Underneath them is Portal: `ComputePress`, `RenderFlow`, `ShaderFoundry`, `TextureLoom`, and related pieces (`SamplerForge` for samplers). This is genuinely the raw layer, thin wrappers over Vulkan calls, no buffer-chain concept, no `VKBuffer` abstraction, no processing token, no `Datum<T>`.

`ComputeProcessor::execute_shader` is a concrete example of the relationship: it calls `ComputePress::bind_all`, `push_constants`, and `dispatch` directly, then `ShaderFoundry::buffer_barrier` and `submit_and_wait` to close out the command buffer. Every `VKBuffer` processor in section 3 is built as a specific, opinionated way of driving these calls, not an alternative to them. `RenderProcessor` does the equivalent through `RenderFlow` and `ShaderFoundry`'s command recording (`begin_secondary_commands`, `end_commands`). `TextureLoom` is what `ShaderProcessor`'s texture handling and Yantra's `TextureExecutionContext` both ultimately call into for actual image upload and sampler binding.

This document does not map Portal's API in the same detail as the layers above it, since almost no CPU-GPU IO decision is made at this level in ordinary use, you reach it directly only when writing a new processor class or working outside the buffer-chain model entirely. Knowing it exists, and that it is what section 3's classes are made of, is the useful takeaway here: if you are ever debugging why a `ComputeProcessor` behaves a certain way around barriers, descriptor binding, or submission, this is the layer to read next, not Yantra.

---

## 5. Binding data by name: descriptors, push constants, nodes

Every binding in MayaFlux is reached through one of three name-keyed maps. Knowing which map your binding lives in tells you what you can and cannot do with it later.

| Map | Owner | Key | What it holds |
|---|---|---|---|
| `m_bound_buffers` | `ShaderProcessor` | descriptor name (string) | `VKBuffer` shared_ptr bound to a UBO/SSBO slot |
| `m_bindings` | `DescriptorBindingsProcessor` | logical name (string) | Node/AudioBuffer/network source plus its target descriptor name |
| `m_bindings` | `NodeBindingsProcessor` | logical name (string) | Node source plus a push constant byte offset |

`add_binding(descriptor_name, ShaderBinding)` on `ShaderProcessor` is where set and binding index are configured for a descriptor name, ahead of any buffer being bound to it. `ShaderBinding` carries `set`, `binding`, and `type`.

**The important asymmetry:** these maps only contain what was routed through them. Any processor that writes descriptors directly via `foundry.update_descriptor_buffer`, bypassing `bind_buffer`, is invisible to name-based lookup. `RelaxationStepProcessor` does exactly this for its ping-pong state (section 7). If a buffer was never named through `bind_buffer` or one of the `DescriptorBindingsProcessor::bind_*` calls, there is no name to look it up by, regardless of what API you reach for.

### Choosing an upload path

The three maps above are three of five real answers to "how do I get this data onto the GPU." Which one is correct depends on where the data is coming from and whether it needs to move every cycle in real time, not on which map happens to be closest at hand.

| Source of the data | Needs every-cycle real-time update? | Use |
|---|---|---|
| A node's output | Yes | `NodeBindingsProcessor::bind_node` (push constant) |
| A node, `AudioBuffer`, or `NodeNetwork`, headed for a UBO/SSBO rather than a push constant | Yes | `DescriptorBindingsProcessor::bind_scalar_node` / `bind_vector_node` / `bind_matrix_node` / `bind_structured_node` / `bind_audio_buffer` / `bind_network` |
| Raw data you already have in hand, going to a push constant | Yes | `ShaderProcessor::set_push_constant_data` directly |
| Raw data you already have in hand, going to a named UBO/SSBO descriptor | Yes | `ShaderProcessor::bind_buffer` against a `VKBuffer` you populate yourself |
| A full CPU-side buffer, not addressed by descriptor name or `back_buffers` slot, needs to land on a specific `VKBuffer` every cycle | Yes | `BufferUploadProcessor::configure_source` (section 6 covers its `back_buffers` counterpart, `configure_back_buffers`) |
| Data assembled once, or infrequently, outside the real-time chain entirely, headed into the Yantra/`Datum<T>` world | No | Extract the data yourself, then Yantra's `upload` / `upload_raw` (per-unit) or `upload_shared_raw` (shared slot) |

The line that actually matters is real-time-chain versus not. Everything in the first five rows lives inside a `BufferProcessingChain`, gets called every audio buffer or every frame, and is built to tolerate that. The last row is a different world entirely: Yantra's upload calls assume you already have the bytes and are pushing them in from outside the buffer-processing model, typically once at setup, or occasionally from a non-real-time thread, not as a per-cycle binding. Reaching for `bind_buffer`/`NodeBindingsProcessor` when you actually just need to seed a Yantra-managed buffer once is the most common wrong turn; reaching for Yantra's `upload` when you actually need something updating every frame from a live node is the opposite mistake, since nothing re-calls it for you the way a chain processor's `processing_function` would.

Within the real-time chain, push constant versus UBO/SSBO is a size and update-frequency question, not a preference: push constants are capped small (128 bytes is the typical hardware limit, see `set_push_constant_data`'s own `static_assert`/`MF_ASSERT`) and are the cheapest path for a handful of scalars changing every cycle; anything larger, anything array-shaped, or anything that needs to persist as addressable GPU memory across dispatches belongs in a named UBO/SSBO instead.

---

## 6. Getting data out by descriptor name, and moving whole back_buffers sets

You asked specifically whether you can specify *which* SSBO/UBO to download by its string key rather than by object identity, and separately whether raw auxiliary handles like `back_buffers` can be reached at all. Both are now covered, by two different primitives for two different binding worlds. Neither bridges into the other, because a string name only exists for the first world.

### Download by descriptor name: `ShaderProcessor::download_bound`

Resolves a descriptor name through `get_bound_buffer` and downloads through `download_from_gpu` in one call:

```cpp
std::vector<float> result;
compute_processor->download_bound("output", result);
```

or the raw-pointer form:

```cpp
compute_processor->download_bound("output", data_ptr, byte_count);
```

Returns `false` if the name has no bound buffer, rather than throwing, since an unbound name is a normal query-time condition. This only works for names bound through `bind_buffer` (directly, or via a `DescriptorBindingsProcessor::bind_*` call), since that is the only map `get_bound_buffer` can see.

### Download by raw handle, outside the named-binding system: `StagingUtils::download_back_buffer`

For a single raw slot that was never routed through `bind_buffer` at all, such as one entry of `RelaxationGridBuffer`'s `back_buffers`:

```cpp
const auto& slot = grid->get_buffer_resources().back_buffers[grid->front_index()];
std::shared_ptr<VKBuffer> staging; // reused across calls if the slot is ever device-local
download_back_buffer(slot, data_ptr, byte_count, staging);
```

Reads `slot.mapped_ptr` directly when the slot is host-visible, which is the case for every current user of `back_buffers` (tier 1, no transfer at all). Falls back to a fenced copy through the supplied staging buffer otherwise, the same shape as `download_from_gpu_async`. Keep `staging` alive across calls; a fresh local reallocates every call in the device-local fallback case.

### Whole back_buffers sets, every processing cycle: `configure_back_buffers` on both processors

Sections 2 and 3 already establish that `BufferDownloadProcessor`/`BufferUploadProcessor` are the chain-attached, every-cycle way to move data, in contrast to the one-shot calls above. Both processors originally only understood a `VKBuffer`'s primary handle, because `back_buffers` did not exist when they were written. They now also support downloading or uploading an entire `back_buffers` vector, one CPU buffer per entry, every cycle:

```cpp
download_processor->configure_back_buffers(grid, { target_slot_0, target_slot_1 });
chain->add_processor(download_processor, grid);
```

```cpp
upload_processor->configure_back_buffers(grid, { source_slot_0, source_slot_1 });
chain->add_processor(upload_processor, grid);
```

`targets`/`sources` must have exactly as many entries as `back_buffers` at process time, checked fresh every cycle, or the call is skipped with an error rather than partially applied. Entries are moved strictly in `back_buffers` order. Which entry is "front", "current", or otherwise meaningful is left entirely to the calling code, these processors only move bytes; interpret the returned targets the same way you would interpret a manual `download_back_buffer` result, by asking the source buffer directly (`front_index()` or equivalent).

The download path writes each entry into a correctly-sized raw byte buffer via `download_back_buffer`, then pushes it through `VKBuffer::set_data`, rather than attempting to obtain a writable view out of the target's existing data, which is not something the existing `DataAccess`/`get_data()` pattern can provide since that pattern is built for reading an existing variant, not producing a landing pad to write into. The upload path memcpys directly into `entry.mapped_ptr`; an entry without a mapped pointer is unsupported today and logged as an error rather than silently skipped, since no fenced-upload-to-raw-handle primitive exists yet to fall back to (the mirror of `download_back_buffer`'s fenced fallback branch does not yet exist on the upload side).

`configure_target`/`configure_source` and their original maps are completely untouched by this. `back_buffers` registration lives in a separate map on each processor, checked first in `processing_function`, falling through to the original path when a buffer isn't registered there.

Section 9 covers what the name-based and `back_buffers` primitives above still cannot do, and which of those limits are deliberate versus genuinely unfinished.

---

## 7. Specialized buffer classes and why they differ

These three classes show the three tiers in practice, and each was built the way it was for a specific reason, not by accident.

### ComputeMeshBuffer: tier 0, by design

Compute writes vertices directly into the `VKBuffer` that `RenderProcessor` reads from. No CPU readback of vertex data, ever. Doxygen states this outright.

The one scalar that does cross back, a vertex count from an atomic counter, uses tier 1, not tier 2: `SDFMeshProcessor::on_after_execute` dereferences `m_counter_buf->get_mapped_ptr()` directly, because that counter buffer is `HOST_STORAGE` and coherent. No `download_from_gpu`, no staging buffer. When you only need one scalar from a host-visible buffer, skip the download abstraction entirely and read the pointer.

### RelaxationGridBuffer: tier 2, request-gated

The canonical shape for "give me a copy on demand, without paying for it every frame":

- State lives as raw handle pairs in `VKBufferResources::back_buffers`, ping-ponged by index. Deliberately never wrapped as a normal `VKBuffer`, so it is invisible to `bind_buffer`/`get_bound_buffer` and to `BufferDownloadProcessor`.
- `request_snapshot()` sets an atomic flag. Callable from any thread, wait-free.
- `RelaxationStepProcessor::on_after_execute` checks `consume_snapshot_request()` every cycle. Only on a hit does it call `StagingUtils::download_back_buffer` on the newly-front slot.
- Delivery is a `BroadcastSource<std::vector<uint8_t>>`, signaled once per fulfilled request, consumed via `co_await` or `Kriya::on_signal()`.

The `back_buffers` slots here are `HostVisible | HostCoherent`, so this is actually tier 1, not tier 2: `download_back_buffer` resolves to a direct `mapped_ptr` memcpy, no fence, no staging transfer. A processor-owned `VKBuffer` (`m_snapshot_staging`) is still kept and passed through on every call as a dormant fallback, lazily sized and ready only if `back_buffers` is ever backed by device-local memory in the future; today it is never touched. Either way, delivery costs nothing on cycles where nobody asked, and is event-driven rather than polled.

### Why the three differ

Each class earned its tier by what it actually needs, not by a missing feature elsewhere:

- `ComputeMeshBuffer` needs zero CPU visibility into vertex data, so it stays tier 0.
- The vertex counter needs one scalar, cheaply, from memory that is already coherent, so it uses tier 1 directly rather than routing through a general download processor.
- `RelaxationGridBuffer` needs occasional, full-state snapshots, gated behind an explicit request rather than run every cycle. In this case the underlying memory turned out to be host-visible, so the actual transfer is tier 1, but the request-gated, event-delivered shape is the right one regardless of tier, and is now backed by `download_back_buffer` (section 6) rather than an inline fenced copy.

If you are designing a new buffer class, pick your tier by the same reasoning: can it skip CPU visibility entirely, is a host-visible pointer read sufficient, or does it genuinely need gated device-to-host transfer.

---

## 8. Decision table

| I need to... | Use |
|---|---|
| Move a node graph value to the GPU and back | Yantra: `ensure_buffer` / `upload` / `download` |
| Share GPU state across multiple pipeline units without a CPU hop | Yantra shared SSBO: `ensure_shared_buffer` / `upload_shared_raw` / `download_shared` |
| Keep a texture result GPU-resident for the next shader | `TextureExecutionContext::OutputMode::IMAGE` |
| Auto-download a texture result | `TextureExecutionContext::OutputMode::CONTAINER` |
| Send a scalar to a shader every cycle from a node | `NodeBindingsProcessor::bind_node` (push constant) or `DescriptorBindingsProcessor::bind_scalar_node` (UBO/SSBO) |
| Upload a full CPU-side buffer to a specific `VKBuffer` every cycle, not addressed by descriptor name or `back_buffers` slot | `BufferUploadProcessor::configure_source` |
| Upload data once, or occasionally, outside the real-time chain entirely, into the Yantra world | Extract the bytes yourself, then Yantra's `upload` / `upload_raw` or `upload_shared_raw` |
| Bind an SSBO/UBO under a name I can look up later | `ShaderProcessor::bind_buffer(name, buffer)`, then `get_bound_buffer(name)` |
| Download a named descriptor's contents | `ShaderProcessor::download_bound(name, ...)`, one call |
| Keep compute output entirely on GPU for render to consume | Same `VKBuffer` in both processors' chain slots, no download at all |
| Read one scalar from a known host-visible buffer | Dereference `mapped_ptr` directly, skip staging entirely |
| Get occasional full-state snapshots from device-local memory without a per-frame cost | Atomic request flag, consumed in `on_after_execute`, delivered via `BroadcastSource`, as in `RelaxationStepProcessor` |
| Read presented pixels from a window | `DisplayService::readback_swapchain_region` / `get_last_frame` |
| Pull GPU-written vertex data back into the node graph | `GeometryReadbackNode` |
| Download a raw handle that lives in `VKBufferResources::back_buffers`, outside any named binding, once | `StagingUtils::download_back_buffer(slot, data, size, staging)` |
| Download or upload an entire `back_buffers` set, every processing cycle, as part of a chain | `BufferDownloadProcessor::configure_back_buffers` / `BufferUploadProcessor::configure_back_buffers` |

---

## 9. Design boundaries and genuine gaps

Two different things get called "gaps" in casual conversation about this system. This section keeps them apart: a boundary is a place two concerns were deliberately kept separate, and asking why they don't merge is asking the wrong question. A gap is something genuinely unfinished, worth closing when a real caller needs it.

### Design boundaries, not gaps

- **Descriptor names and raw handles are two separate worlds, and nothing bridges them on purpose.** `ShaderProcessor`'s named-binding maps (`m_bound_buffers`, section 5) and the `back_buffers`-facing primitives (`download_back_buffer`, `configure_back_buffers`) serve different concerns: one lets a shader author look something up by the name they gave it, the other moves bytes for state that was never given a name because it was never meant to go through descriptor binding in the first place. `BufferDownloadProcessor`/`BufferUploadProcessor` correspondingly never touch `ShaderBinding::set`/`binding` or `m_config.bindings` at all, in either the original path or the `back_buffers` path, for the same reason `BufferUploadProcessor` never grew descriptor awareness: that responsibility belongs to `ShaderProcessor` and its subclasses, not to the upload/download pair. Extending either processor to understand set/binding numbers would blur a boundary that exists so the two classes of processor stay each other's mirror image.
- **`ComputeProcessor`/`RenderProcessor` do not automatically read back their own output, and this is intentional.** Section 1 establishes why: tier 2 transfers cost a real fence round trip, and paying that cost every cycle whether or not anyone asked is the wrong default. `RelaxationGridBuffer` is the worked example of the right shape instead, an atomic request flag consumed in `on_after_execute`, gated so the cost is only paid when a snapshot is actually wanted. `download_bound` and `configure_back_buffers` make the manual step cheaper to write, they do not and should not make it automatic. Yantra's `GpuExecutionContext` (section 2) makes the opposite choice, unconditional readback, because it is solving a different problem: a node-graph layer where correctness by default matters more than avoiding an occasional unnecessary transfer. This layer stays manual because it is meant to be called every audio buffer or every frame, where an unconditional fence wait on every dispatch would be the wrong default instead.
- **`configure_target`/`configure_source` accept only a `VKBuffer` shared_ptr; `back_buffers` is reached through a separate, parallel entry point instead.** The primary-handle case and the `back_buffers` case differ enough in shape, one target vs. a vector of targets that must match `back_buffers.size()` at process time, that folding them into one overloaded signature would make the common, primary-handle case harder to read for no real benefit. `configure_back_buffers` exists alongside `configure_target`/`configure_source`, not merged into them.

### Genuine gaps

- **`back_buffers` upload has no fenced fallback for device-local entries.** `BufferUploadProcessor::configure_back_buffers` only supports host-visible entries (`mapped_ptr` present), erroring rather than transferring otherwise, because no fenced-upload-to-raw-handle primitive exists yet to fall back to. This is asymmetric with the download side, where `download_back_buffer` already has a fenced fallback branch. Worth closing when a device-local `back_buffers` user actually exists; nothing exercises this today.
- **`back_buffers` upload trusts the source's reported byte count rather than validating against the destination entry's expected size.** `VKBufferResources::GenerationSlot` carries no size field of its own, so a misconfigured source silently short-writes into an entry instead of erroring. Callers today get the correct size out-of-band (`RelaxationGridBuffer::get_state_bytes()`). Closing this properly would mean adding a size to `GenerationSlot` itself, which is a struct change, not a processor change, and should be weighed against how many other call sites assume the struct's current shape before it's done. CPU-GPU IO in MayaFlux

Reference map of every way data crosses the CPU-GPU boundary in MayaFlux, current as of the 0.4 development line. This document describes what exists today, not a target design.

## How to use this document

Find your situation in the table of contents. Each section names the exact call, states what it requires, and states what it does not do. Where a gap exists, it is named as a gap, not silently worked around.

## Contents

1. The three tiers of readback
2. Yantra: the Datum-driven GPGPU layer
3. VKBuffer processors: the raw layer
4. Binding data by name: descriptors, push constants, nodes
5. Getting data out by descriptor name, and moving whole back_buffers sets
6. Specialized buffer classes and why they differ
7. Decision table
8. Design boundaries and genuine gaps

---

## 1. The three tiers of readback

Before touching any API, understand which tier your situation falls into. Every readback path in MayaFlux is one of these three:

**Tier 0: never leave the GPU.** Compute writes a buffer, render reads the same buffer, no CPU involvement at any point. Fastest, and the default you should reach for whenever the CPU does not actually need the numbers.

**Tier 1: host-visible, direct read.** The buffer is allocated with `HOST_STORAGE`, `UNIFORM`, or another host-visible usage, meaning it has a coherent `mapped_ptr`. Read it with a plain pointer dereference or memcpy. No staging buffer, no fence, no `StagingUtils` call needed.

**Tier 2: device-local, staged transfer.** The buffer lives in fast device-local memory the CPU cannot see directly. Getting bytes out requires a copy to a staging buffer, a fence wait, then a read from the staging buffer's mapped memory. This is the only tier that does real transfer work, and it is the one you should gate behind an explicit request rather than running every frame.

Every class described below picks one of these three deliberately. `ComputeMeshBuffer` picks tier 0. A push-constant scalar might use tier 1. `RelaxationGridBuffer` uses tier 2, gated behind an atomic request flag.

---

## 2. Yantra: the Datum-driven GPGPU layer

This is the layer node authors touch. `Datum<T>` goes in, `Datum<T>` comes out, GPU residency is an implementation detail you do not manage by hand.

### Regular per-unit SSBO

Owned by `GpuResourceManager`, keyed by `(pipeline_unit_key, index)`.

- `ensure_buffer` allocates or resizes.
- `upload` / `upload_raw` write CPU data to the GPU buffer.
- `download` reads back via direct memcpy from `mapped_ptr`. This assumes host-visible memory (tier 1). There is no staging path at this layer.
- `bind_descriptor` wires the buffer into a descriptor slot.

### Shared SSBO

Same shape, but keyed by `(set, binding_index)` in `m_shared->slots` rather than per-unit. Multiple pipeline units binding the same slot share GPU state without a CPU round trip.

- `ensure_shared_buffer`
- `upload_shared_raw`
- `download_shared`
- `bind_shared_descriptor`

### GpuExecutionContext

Wraps the above with `dispatch_async` / `collect_result`. **Readback here is unconditional**: every `dispatch_core*` call runs `readback_primary` and `readback_aux` before returning. You do not opt in to getting your data back, you would have to opt out, and there is no opt-out. This is the correct default for the node-graph world and is the standard the raw-processor layer (section 3) does not match.

### TextureExecutionContext

For image data, `OutputMode` picks the exit path per node:

- `CONTAINER`: auto-downloads into a `TextureContainer`. Full round trip, no manual step.
- `SCALAR`: skips image download, returns SSBO readback only.
- `IMAGE`: skips CPU readback entirely, result stays GPU-resident for the next shader in the chain. This is tier 0 for textures.

**When to use this layer:** any time you are working through nodes, `Datum<T>`, or the standard node-graph pipeline. This is the default and should remain your first choice.

---

## 3. VKBuffer processors: the raw layer

This is the layer under Yantra: `ComputeProcessor`, `RenderProcessor`, and their relatives, attached directly to a `VKBuffer` via a `BufferProcessingChain`. Unlike Yantra, **readback is never automatic here**. You have to ask for it explicitly.

### Push constants

One-directional, CPU to GPU only. No readback concept applies; there is nothing to get back.

- Direct: `set_push_constant_data` on `ComputeProcessor` / `RenderProcessor`.
- Via node binding: `NodeBindingsProcessor::bind_node(name, node, offset, size)`, which reads a node's output into a push constant offset every cycle.

### UBO / SSBO descriptor bindings

Two entry points:

- Direct: `ShaderProcessor::bind_buffer(descriptor_name, vk_buffer)`. Registers a `VKBuffer` under a named descriptor slot, tracked in `m_bound_buffers`. If the name has no existing binding config, one is auto-created at `set=1` with the next free binding index.
- Via node interop: `DescriptorBindingsProcessor::bind_scalar_node` / `bind_vector_node` / `bind_matrix_node` / `bind_structured_node` / `bind_audio_buffer` / `bind_network`. Each of these allocates its own backing `VKBuffer` internally and calls `bind_buffer` on your behalf, so the descriptor name still resolves through the same `m_bound_buffers` map.

### Textures

`TextureLoom` owns creation, upload, and sampler binding. Not covered further here; this document is about buffer IO.

### Getting bytes out: where the manual step lives

`ComputeProcessor::execute_shader` ends every dispatch with a `buffer_barrier` transitioning the buffer to `eShaderRead | eTransferRead`, then submits. That barrier is the tell: the buffer is left ready for a transfer read, but the processor never issues one. You must add that step yourself, by one of:

- **`StagingUtils::download_from_gpu`** (or the specific `download_host_visible` / `download_device_local` variants), called directly against the `VKBuffer`. `download_from_gpu` auto-detects which variant applies. `download_from_gpu_async` records a fenced copy and waits on the fence rather than `queue.waitIdle`, safe to call off the graphics thread.
- **`BufferDownloadProcessor`**, a `VKBufferProcessor` attached to the chain alongside your compute or render processor. Its original `configure_target(source, target)` still only accepts a `VKBuffer` shared_ptr as source. It now also has `configure_back_buffers(source, targets)`, covering the case where the source is a `VKBuffer` whose actual live state sits in `back_buffers` rather than its primary handle. See section 5 for exact mechanics on both.

For rendered pixels rather than a buffer, `RenderProcessor` draws to an image, not a buffer, so neither of the above applies:

- `DisplayService::readback_swapchain_region` / `get_last_frame` reads the presented swapchain image. Lock-free, published by a per-window readback thread, read via atomic `shared_ptr`.

For vertex data written by compute and consumed again as geometry:

- `GeometryReadbackNode` wraps `download_from_gpu_async` and re-emits the result as a `GeometryWriterNode`. Flagged as needing manual `compute_frame()` invocation off the graphics thread, since it performs its own queue submission.

**When to use this layer:** when you need direct control over a `ComputeProcessor` or `RenderProcessor` outside the node graph, or when working with specialized buffer classes that manage their own GPU state.

---

## 4. Binding data by name: descriptors, push constants, nodes

Every binding in MayaFlux is reached through one of three name-keyed maps. Knowing which map your binding lives in tells you what you can and cannot do with it later.

| Map | Owner | Key | What it holds |
|---|---|---|---|
| `m_bound_buffers` | `ShaderProcessor` | descriptor name (string) | `VKBuffer` shared_ptr bound to a UBO/SSBO slot |
| `m_bindings` | `DescriptorBindingsProcessor` | logical name (string) | Node/AudioBuffer/network source plus its target descriptor name |
| `m_bindings` | `NodeBindingsProcessor` | logical name (string) | Node source plus a push constant byte offset |

`add_binding(descriptor_name, ShaderBinding)` on `ShaderProcessor` is where set and binding index are configured for a descriptor name, ahead of any buffer being bound to it. `ShaderBinding` carries `set`, `binding`, and `type`.

**The important asymmetry:** these maps only contain what was routed through them. Any processor that writes descriptors directly via `foundry.update_descriptor_buffer`, bypassing `bind_buffer`, is invisible to name-based lookup. `RelaxationStepProcessor` does exactly this for its ping-pong state (section 6). If a buffer was never named through `bind_buffer` or one of the `DescriptorBindingsProcessor::bind_*` calls, there is no name to look it up by, regardless of what API you reach for.

---

## 5. Getting data out by descriptor name, and moving whole back_buffers sets

You asked specifically whether you can specify *which* SSBO/UBO to download by its string key rather than by object identity, and separately whether raw auxiliary handles like `back_buffers` can be reached at all. Both are now covered, by two different primitives for two different binding worlds. Neither bridges into the other, because a string name only exists for the first world.

### Download by descriptor name: `ShaderProcessor::download_bound`

Resolves a descriptor name through `get_bound_buffer` and downloads through `download_from_gpu` in one call:

```cpp
std::vector<float> result;
compute_processor->download_bound("output", result);
```

or the raw-pointer form:

```cpp
compute_processor->download_bound("output", data_ptr, byte_count);
```

Returns `false` if the name has no bound buffer, rather than throwing, since an unbound name is a normal query-time condition. This only works for names bound through `bind_buffer` (directly, or via a `DescriptorBindingsProcessor::bind_*` call), since that is the only map `get_bound_buffer` can see.

### Download by raw handle, outside the named-binding system: `StagingUtils::download_back_buffer`

For a single raw slot that was never routed through `bind_buffer` at all, such as one entry of `RelaxationGridBuffer`'s `back_buffers`:

```cpp
const auto& slot = grid->get_buffer_resources().back_buffers[grid->front_index()];
std::shared_ptr<VKBuffer> staging; // reused across calls if the slot is ever device-local
download_back_buffer(slot, data_ptr, byte_count, staging);
```

Reads `slot.mapped_ptr` directly when the slot is host-visible, which is the case for every current user of `back_buffers` (tier 1, no transfer at all). Falls back to a fenced copy through the supplied staging buffer otherwise, the same shape as `download_from_gpu_async`. Keep `staging` alive across calls; a fresh local reallocates every call in the device-local fallback case.

### Whole back_buffers sets, every processing cycle: `configure_back_buffers` on both processors

Sections 2 and 3 already establish that `BufferDownloadProcessor`/`BufferUploadProcessor` are the chain-attached, every-cycle way to move data, in contrast to the one-shot calls above. Both processors originally only understood a `VKBuffer`'s primary handle, because `back_buffers` did not exist when they were written. They now also support downloading or uploading an entire `back_buffers` vector, one CPU buffer per entry, every cycle:

```cpp
download_processor->configure_back_buffers(grid, { target_slot_0, target_slot_1 });
chain->add_processor(download_processor, grid);
```

```cpp
upload_processor->configure_back_buffers(grid, { source_slot_0, source_slot_1 });
chain->add_processor(upload_processor, grid);
```

`targets`/`sources` must have exactly as many entries as `back_buffers` at process time, checked fresh every cycle, or the call is skipped with an error rather than partially applied. Entries are moved strictly in `back_buffers` order. Which entry is "front", "current", or otherwise meaningful is left entirely to the calling code, these processors only move bytes; interpret the returned targets the same way you would interpret a manual `download_back_buffer` result, by asking the source buffer directly (`front_index()` or equivalent).

The download path writes each entry into a correctly-sized raw byte buffer via `download_back_buffer`, then pushes it through `VKBuffer::set_data`, rather than attempting to obtain a writable view out of the target's existing data, which is not something the existing `DataAccess`/`get_data()` pattern can provide since that pattern is built for reading an existing variant, not producing a landing pad to write into. The upload path memcpys directly into `entry.mapped_ptr`; an entry without a mapped pointer is unsupported today and logged as an error rather than silently skipped, since no fenced-upload-to-raw-handle primitive exists yet to fall back to (the mirror of `download_back_buffer`'s fenced fallback branch does not yet exist on the upload side).

`configure_target`/`configure_source` and their original maps are completely untouched by this. `back_buffers` registration lives in a separate map on each processor, checked first in `processing_function`, falling through to the original path when a buffer isn't registered there.

### What still does not exist

- **No single call resolves a descriptor name to a raw `back_buffers` entry.** These remain two separate primitives for two separate binding worlds: `download_bound`/`bind_buffer` for anything name-addressable, `download_back_buffer`/`configure_back_buffers` for raw entries reachable via `get_buffer_resources()`. Nothing bridges them, because nothing needs to.
- **None of these primitives understand set/binding numbers.** All of them work off `VKBuffer` identity, raw slot identity, or `back_buffers` vector position, never `ShaderBinding::set` / `ShaderBinding::binding`. If two descriptor names happen to point at the same buffer, that is invisible to all of them.
- **`configure_target`/`configure_source` (the original, non-`back_buffers` entry points) are unchanged** and still only accept a `VKBuffer` shared_ptr as source or target. They were deliberately left alone rather than extended, since the primary-handle case and the `back_buffers` case are different enough concerns to warrant separate entry points rather than one overloaded signature trying to cover both.
- **Byte-count validation on the upload side trusts the source's own reported size, not the destination entry's expected size.** `upload_back_buffers` copies however many bytes the source `DataVariant` reports holding; nothing on `VKBufferResources::GenerationSlot` itself records an expected byte count per entry, so a misconfigured source silently short-writes into an entry rather than erroring. Callers today get the correct size out-of-band (`RelaxationGridBuffer::get_state_bytes()`); there is no generic way for the processor to check this itself yet.

---

## 6. Specialized buffer classes and why they differ

These three classes show the three tiers in practice, and each was built the way it was for a specific reason, not by accident.

### ComputeMeshBuffer: tier 0, by design

Compute writes vertices directly into the `VKBuffer` that `RenderProcessor` reads from. No CPU readback of vertex data, ever. Doxygen states this outright.

The one scalar that does cross back, a vertex count from an atomic counter, uses tier 1, not tier 2: `SDFMeshProcessor::on_after_execute` dereferences `m_counter_buf->get_mapped_ptr()` directly, because that counter buffer is `HOST_STORAGE` and coherent. No `download_from_gpu`, no staging buffer. When you only need one scalar from a host-visible buffer, skip the download abstraction entirely and read the pointer.

### RelaxationGridBuffer: tier 2, request-gated

The canonical shape for "give me a copy on demand, without paying for it every frame":

- State lives as raw handle pairs in `VKBufferResources::back_buffers`, ping-ponged by index. Deliberately never wrapped as a normal `VKBuffer`, so it is invisible to `bind_buffer`/`get_bound_buffer` and to `BufferDownloadProcessor`.
- `request_snapshot()` sets an atomic flag. Callable from any thread, wait-free.
- `RelaxationStepProcessor::on_after_execute` checks `consume_snapshot_request()` every cycle. Only on a hit does it call `StagingUtils::download_back_buffer` on the newly-front slot.
- Delivery is a `BroadcastSource<std::vector<uint8_t>>`, signaled once per fulfilled request, consumed via `co_await` or `Kriya::on_signal()`.

The `back_buffers` slots here are `HostVisible | HostCoherent`, so this is actually tier 1, not tier 2: `download_back_buffer` resolves to a direct `mapped_ptr` memcpy, no fence, no staging transfer. A processor-owned `VKBuffer` (`m_snapshot_staging`) is still kept and passed through on every call as a dormant fallback, lazily sized and ready only if `back_buffers` is ever backed by device-local memory in the future; today it is never touched. Either way, delivery costs nothing on cycles where nobody asked, and is event-driven rather than polled.

### Why the three differ

Each class earned its tier by what it actually needs, not by a missing feature elsewhere:

- `ComputeMeshBuffer` needs zero CPU visibility into vertex data, so it stays tier 0.
- The vertex counter needs one scalar, cheaply, from memory that is already coherent, so it uses tier 1 directly rather than routing through a general download processor.
- `RelaxationGridBuffer` needs occasional, full-state snapshots, gated behind an explicit request rather than run every cycle. In this case the underlying memory turned out to be host-visible, so the actual transfer is tier 1, but the request-gated, event-delivered shape is the right one regardless of tier, and is now backed by `download_back_buffer` (section 5) rather than an inline fenced copy.

If you are designing a new buffer class, pick your tier by the same reasoning: can it skip CPU visibility entirely, is a host-visible pointer read sufficient, or does it genuinely need gated device-to-host transfer.

---

## 7. Decision table

| I need to... | Use |
|---|---|
| Move a node graph value to the GPU and back | Yantra: `ensure_buffer` / `upload` / `download` |
| Share GPU state across multiple pipeline units without a CPU hop | Yantra shared SSBO: `ensure_shared_buffer` / `upload_shared_raw` / `download_shared` |
| Keep a texture result GPU-resident for the next shader | `TextureExecutionContext::OutputMode::IMAGE` |
| Auto-download a texture result | `TextureExecutionContext::OutputMode::CONTAINER` |
| Send a scalar to a shader every cycle from a node | `NodeBindingsProcessor::bind_node` (push constant) or `DescriptorBindingsProcessor::bind_scalar_node` (UBO/SSBO) |
| Bind an SSBO/UBO under a name I can look up later | `ShaderProcessor::bind_buffer(name, buffer)`, then `get_bound_buffer(name)` |
| Download a named descriptor's contents | `ShaderProcessor::download_bound(name, ...)`, one call |
| Keep compute output entirely on GPU for render to consume | Same `VKBuffer` in both processors' chain slots, no download at all |
| Read one scalar from a known host-visible buffer | Dereference `mapped_ptr` directly, skip staging entirely |
| Get occasional full-state snapshots from device-local memory without a per-frame cost | Atomic request flag, consumed in `on_after_execute`, delivered via `BroadcastSource`, as in `RelaxationStepProcessor` |
| Read presented pixels from a window | `DisplayService::readback_swapchain_region` / `get_last_frame` |
| Pull GPU-written vertex data back into the node graph | `GeometryReadbackNode` |
| Download a raw handle that lives in `VKBufferResources::back_buffers`, outside any named binding, once | `StagingUtils::download_back_buffer(slot, data, size, staging)` |
| Download or upload an entire `back_buffers` set, every processing cycle, as part of a chain | `BufferDownloadProcessor::configure_back_buffers` / `BufferUploadProcessor::configure_back_buffers` |

---

## 8. Design boundaries and genuine gaps

Two different things get called "gaps" in casual conversation about this system. This section keeps them apart: a boundary is a place two concerns were deliberately kept separate, and asking why they don't merge is asking the wrong question. A gap is something genuinely unfinished, worth closing when a real caller needs it.

### Design boundaries, not gaps

- **Descriptor names and raw handles are two separate worlds, and nothing bridges them on purpose.** `ShaderProcessor`'s named-binding maps (`m_bound_buffers`, section 4) and the `back_buffers`-facing primitives (`download_back_buffer`, `configure_back_buffers`) serve different concerns: one lets a shader author look something up by the name they gave it, the other moves bytes for state that was never given a name because it was never meant to go through descriptor binding in the first place. `BufferDownloadProcessor`/`BufferUploadProcessor` correspondingly never touch `ShaderBinding::set`/`binding` or `m_config.bindings` at all, in either the original path or the `back_buffers` path, for the same reason `BufferUploadProcessor` never grew descriptor awareness: that responsibility belongs to `ShaderProcessor` and its subclasses, not to the upload/download pair. Extending either processor to understand set/binding numbers would blur a boundary that exists so the two classes of processor stay each other's mirror image.
- **`ComputeProcessor`/`RenderProcessor` do not automatically read back their own output, and this is intentional.** Section 1 establishes why: tier 2 transfers cost a real fence round trip, and paying that cost every cycle whether or not anyone asked is the wrong default. `RelaxationGridBuffer` is the worked example of the right shape instead, an atomic request flag consumed in `on_after_execute`, gated so the cost is only paid when a snapshot is actually wanted. `download_bound` and `configure_back_buffers` make the manual step cheaper to write, they do not and should not make it automatic, since automatic readback on every dispatch is the behavior Yantra's `GpuExecutionContext` already provides for the node-graph layer. The raw-processor layer stays manual because that is where request-gating decisions belong.
- **`configure_target`/`configure_source` accept only a `VKBuffer` shared_ptr; `back_buffers` is reached through a separate, parallel entry point instead.** The primary-handle case and the `back_buffers` case differ enough in shape, one target vs. a vector of targets that must match `back_buffers.size()` at process time, that folding them into one overloaded signature would make the common, primary-handle case harder to read for no real benefit. `configure_back_buffers` exists alongside `configure_target`/`configure_source`, not merged into them.

### Genuine gaps

- **`back_buffers` upload has no fenced fallback for device-local entries.** `BufferUploadProcessor::configure_back_buffers` only supports host-visible entries (`mapped_ptr` present), erroring rather than transferring otherwise, because no fenced-upload-to-raw-handle primitive exists yet to fall back to. This is asymmetric with the download side, where `download_back_buffer` already has a fenced fallback branch. Worth closing when a device-local `back_buffers` user actually exists; nothing exercises this today.
- **`back_buffers` upload trusts the source's reported byte count rather than validating against the destination entry's expected size.** `VKBufferResources::GenerationSlot` carries no size field of its own, so a misconfigured source silently short-writes into an entry instead of erroring. Callers today get the correct size out-of-band (`RelaxationGridBuffer::get_state_bytes()`). Closing this properly would mean adding a size to `GenerationSlot` itself, which is a struct change, not a processor change, and should be weighed against how many other call sites assume the struct's current shape before it's done.
