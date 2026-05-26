# Portal::Forma

Forma is MayaFlux's immediate-mode spatial layer. Screen regions and pointer
events are first-class participants in the data graph, not a separate UI
toolkit. There is no layout engine, no widget hierarchy, no panel system.

---

## Concepts

**Surface** is the canvas. It owns a `Window`, a `Layer`, and a `Context`.
Most creation functions accept a `Surface&` or return one.

**Layer** is a flat registry of `Element`s in NDC space. It owns bounds and
containment predicates for each element and drives hit testing.

**Context** is the event router. It maps pointer events to element ids via the
`Layer` and dispatches to registered callbacks.

**Element** is a bounded, renderable region. Its spatial description is
deliberately open: an AABB fast-reject, a precise containment predicate, or
both. An element with no buffer is a valid pure hit-test region.

**FormaBuffer** is a CPU-writable GPU vertex buffer. You call `submit()` to
push geometry. The buffer is registered with the graphics backend and rendered
each frame.

**Mapped\<T\>** is an element with a typed state. It combines a `MappedState<T>`
(an atomic value with a version counter), a geometry function that converts `T`
to vertex bytes, and the underlying `FormaBuffer`. Calling `sync()` reruns the
geometry function if the version has advanced.

**Bridge** wires `Mapped<T>` elements to the rest of the graph, in both
directions.

---

## Creating a surface

```cpp
auto window = MayaFlux::create_window({ "My Surface", 1280, 720 });
window->show();

auto surface = Portal::Forma::create_surface(window, "my_surface");
```

`create_surface` builds the `Layer` and `Context` internally and wires them
to the global event manager. The name must be unique across live contexts.

`surface.layer()`, `surface.ctx()`, and `surface.window()` are never hidden.
Anything that works against a bare `Layer` or `Context` works against these.

---

## Static geometry, manual buffer

Use this when geometry does not change and requires no interaction state.

```cpp
auto buf = Portal::Forma::create_buffer(
    window,
    Kakshya::PointVertex {
        .position = { 0.0F, 0.0F, 0.0F },
        .color    = { 1.0F, 1.0F, 1.0F },
        .size     = 16.0F,
    },
    Portal::Graphics::PrimitiveTopology::POINT_LIST);

const uint32_t id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("center_point")
        .with_bounds(Kinesis::AABB2D::from_ndc({ 0.0F, 0.0F }, { 0.05F, 0.05F }))
        .with_circle({ 0.0F, 0.0F }, 0.05F)
        .with_buffer(buf));
```

`create_buffer` with a vertex value (or range) registers and submits in one
call. The overload that takes a byte count and topology creates an empty
buffer that you fill via `buf->submit(...)` yourself.

---

## Element spatial description

Every `Element` has two independent spatial fields:

- `bounds_hint`: `AABB2D` fast-reject. Evaluated first. For rectangular
  regions this is sufficient on its own.
- `contains`: authoritative `bool(glm::vec2 ndc)` predicate. Used for any
  non-rectangular region.

If both are set, `bounds_hint` is checked first. If only `bounds_hint` is set,
it is the sole test. If neither is set, the element never hits.

```cpp
// Rectangle: bounds_hint only
el.with_bounds(Kinesis::AABB2D { { -0.2F, -0.1F }, { 0.2F, 0.1F } });

// Circle: contains + optional bounds_hint for fast reject
el.with_bounds(Kinesis::AABB2D::from_ndc({ 0.F, 0.F }, { 0.05F, 0.05F }))
  .with_circle({ 0.F, 0.F }, 0.05F);

// Polygon: contains + optional bounds_hint
el.with_bounds(box)
  .with_polygon(std::array<glm::vec2, 4> { {
      box.min, { box.min.x, box.max.y }, box.max, { box.max.x, box.min.y },
  } });

// Stroke (path, wire, waveform trace)
el.with_stroke(points, half_thickness_in_ndc);
```

`Kinesis::circular_bounds`, `polygon_bounds`, `stroke_bounds`, `union_bounds`,
`subtract_bounds`, and `intersect_bounds` all produce `std::function<bool(glm::vec2)>`
suitable for direct assignment to `contains`.

---

## Pointer events

```cpp
surface.ctx().on_press  (id, IO::MouseButtons::Left, [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_release(id, IO::MouseButtons::Left, [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_enter  (id, [](uint32_t id) { });
surface.ctx().on_leave  (id, [](uint32_t id) { });
surface.ctx().on_move   (id, [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_scroll (id, [](uint32_t id, glm::vec2 delta) { });
```

`ndc` is the cursor position in NDC space at the time of the event.
Callbacks run on the graphics thread.

Hover tint pattern:

```cpp
auto color = std::make_shared<glm::vec3>(0.3F, 0.3F, 0.3F);
auto redraw = [buf, color, box] {
    buf->submit(std::array<Kakshya::MeshVertex, 4> { {
        { .position = { box.min.x, box.min.y, 0 }, .color = *color },
        { .position = { box.min.x, box.max.y, 0 }, .color = *color },
        { .position = { box.max.x, box.min.y, 0 }, .color = *color },
        { .position = { box.max.x, box.max.y, 0 }, .color = *color },
    } });
};
redraw();

surface.ctx().on_enter(id, [color, redraw](uint32_t) { *color = { 0.5F, 0.5F, 0.5F }; redraw(); });
surface.ctx().on_leave(id, [color, redraw](uint32_t) { *color = { 0.3F, 0.3F, 0.3F }; redraw(); });
```

---

## Mapped\<T\> : typed dynamic geometry

When geometry must update in response to a changing value, use
`create_element<T>`. The geometry function converts the current `T` into
vertex bytes and is rerun automatically when the value changes.

```cpp
auto el = Portal::Forma::create_element<float>(
    surface,
    [track](float v, std::vector<uint8_t>& out, Portal::Forma::Element& elem) {
        float right = track.min.x + v * track.width();
        Portal::Forma::Geometry::write_verts(out, std::array<Kakshya::LineVertex, 4> { {
            { .position = { track.min.x, track.min.y, 0.F }, .color = { 0.2F, 0.6F, 1.F } },
            { .position = { track.min.x, track.max.y, 0.F }, .color = { 0.2F, 0.6F, 1.F } },
            { .position = { right,        track.min.y, 0.F }, .color = { 0.1F, 0.3F, 0.8F } },
            { .position = { right,        track.max.y, 0.F }, .color = { 0.1F, 0.3F, 0.8F } },
        } });
        elem.bounds_hint = track;
    },
    0.0F,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);
```

The geometry function receives the current value, a byte buffer to write into,
and a mutable `Element&`. Writing `elem.bounds_hint` or `elem.contains` inside
the geometry function updates the spatial description as the shape changes.
The `Surface`-accepting overload of `create_element` runs one `sync()` on
construction so bounds and containment are live before the first frame, without
any manual `set_bounds`/`set_contains` calls.

`el.state` is the `shared_ptr<MappedState<T>>`. Write a new value from any
thread via `el.state->write(v)`. The geometry function reruns on the next
`sync()` call.

---

## Slot : post-registration mutations

`layer().add(element)` returns a `Slot`. It supports chained mutations and
converts implicitly to `uint32_t` for existing call sites.

```cpp
const uint32_t id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("background")
        .non_interactive()
        .with_buffer(bg_buf))
    .to_back()
    .id();
```

Available mutations: `relate_to(parent_id)`, `hidden()`, `non_interactive()`,
`to_front()`, `to_back()`.

**`relate_to`**: visibility, z-order, and remove operations on the parent
cascade to this element. Used to group labels, backgrounds, or body panels
under a controlling element.

```cpp
const uint32_t geo_id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("knob")
        .with_bounds(region)
        .with_buffer(geo_buf))
    .relate_to(bg_id)
    .id();
```

Hiding or removing `bg_id` now cascades to `geo_id`.

---

## Bridge : two-way binding

`Portal::Forma::bridge()` returns the application-wide `Bridge`. It connects
`Mapped<T>` elements to the node graph in both directions.

### Inbound : node or callable drives the element

```cpp
// Drive from a node's output each frame
auto sine = vega.Sine(440) | Audio[0];
Portal::Forma::bridge().at(el.state).bind(sine, [](double x) {
    return std::clamp(static_cast<float>(std::abs(x)), 0.F, 1.F);
});

// Drive from an arbitrary callable
Portal::Forma::bridge().at(el.state).bind([] {
    return some_external_value();
});
```

The `bind` overloads spawn a `GraphicsRoutine` that runs each frame, reads the
source, applies the optional projection, and calls `state->write()`. A new
`bind` call on the same element replaces the previous inbound routine.

### Outbound : element value drives a node or shader

```cpp
// Route to a Constant node (feeds further into the audio graph)
auto constant = vega.Constant(0.0) | Audio[0];
Portal::Forma::bridge().at(el.state).write(constant);

// Route to a push constant slot
Portal::Forma::bridge().at(el.state)
    .write(tex, "brightness.frag.spv", offsetof(Params, brightness));

// Route to a descriptor UBO binding
Portal::Forma::bridge().at(el.state)
    .write(std::static_pointer_cast<Buffers::VKBuffer>(quad),
        "forma_ctl.frag", "intensity", 0, 1,
        Portal::Graphics::DescriptorRole::UNIFORM);
```

### Chaining both directions

`bridge().at(...)` returns a `Binding` handle. Every method returns `Binding&`
for chaining:

```cpp
Portal::Forma::bridge().at(el.state)
    .bind(envelope_node, [](double x) { return static_cast<float>(x); })
    .write(constant_node)
    .write(compute_buf, "effect.frag.spv", offsetof(PC, cutoff));
```

### Draggable fader writing to a node — full example

```cpp
constexpr Kinesis::AABB2D track { glm::vec2(-0.8F, -0.08F), glm::vec2(0.8F, 0.08F) };

auto el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::horizontal_fader(track, 0.04F),
    0.5F,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

auto pressed = make_persistent(false);

surface.ctx().on_press  (el.element.id, IO::MouseButtons::Left, [&pressed](uint32_t, glm::vec2) { pressed = true;  });
surface.ctx().on_release(el.element.id, IO::MouseButtons::Left, [&pressed](uint32_t, glm::vec2) { pressed = false; });
surface.ctx().on_move   (el.element.id,
    [state = el.state, &pressed, track](uint32_t, glm::vec2 ndc) {
        if (!pressed)
            return;
        state->write(std::clamp(
            (ndc.x - track.min.x) / track.width(), 0.F, 1.F));
    });

auto constant = vega.Constant(0.0) | Audio[0];
Portal::Forma::bridge().at(el.state).write(constant);
```

---

## TextBuffer as a Forma background

A `TextBuffer` can be used as a texture inside a `FormaBuffer`. Register the
text buffer separately, then assign its texture to the `FormaBuffer` before
submitting UVs.

```cpp
auto text_buf = Portal::Text::press("hello forma",
    Portal::Text::PressParams {
        .color        = { 1.F, 1.F, 1.F, 1.F },
        .render_bounds = { 512, 128 },
        .budget_h      = 128 });

register_graphics_buffer(text_buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

constexpr Kinesis::AABB2D region {
    glm::vec2(-0.4F, -0.15F), glm::vec2(0.4F, 0.15F)
};

auto bg_buf = Portal::Forma::create_buffer(
    window,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP,
    "texSampler");
bg_buf->set_texture(text_buf->get_texture(), "texSampler");

bg_buf->submit(std::array<Kakshya::MeshVertex, 4> { {
    { .position = { region.min.x, region.min.y, 0.F }, .color = { 1.F, 1.F, 1.F }, .uv = { 0.F, 1.F } },
    { .position = { region.min.x, region.max.y, 0.F }, .color = { 1.F, 1.F, 1.F }, .uv = { 0.F, 0.F } },
    { .position = { region.max.x, region.min.y, 0.F }, .color = { 1.F, 1.F, 1.F }, .uv = { 1.F, 1.F } },
    { .position = { region.max.x, region.max.y, 0.F }, .color = { 1.F, 1.F, 1.F }, .uv = { 1.F, 0.F } },
} });

const uint32_t bg_id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("background")
        .non_interactive()
        .with_buffer(bg_buf))
    .to_back()
    .id();
```

Note the UV Y-flip: Vulkan texture coordinates have `(0,0)` at top-left and
`(1,1)` at bottom-right, while the quad vertices are in NDC (+Y up). The UVs
`{ 0,1 }` at bottom-left and `{ 0,0 }` at top-left correct for this.

---

## Quick examples

### Mouse follower

```cpp
auto el = Portal::Forma::create_element<glm::vec2>(
    surface,
    Portal::Forma::Geometry::point({ 0.2F, 0.8F, 1.0F }, 14.0F, 0.04F),
    glm::vec2 { 0.F, 0.F },
    Portal::Graphics::PrimitiveTopology::POINT_LIST);

surface.ctx().on_move(el.element.id, [state = el.state](uint32_t, glm::vec2 ndc) {
    state->write(ndc);
});
```

`on_move` fires globally when the cursor moves anywhere on the surface.
The geometry function receives the new NDC position and re-emits a
`PointVertex` there. `sync()` is driven automatically by the Bridge.

### Radial indicator

```cpp
// Sweeps from 3π/4 (bottom-left) to π/4 (bottom-right), full rotation
auto el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::radial(
        { 0.F, 0.F }, 0.3F,
        glm::radians(225.F), glm::radians(-45.F),
        { 1.F, 0.8F, 0.2F }),
    0.5F,
    Portal::Graphics::PrimitiveTopology::LINE_LIST);
```

`value` in `[0, 1]` sweeps the indicator line between `angle_start` and
`angle_end`. The hit region is a circle of the given radius. Drive the
value from a node via Bridge or write it directly via `el.state->write(v)`.

### Horizontal fader

```cpp
constexpr Kinesis::AABB2D track { glm::vec2(-0.6F, -0.05F), glm::vec2(0.6F, 0.05F) };

auto el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::horizontal_fader(track, 0.04F),
    0.5F,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);
```

The handle hit region tracks the handle position automatically via the
geometry function. Wire drag interaction as shown in the Bridge section above.

### 2D position picker

```cpp
constexpr Kinesis::AABB2D area { glm::vec2(-0.5F, -0.5F), glm::vec2(0.5F, 0.5F) };

auto el = Portal::Forma::create_element<glm::vec2>(
    surface,
    Portal::Forma::Geometry::position_picker(area, { 0.9F, 0.4F, 0.1F }, 10.F),
    glm::vec2 { 0.5F, 0.5F },
    Portal::Graphics::PrimitiveTopology::POINT_LIST);

auto pressed = make_persistent(false);
surface.ctx().on_press  (el.element.id, IO::MouseButtons::Left, [&pressed](uint32_t, glm::vec2) { pressed = true;  });
surface.ctx().on_release(el.element.id, IO::MouseButtons::Left, [&pressed](uint32_t, glm::vec2) { pressed = false; });
surface.ctx().on_move   (el.element.id, [state = el.state, &pressed, area](uint32_t, glm::vec2 ndc) {
    if (!pressed) return;
    state->write(glm::clamp(
        (ndc - area.min) / glm::vec2(area.width(), area.height()),
        glm::vec2(0.F), glm::vec2(1.F)));
});
```

Value is `glm::vec2` in `[0,1]×[0,1]`, mapped to a pixel position inside
`area`. Route both axes to nodes or push constants via Bridge as needed.

---

## Pure hit-test regions

An element with no buffer participates in spatial queries and event routing
but contributes no rendered output:

```cpp
surface.layer().add(
    Portal::Forma::Element {}
        .with_name("trigger_zone")
        .with_bounds(zone)
        .with_polygon(verts));

surface.ctx().on_press(id, IO::MouseButtons::Left, [](uint32_t, glm::vec2 ndc) {
    // cursor entered the zone
});
```

Valid uses: named audio voice positions, invisible trigger zones, compute
shader dispatch regions, named spatial anchors.
