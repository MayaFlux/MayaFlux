# **Visual Materiality: Part I**

_In MayaFlux, geometry isn't shapes you draw. It's data you generate.  
Points, vertices, colors: all numerical streams you sculpt and send to the GPU.  
This tutorial shows you the smallest visual gesture: a single point._

---

# **Tutorial: Points in Space**

```cpp
void compose() {
    auto window = MayaFlux::create_window({ "Visual", 800, 600 });

    auto point = vega.PointNode(glm::vec3(0.0f, 0.5f, 0.0f)) | Graphics;
    auto buffer = vega.GeometryBuffer(point) | Graphics;

    buffer->setup_rendering({ .target_window = window });
    window->show();
}
```

Run this. You see a white point in the upper half of the window.

Change `0.5f` to `-0.5f`. The point moves to the lower half. Try `0.0f, 0.0f, 0.0f`. It centers.

**That's it.** A point rendered from explicit coordinates. No primitives. Just three numbers.

---

## Expansion 1: What Is PointNode?

<details>
<summary>Click to expand: Points Are Vertex Data</summary>

`PointNode` is a **GeometryWriterNode**, a node that generates vertex data each frame.

```cpp
struct PointVertex {
    glm::vec3 position;  // x, y, z coordinates
    glm::vec3 color;     // r, g, b values
    float size;          // point size in pixels
};
```

When you create `vega.PointNode(glm::vec3(0.0f, 0.5f, 0.0f))`, you're setting:

- `position = {0.0, 0.5, 0.0}` (center horizontally, upper half, at origin depth)
- `color = {1.0, 1.0, 1.0}` (white, default)
- `size = 10.0` (10 pixels, default)

Every frame (at `GRAPHICS_BACKEND` - 60 FPS), `compute_frame()` writes these values into a CPU byte buffer. That buffer uploads to GPU as vertex data.

**Coordinates are normalized device coordinates (NDC):**

- X: -1.0 (left edge) → +1.0 (right edge)
- Y: -1.0 (bottom) → +1.0 (top)
- Z: -1.0 (near) → +1.0 (far)

The `| Graphics` token registers the node with the graphics subsystem to run at visual rate. Without it, the node exists but never processes.

</details>

---

## Expansion 2: GeometryBuffer Connects Node → GPU

<details>
<summary>Click to expand: The Upload Chain</summary>

`GeometryBuffer` does what `SoundContainerBuffer` does for audio: connects a data source to processing infrastructure.

```cpp
auto buffer = vega.GeometryBuffer(point) | Graphics;
```

This creates:

1. **VKBuffer** on GPU (device-local vertex buffer storage)
2. **GeometryBindingsProcessor** (default processor that handles CPU→GPU upload)
3. **BufferProcessingChain** (where you can add transformations)

The fluent `| Graphics` token calls `setup_processors(ProcessingToken::GRAPHICS_BACKEND)` behind the scenes, which:

- Creates the bindings processor
- Sets it as the default processor
- Binds the geometry node to this buffer
- Registers buffer with graphics subsystem scheduler

**Each frame cycle:**

1. Scheduler triggers buffer processing at `GRAPHICS_BACKEND`
2. Default processor runs: `GeometryBindingsProcessor::processing_function()`
3. Processor calls `point->compute_frame()` (generates vertex data)
4. Processor checks `point->needs_gpu_update()` (dirty flag)
5. If dirty: upload vertex bytes to GPU via staging buffer
6. Processing chain runs (currently empty, we'll add to it later)

This is **identical to audio flow**, just different data:

- Audio: `SoundContainer → SoundContainerBuffer → FilterProcessor → Speakers`
- Graphics: `PointNode → GeometryBuffer → [empty chain] → (needs RenderProcessor)`

**The separation:** Upload and rendering are separate processors. You can upload geometry without rendering it (for compute shader reads), or render the same geometry to multiple windows.

</details>

---

## Expansion 3: setup_rendering() Adds Draw Calls

<details>
<summary>Click to expand: RenderProcessor and the Rendering Pipeline</summary>

```cpp
buffer->setup_rendering({ .target_window = window });
```

This creates a **RenderProcessor** and adds it to the buffer's processing chain. Now the full flow is:

**Default Processor (GeometryBindingsProcessor):**

- Upload vertex data to GPU

**Processing Chain (RenderProcessor):**

- Record Vulkan draw commands
- Issue draw call
- Present to window

**What RenderProcessor does each frame:**

```cpp
// Simplified RenderProcessor::execute_shader() flow:
void RenderProcessor::execute_shader(VKBuffer* buffer) {
    auto cmd_id = foundry.begin_secondary_commands(color_format);
    auto cmd = foundry.get_command_buffer(cmd_id);

    flow.begin_render_pass(cmd_id, m_target_window);

    flow.bind_pipeline(cmd_id, m_pipeline_id);
    flow.bind_vertex_buffers(cmd_id, {buffer});

    flow.draw(cmd_id, vertex_count);  // vertex_count = 1 for PointNode

    foundry.end_commands(cmd_id);
}
```

**RenderFlow** is the high-level rendering API (analogous to RtAudio for audio). It wraps Vulkan command recording but you never touch Vulkan directly unless you want to.

**RenderConfig** lets you customize:

```cpp
Portal::Graphics::RenderConfig {
    .target_window = window,
    .vertex_shader = "point.vert.spv",     // Default: point rendering
    .fragment_shader = "point.frag.spv",   // Default: flat color
    .topology = PrimitiveTopology::POINT_LIST,  // POINT_LIST | LINE_LIST | TRIANGLE_LIST
    .polygon_mode = PolygonMode::FILL,
    .cull_mode = CullMode::NONE
}
```

**Critical: No draw loop.** You never write:

```cpp
while (!window.should_close()) {
    draw_something();
    swap_buffers();
}
```

The graphics subsystem runs its own thread, ticks at 60 FPS, processes visual-rate nodes, processes graphics buffers, presents frames. **You declare what to render. The engine handles when and how.**

</details>

---

## Expansion 4: Windowing and GLFW

<details>
<summary>Click to expand: Window Management Without Manual Event Loops</summary>

```cpp
auto window = MayaFlux::create_window({ "Visual", 800, 600 });
```

Creates a **GLFW window** (cross-platform windowing library) with:

- Title: "Visual"
- Size: 800x600 pixels
- Vulkan surface attached automatically
- Event handling registered with MayaFlux's event system

**WindowManager** handles GLFW event polling in the graphics thread. You don't call `glfwPollEvents()` yourself. It's handled by the subsystem's `process()` cycle.

```cpp
window->show();
```

Makes the window visible. Windows are created hidden by default so you can set everything up before display.

**Key architectural point:** Windows, like buffers, are resources managed by subsystems. You create them, register them for processing (via the `| Graphics` token on buffers), and the subsystem schedules their updates.

When you call `setup_rendering({ .target_window = window })`:

1. RenderProcessor stores window reference
2. Each frame, processor queries window's Vulkan swapchain
3. Records draw commands to swapchain framebuffer
4. Presents frame via DisplayService

**You never manually manage swapchains, framebuffers, or presentation.** That's RenderFlow's job.

</details>

---

## Expansion 5: The Fluent API and Separation of Concerns

<details>
<summary>Click to expand: Processor Architecture vs. Monolithic Rendering</summary>

Compare this to typical graphics framework code:

**Typical OpenFrameworks/Processing:**

```cpp
void draw() {
    ofBackground(0);
    ofSetColor(255);
    ofDrawCircle(width/2, height/2, 10);  // Immediate mode, mixed concerns
}
```

**MayaFlux:**

```cpp
auto point = vega.PointNode(glm::vec3(0.0, 0.0, 0.0)) | Graphics;
auto buffer = vega.GeometryBuffer(point) | Graphics;
buffer->setup_rendering({ .target_window = window });
```

**Separation:**

- **PointNode**: Data generation (position, color, size)
- **GeometryBuffer**: GPU memory management and upload
- **GeometryBindingsProcessor**: CPU→GPU transfer logic
- **RenderProcessor**: Vulkan command recording and presentation

Each processor has one job. You can:

- Replace GeometryBindingsProcessor with a compute shader that generates vertices on GPU
- Add processors between upload and render (transform vertex data, apply shaders)
- Render the same buffer to multiple windows with different shaders
- Upload geometry without rendering (for other buffers to read)

**The fluent API hides complexity without removing access:**

Fluent:

```cpp
auto buffer = vega.GeometryBuffer(point) | Graphics;
```

Explicit equivalent:

```cpp
auto buffer = std::make_shared<GeometryBuffer>(point);
buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
MayaFlux::register_buffer(buffer);
```

The `vega` proxy and `| Graphics` operator do the setup for you. But you can always access the explicit API when you need control.

</details>

---

## Try It

```cpp
// Change position (move to bottom-left)
auto point = vega.PointNode(glm::vec3(-0.8f, -0.8f, 0.0f)) | Graphics;

// Change color to red
auto point = vega.PointNode(
    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 0.0f),  // RGB: red
    10.0f                          // size
) | Graphics;

// Make it huge
auto point = vega.PointNode() | Graphics;
point->set_size(100.0f);

// Multiple points (each needs its own buffer for now)
auto p1 = vega.PointNode(glm::vec3(-0.5f, 0.0f, 0.0f)) | Graphics;
auto p2 = vega.PointNode(glm::vec3(0.5f, 0.0f, 0.0f)) | Graphics;

auto b1 = vega.GeometryBuffer(p1) | Graphics;
auto b2 = vega.GeometryBuffer(p2) | Graphics;

b1->setup_rendering({ .target_window = window });
b2->setup_rendering({ .target_window = window });
```

---

## What You've Learned

You now understand the complete graphics stack:

**Nodes:** Generate vertex data at visual rate (60 FPS)  
**Buffers:** Manage GPU memory and processor chains  
**Default Processor (GeometryBindingsProcessor):** Upload CPU data to GPU  
**Processing Chain (RenderProcessor):** Record draw commands and present  
**Tokens:** `| Graphics` registers components with visual-rate scheduler  
**No draw loop:** Subsystem handles timing, you declare structure

**Next:** Section 2 will show you `PointCollectionNode` for rendering many points efficiently, and Section 3 will drive point positions from audio nodes, finally crossing the audio/visual boundary.

---

# **Tutorial: Collections and Aggregation**

_In the previous section, you rendered a single point with its own buffer.  
Now: render many points with one buffer, one upload, one draw call.  
This is how you work with data at scale: aggregation, not repetition._

---

```cpp
void compose() {
    auto window = MayaFlux::create_window({ "Visual", 800, 600 });

    // Create a spiral of points
    auto points = vega.PointCollectionNode() | Graphics;

    for (int i = 0; i < 200; i++) {
        float t = i * 0.1f;
        float radius = t * 0.05f;
        float x = radius * std::cos(t);
        float y = radius * std::sin(t);

        // Color transitions from red → green → blue
        float hue = static_cast<float>(i) / 200.0f;
        glm::vec3 color(
            std::sin(hue * 6.28f) * 0.5f + 0.5f,
            std::sin(hue * 6.28f + 2.09f) * 0.5f + 0.5f,
            std::sin(hue * 6.28f + 4.19f) * 0.5f + 0.5f
        );

        points->add_point({ glm::vec3(x, y, 0.0f), color, 8.0f });
    }

    auto buffer = vega.GeometryBuffer(points) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();
}
```

Run this. You see a colorful spiral expanding from center to edges.

Change the formula. Try `float radius = std::sin(t) * 0.5f;` for a circular pattern. Try different color equations. The pattern is the same: generate point data procedurally, one buffer handles all of them.

**That's the key difference:** One `GeometryBuffer`, 200 vertices. One upload cycle, one draw call.

---

## Expansion 1: What Is PointCollectionNode?

<details>
<summary>Click to expand: Unstructured Aggregation</summary>

`PointCollectionNode` is a **GeometryWriterNode** that manages multiple `PointVertex` entries in a single vertex buffer.

```cpp
class PointCollectionNode : public GeometryWriterNode {
private:
    std::vector<PointVertex> m_points;  // CPU-side point storage
};
```

**Key methods:**

```cpp
points->add_point(PointVertex{position, color, size});     // Append
points->set_points(std::vector<PointVertex>);              // Replace all
points->update_point(size_t index, PointVertex);           // Modify one
points->clear_points();                                     // Empty
```

Each modification sets `m_vertex_data_dirty = true`. Next frame, `compute_frame()` uploads the entire collection:

```cpp
void PointCollectionNode::compute_frame() {
    if (m_points.empty()) {
        resize_vertex_buffer(0);
        return;
    }

    // Copy all points to flat vertex buffer
    set_vertices<PointVertex>(std::span{m_points.data(), m_points.size()});

    // Update vertex layout metadata
    auto layout = get_vertex_layout();
    layout->vertex_count = m_points.size();
    set_vertex_layout(*layout);
}
```

**Why "unstructured"?**

Points have no relationships. They're just a list of positions. No topology, no connectivity, no physics.

For particle systems with relationships (springs, forces, neighborhoods), use `ParticleNetwork` instead (covered in a future tutorial).

**When to use PointCollectionNode:**

- Static data visualization (plot 10,000 data points)
- Debug markers (show algorithm execution paths)
- Procedural forms (generated geometry like this spiral)
- Any collection where points don't interact

</details>

---

## Expansion 2: One Buffer, One Draw Call

<details>
<summary>Click to expand: Batching and GPU Efficiency</summary>

Compare two approaches:

**Inefficient (Section 1 pattern repeated):**

```cpp
for (int i = 0; i < 200; i++) {
    auto point = vega.PointNode(positions[i]) | Graphics;
    auto buffer = vega.GeometryBuffer(point) | Graphics;
    buffer->setup_rendering({.target_window = window});
}
```

Result:

- 200 buffers
- 200 upload operations per frame
- 200 draw calls
- Massive CPU→GPU bandwidth waste

**Efficient (current section):**

```cpp
auto points = vega.PointCollectionNode() | Graphics;
for (int i = 0; i < 200; i++) {
    points->add_point({positions[i], colors[i], 8.0f});
}
auto buffer = vega.GeometryBuffer(points) | Graphics;
buffer->setup_rendering({.target_window = window});
```

Result:

- 1 buffer
- 1 upload operation per frame (only if dirty)
- 1 draw call: `flow.draw(cmd_id, 200)`
- Minimal overhead

**How GeometryBindingsProcessor batches:**

```cpp
void GeometryBindingsProcessor::processing_function(Buffer* buffer) {
    // For EACH bound geometry node (but we only have one):
    auto vertices = binding.node->get_vertex_data();  // All 200 points

    size_t upload_size = vertices.size_bytes();  // 200 * sizeof(PointVertex)

    upload_to_gpu(
        vertices.data(),
        upload_size,
        binding.gpu_vertex_buffer,
        binding.staging_buffer  // If GPU buffer is device-local
    );
}
```

It uploads the entire vertex array as a contiguous block. GPU reads it sequentially, renders all points in one dispatch.

**This is fundamental to real-time graphics:** minimize state changes, batch identical operations, upload contiguous data.

</details>

---

## Expansion 3: RootGraphicsBuffer and Graphics Subsystem Architecture

<details>
<summary>Click to expand: How Rendering Actually Happens</summary>

You've been working with `GeometryBuffer`, a specialized buffer for vertex data. But there's a hidden orchestrator: **RootGraphicsBuffer**.

**The Graphics Subsystem runs its own thread:**

```cpp
void GraphicsSubsystem::graphics_thread_loop() {
    while (m_running.load()) {
        m_frame_clock->tick();  // Advance to next frame (60 FPS)

        // 1. Process visual-rate nodes (PointCollectionNode::compute_frame())
        m_handle->nodes.process(1);

        // 2. Process all graphics buffers
        m_handle->buffers.process(1);  // <-- This processes RootGraphicsBuffer

        // 3. Window management
        m_handle->windows.process();

        m_frame_clock->wait_for_next_frame();
    }
}
```

**Inside `m_handle->buffers.process(1)`, RootGraphicsBuffer executes:**

```cpp
class RootGraphicsBuffer : public Buffer {
private:
    std::vector<std::shared_ptr<VKBuffer>> m_child_buffers;  // Your GeometryBuffer is here
    std::shared_ptr<GraphicsBatchProcessor> m_default_processor;
    std::shared_ptr<PresentProcessor> m_final_processor;
};
```

**The processing sequence:**

1. **GraphicsBatchProcessor** (default processor) runs first:
   - Iterates through all child buffers (your GeometryBuffer)
   - Calls each buffer's default processor (GeometryBindingsProcessor - uploads vertices)
   - Calls each buffer's processing chain (RenderProcessor - records draw commands)
   - Collects buffers that have render commands ready

2. **PresentProcessor** (final processor) runs last:
   - Receives the list of buffers ready to render
   - Groups buffers by target window
   - For each window: creates primary command buffer, executes secondary command buffers, presents frame

**GraphicsBatchProcessor code (simplified):**

```cpp
void GraphicsBatchProcessor::processing_function(Buffer* buffer) {
    auto root_buf = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);

    // Process all child buffers
    for (auto& ch_buffer : root_buf->get_child_buffers()) {
        // Upload vertices (GeometryBindingsProcessor)
        if (ch_buffer->has_default_processor()) {
            ch_buffer->process_default();
        }

        // Run processing chain (RenderProcessor records commands)
        if (ch_buffer->has_processing_chain()) {
            ch_buffer->get_processing_chain()->process(ch_buffer);
        }

        // If buffer has render pipeline, mark it renderable
        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(ch_buffer)) {
            if (vk_buffer->has_render_pipeline()) {
                root_buf->add_renderable_buffer({
                    .buffer = vk_buffer,
                    .target_window = window,
                    .pipeline_id = pipeline_id,
                    .command_buffer_id = cmd_id
                });
            }
        }
    }
}
```

**PresentProcessor fallback renderer (simplified):**

```cpp
void PresentProcessor::fallback_renderer(RootGraphicsBuffer* root) {
    // Group buffers by window
    std::unordered_map<Window*, std::vector<BufferInfo>> buffers_by_window;

    for (const auto& renderable : root->get_renderable_buffers()) {
        buffers_by_window[renderable.target_window].push_back(renderable);
    }

    // Render each window
    for (const auto& [window, buffer_infos] : buffers_by_window) {
        // Begin dynamic rendering
        auto primary_cmd = foundry.begin_commands(GRAPHICS);
        flow.begin_rendering(primary_cmd, window, swapchain_image);

        // Execute all secondary command buffers for this window
        std::vector<vk::CommandBuffer> secondaries;
        for (const auto& info : buffer_infos) {
            secondaries.push_back(info.command_buffer);
        }
        primary_cmd.executeCommands(secondaries);

        flow.end_rendering(primary_cmd, window);

        // Submit and present
        display_service->submit_and_present(window, primary_cmd);
    }
}
```

**Why this architecture?**

- **Separation:** Upload (GeometryBindingsProcessor) and rendering (RenderProcessor) are distinct
- **Batching:** All uploads happen first, then all rendering
- **Multi-window:** Same geometry can render to multiple windows
- **No draw loop:** You never write `while (!should_close())` - the subsystem handles timing

</details>

---

## Expansion 4: Dynamic Rendering (Vulkan 1.3)

<details>
<summary>Click to expand: No Render Passes, Just Begin/End Rendering</summary>

MayaFlux uses **Vulkan 1.3 dynamic rendering**, which means no `VkRenderPass` objects.

**Traditional Vulkan (pre-1.3):**

- Create `VkRenderPass` ahead of time
- Specify all attachments (color, depth) statically
- Pipeline tied to render pass compatibility
- Inflexible: changing attachments requires new pipelines

**Dynamic Rendering (Vulkan 1.3+):**

- No `VkRenderPass` objects
- Call `vkCmdBeginRendering()` with inline attachment info
- Specify attachments per-frame dynamically
- Flexible: same pipeline works with different attachments

**RenderFlow::begin_rendering() implementation:**

```cpp
void RenderFlow::begin_rendering(
    CommandBufferID cmd_id,
    Window* window,
    vk::Image swapchain_image,
    const std::array<float, 4>& clear_color)
{
    auto cmd = foundry.get_command_buffer(cmd_id);

    // Inline color attachment info
    vk::RenderingAttachmentInfo color_attachment;
    color_attachment.imageView = swapchain_image_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.clearValue.color = vk::ClearColorValue(std::array{
        clear_color[0], clear_color[1], clear_color[2], clear_color[3]
    });

    // Dynamic rendering info
    vk::RenderingInfo rendering_info;
    rendering_info.renderArea = {{0, 0}, {width, height}};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

    // Begin rendering - no render pass object!
    cmd.beginRendering(rendering_info);
}
```

**Why this matters:**

- Post-processing chains: Render to texture A, then B, then screen (no pipeline recreation)
- Multi-pass rendering: Different attachments per pass without render pass combinatorial explosion
- Flexibility: Attachments decided at runtime, not compile time

**From your perspective:** Invisible. `RenderProcessor` handles it. But it enables advanced techniques later without architectural changes.

</details>

---

## Try It

```cpp
// Lissajous curve (parametric 2D oscillation)
void compose() {
    auto w1 = MayaFlux::create_window({ "lissajous", 800, 600 });
    auto w2 = MayaFlux::create_window({ "waves", 800, 600 });

    auto lissajous = vega.PointCollectionNode() | Graphics;
    for (int i = 0; i < 500; i++) {
        float t = i * 0.02f;
        float x = std::sin(3.0f * t) * 0.8f;
        float y = std::cos(5.0f * t) * 0.8f;
        float intensity = (std::sin(t * 2.0f) + 1.0f) * 0.5f;

        lissajous->add_point({ glm::vec3(x, y, 0.0f),
            glm::vec3(intensity, 1.0f - intensity, 0.5f),
            6.0f });
    }

    // Wave interference pattern
    auto waves = vega.PointCollectionNode() | Graphics;
    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {
            float nx = x / 20.0f;
            float ny = y / 20.0f;
            float dist = std::sqrt(nx * nx + ny * ny);
            float wave = std::sin(dist * 10.0f) * 0.5f + 0.5f;

            waves->add_point({ glm::vec3(nx * 0.9f, ny * 0.9f, 0.0f),
                glm::vec3(wave, wave, 1.0f),
                4.0f });
        }
    }

    auto b1 = vega.GeometryBuffer(waves) | Graphics;
    b1->setup_rendering({ .target_window = w1 });

    auto b2 = vega.GeometryBuffer(lissajous) | Graphics;
    b2->setup_rendering({ .target_window = w2 });

    w1->show();
    w2->show();
}
```

---

## What You've Learned

**PointCollectionNode:** Aggregate many vertices in one node  
**Batching:** One buffer, one upload, one draw call - critical for performance  
**RootGraphicsBuffer:** Hidden orchestrator managing all graphics buffers  
**GraphicsBatchProcessor:** Default processor coordinating child buffer processing  
**PresentProcessor:** Final processor grouping buffers by window and presenting frames  
**Graphics Subsystem Thread:** Self-driven loop running at 60 FPS, no manual event loops  
**Dynamic Rendering:** Vulkan 1.3 approach with `vkCmdBeginRendering`, no render pass objects

**Next:** Section 3 crosses domains. We'll drive point positions from audio nodesfor live audio analysis moving visual data. Finally: numbers are just numbers.

---

# **Tutorial: Time and Updates**

_In the previous sections, geometry was static, created once in `compose()`.  
Now: geometry that evolves. Points that grow, move, disappear.  
This is where MayaFlux's architecture reveals its power: no draw loop, updates on your terms._

---

```cpp
void compose() {
    auto window = MayaFlux::create_window({ "Growing Spiral", 1920, 1080 });

    auto points = vega.PointCollectionNode(256) | Graphics;
    auto buffer = vega.GeometryBuffer(points) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();

    // Grow spiral over time: 10 new points per frame
    static float angle = 0.0f;
    static float radius = 0.0f;

    MayaFlux::schedule_metro(0.016, [points]() {  // ~60 Hz
        angle += 0.02f;
        radius += 0.001f;

        // Reset when spiral fills screen
        if (radius > 2.0f) {
            points->clear_points();
            radius = 0.0f;
        }

        // Add 10 new points this frame
        for (int i = 0; i < 10; ++i) {
            float local_angle = angle + (i / 10.0f) * 6.28f;
            float x = std::cos(local_angle) * radius;
            float y = std::sin(local_angle) * radius;

            float brightness = 1.0f - radius;
            points->add_point({
                glm::vec3(x, y, 0.0f),
                glm::vec3(brightness, brightness * 0.8f, brightness * 0.5f),
                10.0f + (i * 2.0f)
            });
        }
    });
}
```

Run this. The spiral grows from center, fades as it expands, then resets and repeats.

**That's the pattern:** Create geometry once. Schedule updates whenever you want. The graphics subsystem handles the rest.

---

## Expansion 1: No Draw Loop, `compose()` Runs Once

<details>
<summary>Click to expand: Why This Is Different</summary>

**Typical graphics framework (Processing, openFrameworks):**

```cpp
// Global state (forced pattern)
std::vector<Point> points;

void setup() {
    // Initialize once
}

void draw() {
    background(0);

    // MUST redraw everything, every frame, forever
    for (auto& p : points) {
        circle(p.x, p.y, 10);
    }

    // Update state for next frame
    updatePhysics();
}

// Hidden main loop you don't control:
while (running) {
    draw();      // Called automatically at framerate
    swap_buffers();
}
```

**Problems:**

1. **Forced void:** `draw()` is a mandatory callback you're trapped in
2. **Global state:** Everything must be accessible from both `setup()` and `draw()`
3. **No separation:** Update and render are coupled in `draw()`
4. **No timing control:** Can't easily say "update every 2 frames" or "render at 120 FPS, update at 30 FPS"

---

**MayaFlux:**

```cpp
void compose() {
    // Setup happens once
    auto points = vega.PointCollectionNode() | Graphics;
    auto buffer = vega.GeometryBuffer(points) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    // Schedule updates independently
    MayaFlux::schedule_metro(0.033, [points]() {  // 30 Hz updates
        points->add_point({/* ... */});
    });

    // Rendering happens at 60 FPS automatically in graphics thread
    // No coupling, no forced structure
}
```

**`compose()` is not a loop.** It runs once at startup. You declare structure, not repeated execution.

**Updates are scheduled, not looped:** You schedule tasks that run at specific intervals using the task scheduler. These run independently from rendering.

**Rendering is automatic:** The graphics subsystem thread runs at 60 FPS, processes visual-rate nodes, uploads geometry, issues draw calls, presents frames. You never write that loop.

</details>

---

## Expansion 2: Multiple Windows Without Offset Hacks

<details>
<summary>Click to expand: Different Content, Different Windows</summary>

Because each buffer targets a specific window through `setup_rendering()`, you don't need to offset or partition your geometry. Just create separate content for separate windows:

```cpp
void compose() {
    auto window1 = MayaFlux::create_window({ "Spiral", 800, 600 });
    auto window2 = MayaFlux::create_window({ "Grid", 800, 600 });

    // Spiral in window 1
    auto spiral = vega.PointCollectionNode() | Graphics;
    auto spiral_buffer = vega.GeometryBuffer(spiral) | Graphics;
    spiral_buffer->setup_rendering({ .target_window = window1 });

    // Grid in window 2
    auto grid = vega.PointCollectionNode() | Graphics;
    auto grid_buffer = vega.GeometryBuffer(grid) | Graphics;
    grid_buffer->setup_rendering({ .target_window = window2 });

    window1->show();
    window2->show();

    // Update spiral
    MayaFlux::schedule_metro(0.016, [spiral]() {
        /* add spiral points */
    });

    // Update grid independently
    MayaFlux::schedule_metro(0.033, [grid]() {
        /* add grid points */
    });
}
```

**What PresentProcessor does:**

```cpp
// Groups buffers by target window
std::unordered_map<Window*, std::vector<BufferInfo>> buffers_by_window;
buffers_by_window[window1].push_back(spiral_buffer_info);
buffers_by_window[window2].push_back(grid_buffer_info);

// Each window gets its own rendering pass
for (auto& [window, buffer_infos] : buffers_by_window) {
    begin_dynamic_rendering(window);
    for (auto& info : buffer_infos) {
        execute_secondary_command_buffer(info.cmd_buffer);
    }
    end_dynamic_rendering(window);
    present(window);
}
```

**Key point:** You're not rendering to one framebuffer and offsetting content. Each window is a separate rendering target. No manual viewport management, no draw call coordination.

Traditional frameworks force you to either:

- Render everything to one window and manually partition
- Manage multiple OpenGL contexts yourself
- Fight the framework's assumptions

MayaFlux: just create another window, point a buffer at it. Done.

</details>

---

## Expansion 3: Update Timing: Three Approaches

<details>
<summary>Click to expand: Metro Tasks, Coroutines, and Node Ticks</summary>

MayaFlux gives you three ways to schedule updates, each for different use cases:

### Approach 1: `schedule_metro` (Simplest)

```cpp
MayaFlux::schedule_metro(interval_seconds, callback);
```

Schedules a callback to run at fixed intervals using the **TaskScheduler**:

```cpp
MayaFlux::schedule_metro(0.016, [points]() {  // ~60 Hz
    points->add_point({/* ... */});
});
```

**Under the hood:**

```cpp
// Creates a SoundRoutine coroutine
auto metro_routine = [](Vruta::TaskScheduler& scheduler) -> Vruta::SoundRoutine {
    auto& promise = co_await Kriya::GetAudioPromise{};
    uint64_t interval_samples = scheduler.seconds_to_samples(interval_seconds);

    while (!promise.should_terminate) {
        callback();  // Your function
        co_await Kriya::SampleDelay{interval_samples};
    }
};

scheduler->add_task(std::make_shared<Vruta::SoundRoutine>(metro_routine(*scheduler)));
```

**When to use:** Simple periodic updates. Most common case.

---

### Approach 2: Coroutines with `Sequence` or `EventChain`

**EventChain** for sequential timed events:

```cpp
Kriya::EventChain()
    .then([]() { points->clear_points(); })
    .then([]() {
        for (int i = 0; i < 50; i++) {
            points->add_point({/* ... */});
        }
    }, 0.5)  // After 0.5 seconds
    .then([]() { /* do something else */ }, 1.0)  // After another 1.0 seconds
    .start();
```

**Under the hood:**

```cpp
auto coroutine_func = [](Vruta::TaskScheduler& scheduler, EventChain* chain) -> Vruta::SoundRoutine {
    for (const auto& event : chain->m_events) {
        co_await Kriya::SampleDelay{scheduler.seconds_to_samples(event.delay_seconds)};
        event.action();  // Execute callback
    }
};
```

**When to use:** Multi-step animations with specific timing. State machines. Choreographed sequences.

**Note:** `Sequence` is similar but less commonly used for graphics. See project knowledge for details.

---

### Approach 3: Node `on_tick` Callbacks

Every node can register callbacks that fire whenever it processes a sample:

```cpp
points->on_tick([points](const Nodes::NodeContext& ctx) {
    static int frame_count = 0;
    if (++frame_count % 60 == 0) {  // Every 60 calls
        points->add_point({
            glm::vec3(
                MayaFlux::get_uniform_random(-1.0f, 1.0f),
                MayaFlux::get_uniform_random(-1.0f, 1.0f),
                0.0f
            ),
            glm::vec3(1.0f),
            10.0f
        });
    }
});
```

**When the callback fires:**

```cpp
// Inside PointCollectionNode::compute_frame() or process_sample():
void notify_tick(double value) override {
    update_context(value);
    auto& ctx = get_last_context();

    // Unconditional callbacks
    for (auto& callback : m_callbacks) {
        callback(ctx);
    }

    // Conditional callbacks
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(ctx)) {
            callback(ctx);
        }
    }
}
```

**NodeContext contains:**

```cpp
struct NodeContext {
    double value;           // Current output value
    std::string type_id;    // Node type identifier
    // Derived classes add more info (e.g., FilterContext adds history buffers)
};
```

**When to use:** Tight coupling with node processing. Sample-accurate reactions. Rare for graphics (more common for audio analysis driving visual changes).

---

**Comparison:**

| Method           | Timing Precision               | Complexity | Use Case             |
| ---------------- | ------------------------------ | ---------- | -------------------- |
| `schedule_metro` | Sample-accurate (~1ms @ 48kHz) | Low        | Periodic updates     |
| `EventChain`     | Sample-accurate                | Medium     | Multi-step sequences |
| `on_tick`        | Per-process call               | Low        | Node-coupled logic   |

</details>

---

## Expansion 4: Clearing vs. Replacing vs. Updating

<details>
<summary>Click to expand: Four Update Patterns</summary>

### Pattern 1: Additive Growth (Original Example)

```cpp
MayaFlux::schedule_metro(0.016, [points]() {
    points->add_point({/* new point */});  // Grows indefinitely
});
```

Points accumulate until cleared. Trail effects, particle emissions, growing forms.

---

### Pattern 2: Full Replacement

```cpp
MayaFlux::schedule_metro(0.016, [points]() {
    std::vector<Nodes::PointVertex> new_points;

    // Generate entirely new point set
    for (int i = 0; i < 100; i++) {
        new_points.push_back({/* compute position */});
    }

    points->set_points(new_points);  // Replace all
});
```

**Internally:**

```cpp
void PointCollectionNode::set_points(const std::vector<PointVertex>& points) {
    m_points.clear();
    m_points = points;
    m_vertex_data_dirty = true;  // Triggers upload next frame
}
```

Entire buffer replaced. Simulations that recompute all positions (physics, flocking).

---

### Pattern 3: Selective Updates

```cpp
MayaFlux::schedule_metro(0.016, [points]() {
    for (size_t i = 0; i < points->get_point_count(); i++) {
        auto p = points->get_point(i);

        // Modify position
        p.position.x += std::sin(i * 0.1f) * 0.01f;
        p.position.y += std::cos(i * 0.1f) * 0.01f;

        points->update_point(i, p);  // Updates single point
    }
});
```

Modifies existing points in place. Mesh deformations, vertex animations.

---

### Pattern 4: Conditional Clearing

```cpp
static float radius = 0.0f;

MayaFlux::schedule_metro(0.016, [points]() {
    radius += 0.01f;

    if (radius > 1.5f) {
        points->clear_points();  // Reset
        radius = 0.0f;
        return;
    }

    points->add_point({/* ... */});
});
```

Growth with periodic reset. Cyclical animations.

</details>

---

## Try It

```cpp
// Pendulum with physics
void compose() {
    auto window = MayaFlux::create_window({ "Pendulum Trail", 800, 800 });

    auto trail = vega.PointCollectionNode() | Graphics;
    auto buffer = vega.GeometryBuffer(trail) | Graphics;
    buffer->setup_rendering({ .target_window = window });
    window->show();

    static float angle = 1.5f;
    static float velocity = 0.0f;
    const float length = 0.7f;
    const float gravity = 9.81f;
    const float dt = 0.016f;

    MayaFlux::schedule_metro(dt, [trail]() {
        // Physics
        float acceleration = -(gravity / length) * std::sin(angle);
        velocity += acceleration * dt;
        angle += velocity * dt;
        velocity *= 0.999f;  // Damping

        // Position
        float x = std::sin(angle) * length;
        float y = -std::cos(angle) * length;

        // Add to trail
        float hue = (angle + 3.14f) / 6.28f;
        trail->add_point({
            glm::vec3(x, y, 0.0f),
            glm::vec3(
                std::sin(hue * 6.28f) * 0.5f + 0.5f,
                std::sin(hue * 6.28f + 2.09f) * 0.5f + 0.5f,
                std::sin(hue * 6.28f + 4.19f) * 0.5f + 0.5f
            ),
            8.0f
        });

        // Keep last 500
        if (trail->get_point_count() > 500) {
            auto points = trail->get_points();
            points.erase(points.begin());
            trail->set_points(points);
        }
    });
}
```

---

## What You've Learned

**`compose()` runs once:** Setup, not a loop  
**`schedule_metro`:** Periodic callbacks via task scheduler  
**EventChain:** Sequential timed events with coroutines  
**`on_tick`:** Node-coupled callbacks fired per-process  
**Update patterns:** Additive, replacement, selective, conditional  
**Multiple windows:** Separate content, separate targets, no offsets  
**Dirty flags:** Geometry uploads only when changed

**Next:** Section 4 crosses domains. Audio nodes drive point positions, i.e live audio analysis controlling visual data. Numbers driving numbers, no artificial boundaries.

# **Tutorial: Audio → Geometry**

```cpp
void settings() {
    auto& stream = MayaFlux::Config::get_global_stream_info();
    stream.input.enabled = true;
    stream.input.channels = 1;
}

void compose() {
    auto window = MayaFlux::create_window({ "Reactive", 1920, 1080 });

    // Audio source: modulated drone
    auto carrier = vega.Sine(220.0) | Audio;
    auto modulator = vega.Sine(0.3) | Audio; // Slow amplitude modulation
    auto envelope = vega.Polynomial([](double x) { return (x + 1.0) * 0.5; }) | Audio;
    auto modded = modulator >> envelope; // Convert -1..1 to 0..1

    // Play the modulated audio
    auto audio_out = vega.Polynomial([modded](double x) {
        return x * modded->get_last_output();
    }) | Audio;
    carrier >> audio_out;

    // Particles
    auto particles = vega.ParticleNetwork(
                         500,
                         glm::vec3(-1.5f, -1.0f, -0.5f),
                         glm::vec3(1.5f, 1.0f, 0.5f),
                         ParticleNetwork::InitializationMode::RANDOM_VOLUME)
        | Graphics;

    particles->set_gravity(glm::vec3(0.0f, -2.0f, 0.0f));
    particles->set_drag(0.02f);
    particles->set_bounds_mode(ParticleNetwork::BoundsMode::BOUNCE);
    particles->set_output_mode(NodeNetwork::OutputMode::GRAPHICS_BIND);

    // The crossing: envelope node controls gravity
    particles->map_parameter("gravity", envelope, NodeNetwork::MappingMode::BROADCAST);

    auto buffer = vega.NetworkGeometryBuffer(particles) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();
}
```

Run this. You hear a slowly pulsing drone. Particles fall heavily when the sound swells, float when it recedes. The same envelope shapes both audio amplitude and gravitational force.

That's the paradigm shift. One node (one stream of numbers) simultaneously controls audio loudness and particle physics. Not because we "routed" audio to visuals. Because they were never separate.

---

## Expansion 1: What Are NodeNetworks?

<details>
<summary>Click to expand: Many Nodes, One Structure</summary>

You've worked with individual **Nodes** i.e sample-by-sample processors. **NodeNetworks** coordinate _many_ nodes with relationships between them.

| Aspect        | Node                 | NodeNetwork                             |
| ------------- | -------------------- | --------------------------------------- |
| Scale         | Single unit          | 100–1000+ internal nodes                |
| Relationships | None (pure function) | Topology-defined (spatial, chain, ring) |
| Output        | One value per sample | Aggregated or per-node                  |
| Use case      | Signal processing    | Emergent behavior, simulations          |

**Available networks:**

- `ParticleNetwork`: N-body physics simulation (particles with forces, velocities, collisions)
- `ModalNetwork`: Physical modeling (resonant modes, coupled oscillators)

**ParticleNetwork** treats each particle as a `PointNode` (geometry) with physics state:

```cpp
struct Particle {
    std::shared_ptr<PointNode> point;  // Position, color, size
    glm::vec3 velocity;
    glm::vec3 force;
    float mass;
};
```

Every frame, physics integrates: forces → velocities → positions. The `PointNode` positions update. GPU receives new vertex data.

</details>

---

## Expansion 2: Parameter Mapping from Buffers

<details>
<summary>Click to expand: How Buffer Data Reaches Particle Physics</summary>

When you write:

```cpp
particles->map_parameter("gravity", envelope, NodeNetwork::MappingMode::BROADCAST);
```

MayaFlux stores the mapping. Each physics step, `ParticleNetwork::update_mapped_parameters()` reads from the node:

```cpp
void ParticleNetwork::update_mapped_parameters() {
    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode == MappingMode::BROADCAST && mapping.broadcast_source) {
            double value = mapping.broadcast_source->get_last_output();
            apply_broadcast_parameter(mapping.param_name, value);
        }
    }
}
```

**The key**: Nodes have `get_last_output()`, a single queryable value from their most recent sample. The particle network runs at visual rate (60Hz), the node runs at audio rate (48kHz). The network simply reads whatever value the node last computed.

**No conversion, no special routing**. The node doesn't know it's controlling particles. The particles don't know the value came from audio synthesis. Numbers flow through the same substrate.

</details>

---

## Expansion 3: NetworkGeometryBuffer Aggregates for GPU

<details>
<summary>Click to expand: 500 Particles, One Draw Call</summary>

`NetworkGeometryBuffer` does for particle networks what `PointCollectionNode` did for manual points: aggregate many vertices into one GPU upload.

```cpp
auto buffer = std::make_shared<NetworkGeometryBuffer>(particles);
buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
buffer->setup_rendering({ .target_window = window });
```

**Each frame:**

1. `NodeGraphManager` calls `particles->process_batch(1)` (physics step)
2. Physics updates: forces → velocities → positions
3. `NetworkGeometryProcessor` extracts all 500 `PointNode` positions
4. Uploads to GPU as single contiguous vertex buffer
5. One draw call renders everything

Without aggregation, 500 particles would mean 500 buffers, 500 uploads, 500 draw calls. That's GPU-hostile. `NetworkGeometryBuffer` batches automatically.

</details>

---

## Try It

```cpp
// Inverse relationship: quiet = chaos, loud = order
void compose() {
    auto window = MayaFlux::create_window({ "Inverse", 1920, 1080 });

    // Chaotic source: noise filtered into slow undulation
    auto noise = vega.Random() | Audio;
    auto smooth = vega.IIR(std::vector { 0.001 }, std::vector { 1.0, -0.999 }) | Audio; // Very slow smoothing
    noise >> smooth;

    // Invert: high values → low output, low values → high output
    auto inverter = vega.Polynomial([](double x) {
        return 1.0 - std::clamp((x + 1.0) * 0.5, 0.0, 1.0); // Flip and normalize
    }) | Audio;
    auto mod = smooth >> inverter;

    // Audio: the raw smoothed noise as drone
    auto drone = vega.Sine(80.0) | Audio;
    auto amp = vega.Polynomial([mod](double x) { return x * mod->get_last_output() * 0.3; }) | Audio;
    drone >> amp;
    smooth >> amp;

    auto particles = vega.ParticleNetwork(
                         400,
                         glm::vec3(-1.5f, -1.5f, -0.5f),
                         glm::vec3(1.5f, 1.5f, 0.5f))
        | Graphics;
    particles->set_topology(NodeNetwork::Topology::SPATIAL);
    particles->set_interaction_radius(0.6f);
    particles->set_spring_stiffness(0.3f);
    particles->set_gravity(glm::vec3(0.0f, 0.0f, 0.0f));
    particles->set_drag(0.05f);
    particles->set_output_mode(NodeNetwork::OutputMode::GRAPHICS_BIND);

    // Inverted signal → turbulence: when audio swells, particles calm; when audio recedes, chaos
    particles->map_parameter("turbulence", inverter, NodeNetwork::MappingMode::BROADCAST);

    auto buffer = vega.NetworkGeometryBuffer(particles) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();

}
```

Sound and stillness invert. The drone grows loud, particles settle into structure. The drone fades, particles scatter into turbulence.

---

# **Tutorial: Logic Events → Visual Impulse**

```cpp
void compose() {
    auto window = MayaFlux::create_window({ "Event Reactive", 1920, 1080 });

    // Irregular pulse source: noise → threshold creates stochastic triggers
    auto noise = vega.Random();
    noise->set_amplitude(0.3f);
    auto slow_filter = vega.IIR(std::vector { 0.01 }, std::vector { 1.0, -0.99 }) | Audio;
    noise >> slow_filter;

    // Derivative approximation (emphasizes change, not level)
    auto diff = vega.IIR(std::vector { 1.0, -1.0 }, std::vector { 1.0 }) | Audio;
    slow_filter >> diff;

    // Rectify and smooth
    auto rect = vega.Polynomial([](double x) { return std::abs(x); }) | Audio;
    diff >> rect;

    auto smooth = vega.IIR(std::vector { 0.1 }, std::vector { 1.0, -0.9 }) | Audio;
    rect >> smooth;

    // Threshold into logic: fires when change exceeds threshold
    auto event_logic = vega.Logic(LogicOperator::THRESHOLD, 0.008) | Audio;
    smooth >> event_logic;

    // Particles in spherical formation
    auto particles = vega.ParticleNetwork(
                         300,
                         glm::vec3(-1.0f, -1.0f, -1.0f),
                         glm::vec3(1.0f, 1.0f, 1.0f),
                         ParticleNetwork::InitializationMode::SPHERE_SURFACE)
        | Graphics;

    particles->set_gravity(glm::vec3(0.0f, 0.0f, 0.0f));
    particles->set_drag(0.04f);
    particles->set_bounds_mode(ParticleNetwork::BoundsMode::NONE);
    particles->set_attraction_point(glm::vec3(0.0f, 0.0f, 0.0f));

    auto buffer = vega.NetworkGeometryBuffer(particles) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();

    // On logic rising edge: breathing impulse
    event_logic->on_change_to(true, [particles](const Nodes::NodeContext& ctx) {
        // Radial expansion from center
        for (auto& particle : particles->get_particles()) {
            glm::vec3 pos = particle.point->get_position();
            glm::vec3 outward = glm::normalize(pos) * 3.0f;
            particle.velocity += outward;
        }
    }); // true = rising edge (false→true)
}
```

Run this. Stochastic events emerge from filtered noise which is unpredictable but fully random. Each event triggers a click and a radial breath. The sphere pulses with emergent rhythm, not metronomic time.

The chain detects change in the noise contour, not amplitude. Slow drifts pass silently. Sharp inflections trigger events. The same logic detection pattern from the original, but driven by generative source rather than external input.

---

## Expansion 1: Logic Processor Callbacks

<details>
<summary>Click to expand: Edge Detection for Discrete Events</summary>

Logic processors output binary values (0.0 or 1.0). For visual events, you want _transitions_, not continuous states:

| Callback                    | When It Fires                    |
| --------------------------- | -------------------------------- |
| `on_rising_edge(callback)`  | Once per false→true transition   |
| `on_falling_edge(callback)` | Once per true→false transition   |
| `on_any_edge(callback)`     | Any transition                   |
| `while_true(callback)`      | Continuously while state is true |

**For transient detection, you want the rising edge:**

```cpp
logic_proc->on_rising_edge([particles]() {
    // Fires ONCE per detected transient
    particles->apply_global_impulse(/* ... */);
});
```

If you used `while_true()`, you'd fire continuously while the transient is detected (potentially many frames). `on_rising_edge()` fires once, precisely at the moment of detection.

</details>

---

## Expansion 2: Transient Detection Chain

<details>
<summary>Click to expand: Derivative → Rectify → Smooth → Threshold</summary>

The transient detection chain:

```
mic → diff → rect → smooth → threshold
```

**Step 1: Derivative (emphasize change)**

```cpp
auto diff = vega.IIR({1.0, -1.0}, {1.0});
```

Output is approximately `current - previous`. Sustained tones produce near-zero. Sharp attacks produce spikes.

**Step 2: Rectify (absolute value)**

```cpp
auto rect = vega.Polynomial([](double x) { return std::abs(x); });
```

Both positive and negative spikes become positive. We care about _magnitude_ of change, not direction.

**Step 3: Smooth (envelope follower)**

```cpp
auto smooth = vega.IIR({0.1}, {1.0, -0.9});
```

Lowpass on the rectified derivative. Converts rapid spikes into slower contour. The `0.9` feedback creates ~10ms decay.

**Step 4: Threshold**

```cpp
auto transient_logic = vega.Logic(LogicOperator::THRESHOLD, 0.15);
```

When smoothed derivative exceeds 0.15, output is 1.0. Below, output is 0.0. Tune this to your input sensitivity.

**This chain detects _change_, not loudness.** A sustained loud tone won't trigger. A quiet click will.

</details>

---

## Expansion 3: Per-Particle Impulse vs Global Impulse

<details>
<summary>Click to expand: Directional vs Uniform Force Application</summary>

Two approaches to impulse:

**Global impulse (uniform direction):**

```cpp
particles->apply_global_impulse(glm::vec3(0.0f, 5.0f, 0.0f));
```

Every particle gets the same velocity added. Good for waves, directional pushes.

**Per-particle impulse (radial breathing):**

```cpp
for (auto& particle : particles->get_particles()) {
    glm::vec3 pos = particle.point->get_position();
    glm::vec3 outward = glm::normalize(pos) * 3.0f;
    particle.velocity += outward;
}
```

Each particle moves outward from center. Good for expansion/contraction, breathing forms.

**Combine with audio intensity:**

```cpp
logic_proc->on_rising_edge([particles, mic]() {
    double intensity = extract_buffer_rms(mic);
    float scale = static_cast<float>(intensity) * 10.0f;

    for (auto& particle : particles->get_particles()) {
        glm::vec3 pos = particle.point->get_position();
        glm::vec3 outward = glm::normalize(pos) * scale;
        particle.velocity += outward;
    }
});
```

Louder transients → bigger breaths.

</details>

---

## Try It

```cpp
// Spectral splitting: low frequencies → horizontal, high frequencies → vertical
void compose() {
    auto window = MayaFlux::create_window({ "Spectral Spatial", 1920, 1080 });

    // Two independent LFOs at different rates
    auto low_lfo = vega.Sine(0.1); // Slow oscillation
    auto high_lfo = vega.Sine(0.7); // Faster oscillation

    // Normalize to 0..1
    auto low_norm = vega.Polynomial([](double x) { return (x + 1.0) * 0.5; }) | Audio;
    auto high_norm = vega.Polynomial([](double x) { return (x + 1.0) * 0.5; }) | Audio;
    low_norm->set_input_node(low_lfo);
    high_norm->set_input_node(high_lfo);

    low_norm->enable_mock_process(true);
    high_norm->enable_mock_process(true);

    // Audio output: layered tones
    auto bass = vega.Sine(55.0) | Audio;
    auto treble = vega.Sine(880.0) | Audio;
    auto bass_amp = bass * low_norm;
    auto treble_amp = treble * high_norm;
    auto mix = bass_amp + treble_amp;
    mix * 0.3;

    auto particles = vega.ParticleNetwork(
                         600,
                         glm::vec3(-2.0f, -1.5f, -0.5f),
                         glm::vec3(2.0f, 1.5f, 0.5f),
                         ParticleNetwork::InitializationMode::GRID)
        | Graphics;

    particles->set_gravity(glm::vec3(0.0f, 0.0f, 0.0f));
    particles->set_drag(0.08f);
    particles->set_bounds_mode(ParticleNetwork::BoundsMode::BOUNCE);
    particles->set_topology(NodeNetwork::Topology::GRID_2D);

    particles->map_parameter("turbulence", low_norm, NodeNetwork::MappingMode::BROADCAST);

    particles->map_parameter("drag", high_norm, NodeNetwork::MappingMode::BROADCAST);

    auto buffer = vega.NetworkGeometryBuffer(particles) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();
}
```

Bass makes the form sway side to side. Treble makes it bounce up and down. Spectral content becomes spatial dimension.

---

# **Tutorial: Topology and Emergent Form**

```cpp
void compose() {
    auto window = MayaFlux::create_window({ "Emergent", 1920, 1080 });

    // Control signal: slow triangle wave
    auto control = vega.Phasor(0.15) | Audio;  // 0..1 ramp, ~7 second cycle
    auto shaped = vega.Polynomial([](double x) {
        return x < 0.5 ? x * 2.0 : 2.0 - x * 2.0;  // Triangle
    }) | Audio;
    control >> shaped;

    // Audio: resonant ping modulated by control
    auto resonator = vega.Sine(330.0) | Audio;
    auto env = vega.Polynomial([](double x) {
        return std::exp(-x * 5.0);
    }) | Audio;
    shaped >> env;
    auto audio_out = resonator * env | Audio;
    audio_out * 0.3;

    // Particles with spatial interaction
    auto particles = vega.ParticleNetwork(
        400,
        glm::vec3(-1.5f, -1.5f, -0.5f),
        glm::vec3(1.5f, 1.5f, 0.5f),
        ParticleNetwork::InitializationMode::GRID
    ) | Graphics;

    particles->set_topology(NodeNetwork::Topology::SPATIAL);
    particles->set_interaction_radius(0.5f);
    particles->set_spring_stiffness(0.2f);
    particles->set_repulsion_strength(1.0f);
    particles->set_gravity(glm::vec3(0.0f, 0.0f, 0.0f));
    particles->set_drag(0.03f);
    particles->set_bounds_mode(ParticleNetwork::BoundsMode::BOUNCE);

    // Control signal → spring stiffness: rising = rigidifying, falling = softening
    particles->map_parameter("spring_stiffness", shaped, NodeNetwork::MappingMode::BROADCAST);

    auto buffer = vega.NetworkGeometryBuffer(particles) | Graphics;
    buffer->setup_rendering({ .target_window = window });

    window->show();

    // Periodic disturbance to reveal stiffness changes
    MayaFlux::schedule_metro(0.5, [particles]() {
        particles->apply_global_impulse(glm::vec3(
            MayaFlux::get_uniform_random(-0.5f, 0.5f),
            MayaFlux::get_uniform_random(-0.5f, 0.5f),
            MayaFlux::get_uniform_random(-0.2f, 0.2f)
        ));
    });
}
```

Run this. The grid receives periodic disturbances. When you're silent, particles flow fluidly, springs are weak. When you speak, the structure rigidifies, springs tighten. Sound becomes material property.

---

## Expansion 1: Topology Types

<details>
<summary>Click to expand: How Particles Relate</summary>

`set_topology()` determines which particles interact:

| Topology      | Behavior                         | Use Case                     |
| ------------- | -------------------------------- | ---------------------------- |
| `INDEPENDENT` | No inter-particle forces         | Particle storms, rain        |
| `SPATIAL`     | Neighbors within radius interact | Organic clustering, flocking |
| `RING`        | Each connects to prev/next       | Chains, ropes, tentacles     |
| `GRID_2D`     | 2D lattice connections           | Cloth, membranes             |

**SPATIAL** is the most general. Particles find neighbors within `interaction_radius`. Springs pull distant neighbors closer. Repulsion pushes overlapping particles apart.

**The metaphor:** Sound doesn't just _push_ particles. It changes _how they relate to each other_. The form itself responds to audio, not just its position.

</details>

---

## Expansion 2: Material Properties as Audio Targets

<details>
<summary>Click to expand: Beyond Position: Mapping to Structure</summary>

Most audio-reactive systems map amplitude → position/size/color. That's valid but limited. MayaFlux lets you map to _material properties_:

| Property             | Effect                               | Metaphor                               |
| -------------------- | ------------------------------------ | -------------------------------------- |
| `spring_stiffness`   | How rigid connections are            | Sound as material phase (liquid↔solid) |
| `interaction_radius` | How far influence extends            | Sound as social distance               |
| `repulsion_strength` | How strongly particles avoid overlap | Sound as personal space                |
| `drag`               | How quickly motion decays            | Sound as viscosity                     |

**Example: Sound as temperature**

High temperature = particles move freely, weak bonds. Low temperature = rigid structure.

```cpp
// Invert audio: silence = cold/rigid, loud = hot/fluid
auto inverter = vega.Polynomial([](double x) { return 1.0 - std::clamp(x * 2.0, 0.0, 1.0); });
MayaFlux::create_processor<PolynomialProcessor>(mic, inverter);

particles->map_buffer_parameter("spring_stiffness", mic, MappingMode::BROADCAST);
particles->map_buffer_parameter("drag", mic, MappingMode::BROADCAST);
```

Now silence freezes the form. Sound melts it.

</details>

---

## What You've Learned

**NodeNetworks:** Collections of nodes with relationships (physics, topology)  
**ParticleNetwork:** N-body simulation with configurable interaction  
**NetworkGeometryBuffer:** Aggregates particle positions → single GPU upload  
**Buffer-to-parameter mapping:** Audio buffers control physics properties  
**Logic processor callbacks:** Edge detection for discrete visual events  
**Topology:** How particles relate—independent, spatial, ring, grid  
**Material property mapping:** Audio controls structure, not just position

**Pattern:**

```cpp
// 1. Create audio analysis buffer
auto mic = MayaFlux::create_input_listener_buffer(0, false);
// Add processors: envelope, filter, logic, etc.

// 2. Create particle network
auto particles = std::make_shared<ParticleNetwork>(count, bounds_min, bounds_max);
particles->set_topology(/* ... */);

// 3. Map audio to physics parameters
particles->map_buffer_parameter("spring_stiffness", mic, MappingMode::BROADCAST);

// 4. Setup rendering
auto buffer = std::make_shared<NetworkGeometryBuffer>(particles);
buffer->setup_rendering({ .target_window = window });
MayaFlux::register_node_network(particles, ProcessingToken::GRAPHICS_BACKEND);
```

---

# Conclusion

## The Deeper Point

You've crossed the boundary that most frameworks enforce: audio and visuals as separate pipelines, separate mental models, separate codebases.

In MayaFlux, there is no boundary. A node that shapes amplitude can shape gravity. A logic gate that triggers a click can trigger a breath. The same polynomial that distorts audio can warp spatial relationships.

This isn't a "feature." It's the consequence of treating all creative data as what it actually is: numbers flowing through transformations.

The particle systems you've built here demonstrate the principle cross-domain data flow, but they're still working with pre-built physics simulations. The GPU does what `ParticleNetwork` tells it to do.

---

## What Comes Next

**Visual Materiality Part II** moves deeper into the GPU itself.

Instead of mapping audio to physics parameters, you'll bind nodes directly to shader programs. `NodeBindingsProcessor` writes node outputs to push constants, i.e small, fast values updated every frame. `DescriptorBindingsProcessor` writes larger data (vectors, matrices, spectra) to UBOs and SSBOs.

You'll learn:

- **Compute shaders**: Massively parallel data transformation on GPU
- **Push constant bindings**: Node values injected directly into shader execution
- **Descriptor bindings**: Spectrum data, matrices, structured arrays flowing to GPU
- **Custom vertex transformations**: Audio-driven geometry deformation
- **Fragment manipulation**: Color, texture, and pixel-level audio response

The architecture you've learned: nodes, buffers, processors, tokens; remains identical. But instead of `map_parameter("gravity", envelope)`, you'll write:

```cpp
auto processor = std::make_shared<NodeBindingsProcessor>("displacement.comp");
processor->bind_node("amplitude", envelope, offsetof(PushConstants, amplitude));
```

And inside your GLSL:

```glsl
layout(push_constant) uniform PushConstants {
    float amplitude;
};

void main() {
    vec3 displaced = position + normal * amplitude * 0.5;
    // ...
}
```

Same `get_last_output()`. Same data flow. But now you control every vertex, every fragment, every compute thread.

**The substrate doesn't change. Your access to it deepens.**
