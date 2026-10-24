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

**FormaBuffer** is a CPU-writable GPU vertex buffer, registered with the
graphics backend and rendered each frame.

**FormaTarget** directs an element's output to a whole `FormaBuffer` or one
slice of a buffer shared by several elements. `Element::buffer` is a
`FormaTarget`; a `shared_ptr<FormaBuffer>` converts to a whole-buffer target.
Geometry, texture binding, visibility, draw order, and retirement go through
the target.

**Slice** holds one element's geometry within a shared buffer. Each slice has
its own input topology. The slices draw together as one batch.

**Mapped\<T\>** is an element with a typed state. It combines a `MappedState<T>`
(a graphics-thread value with a version counter), a geometry function that
converts `T` to vertex bytes, and its `FormaTarget`. Calling `sync()` reruns
the geometry function if the version has advanced.

**Bridge** wires `Mapped<T>` elements to the rest of the graph, in both
directions.

**Tend\<T\>** chains a `Form<T>`'s construction into a `Bridge` binding in one
fluent statement, the same `place()` pattern `Collapsible`, `Scrollable`,
`TextField`, `Dropdown`, and `ColorPicker` already follow with their own construction.

---

## Creating a surface

```cpp
auto window = MayaFlux::create_window({ "My Surface", 1280, 720 });
window->show();

auto surface = Portal::Forma::create_surface(
    Portal::Forma::SurfaceConfig { .window = window, .name = "my_surface" });
```

`create_surface` builds the `Layer` and `Context` internally and wires them
to the global event manager. The name must be unique across live contexts.

`surface.layer()`, `surface.ctx()`, and `surface.window()` are never hidden.
Anything that works against a bare `Layer` or `Context` works against these.

---

## Buffers, slices and batches

`Forma::create_buffer(RenderConfig)` registers a render-ready buffer. Build
the config with `Surface::render_config`, which sets the window and preserves
other fields you pass:

```cpp
auto buf = Portal::Forma::create_buffer(surface.render_config());
```

The buffer triangulates slices into a `TRIANGLE_LIST`. Capacity grows on flush
from a fixed initial size. Each slice specifies its input topology; line and
point slices are milled into NDC ribbons using widths measured in pixels
against the window height at buffer creation.

### Drawing into a slice

```cpp
auto target = Portal::Forma::FormaTarget::slice_of(
    buf, Portal::Graphics::PrimitiveTopology::POINT_LIST);

target.submit(Kakshya::PointVertex {
    .position = { 0.0F, 0.0F, 0.0F },
    .color = { 1.0F, 1.0F, 1.0F },
    .size = 16.0F,
});

const uint32_t id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("center_point")
        .with_bounds(Kinesis::AABB2D::from_ndc({ 0.0F, 0.0F }, { 0.05F, 0.05F }))
        .with_circle({ 0.0F, 0.0F }, 0.05F)
        .with_buffer(target));
```

`slice_of` returns an empty handle if the buffer is not ready for slices.
Several elements can use slices of the same buffer as one batch. Retiring the
last slice retires that buffer.

### Texture slots

With `forma_multi.frag`, a positive vertex `weight` selects an image slot,
zero draws flat colour, and a negative weight selects glyph coverage from a
slot. Custom shaders can use different semantics; the plot shader does.
Allocate an `additional_textures` slot only when the batch needs one. See
`Portal-Text.md` for glyph coverage.

### Elements with their own buffer

`create_element` and `create(Surface&, Form, ...)` create one buffer for their
element, with its geometry in a slice:

```cpp
auto el = Portal::Forma::create_element<float>(surface, geom_fn, 0.5F);
```

Pass a `FormaTarget` to the explicit overload to share a buffer with others.

---

## Kinesis::Geometry2D : static vertex data

`Kinesis::Geometry2D.hpp` produces raw vertex arrays in NDC (z = 0). Submit
them to a `FormaTarget::slice_of(buf, topology)`, or return them from a
geometry function. Match the slice topology to the returned vertices:
`TRIANGLE_STRIP` for four-vertex filled rectangles, `TRIANGLE_LIST` for other
filled shapes, and `LINE_LIST` for outlines.

### Filled shapes : `Kakshya::Vertex`

| Function                                                  | Description                                                     |
| --------------------------------------------------------- | --------------------------------------------------------------- |
| `filled_rect(region, color)`                              | 4-vertex TRIANGLE_STRIP rect. Also in `GeometryPrimitives.hpp`. |
| `filled_rect_gradient(region, bl, br, tl, tr)`            | Same shape, per-corner colors.                                  |
| `filled_rounded_rect(region, corner_r, segments, color)`  | Rounded rect decomposed into body rects + corner fans.          |
| `filled_circle(center, radius, segments, color)`          | Triangle fan.                                                   |
| `filled_ring(center, inner_r, outer_r, segments, color)`  | Annulus as quad strip.                                          |
| `filled_polygon(center, radius, sides, rotation, color)`  | Regular n-gon fan.                                              |
| `filled_arc(center, radius, start, end, segments, color)` | Pie-slice sector fan.                                           |

### Outlines : `Kakshya::LineVertex`, LINE_LIST

| Function                                                              | Description                           |
| --------------------------------------------------------------------- | ------------------------------------- |
| `rect_outline(region, color, thickness)`                              | 4 edges, 8 vertices.                  |
| `circle_outline(center, radius, segments, color, thickness)`          | Closed loop.                          |
| `arc_outline(center, radius, start, end, segments, color, thickness)` | Open arc.                             |
| `polygon_outline(center, radius, sides, rotation, color, thickness)`  | Closed n-gon.                         |
| `polyline(pts, color, thickness)`                                     | Open path from a span of `glm::vec2`. |
| `polyline_colored(pts, colors, thickness)`                            | Same, per-vertex colors.              |

### Path sampling

`arc_path(center, rx, ry, start, end, segments)` returns `vector<glm::vec2>`,
useful as input to `polyline` or `stroke_slider`:

```cpp
// Semicircular arc path for a stroke_slider
auto path = Kinesis::arc_path({ 0.F, 0.F }, 0.4F, 0.4F,
    glm::radians(200.F), glm::radians(-20.F), 64);

auto handle_buf = Portal::Forma::FormaTarget::slice_of(
    Portal::Forma::create_buffer(surface.render_config()),
    Portal::Graphics::PrimitiveTopology::POINT_LIST);

auto el = Portal::Forma::create(
    surface, Portal::Forma::Geometry::stroke_slider(path, handle_buf), 0.5F);
```

---

## Form : geometry with everything its realization needs

A geometry function alone is not enough to construct an element. Its topology
is fixed by the vertex kind it writes, and its interaction by the forward
placement it performs. The geometry factory already knows both.

`Form<T>` carries all four together:

```cpp
template <typename T>
struct Form {
    GeometryFn<T> geometry;
    Portal::Graphics::PrimitiveTopology topology;
    size_t capacity;
    std::function<void(Context&, uint32_t, std::shared_ptr<MappedState<T>>)> wire;
};
```

`Portal::Forma::create` realizes one: builds a growing buffer, adds a slice
with the Form's topology, registers the element, syncs so `bounds_hint` and
`contains` from the geometry function reach the Layer before the first frame,
then runs the wiring. The legacy `Form::capacity` field does not size this
buffer.

```cpp
constexpr Kinesis::AABB2D track { glm::vec2(-0.8F, -0.08F), glm::vec2(0.8F, 0.08F) };

auto el = Portal::Forma::create(
    surface, Portal::Forma::Geometry::horizontal_fader(track, 0.04F), 0.5F);

auto constant = vega.Constant(0.0) | Audio[0];
Portal::Forma::bridge().at(el.state).write(constant);
```

That is the whole fader: geometry, correct topology, and a drag whose inverse
matches the handle placement.

`Tend<T>`, introduced after Bridge below, chains this same construction
directly into a binding for the common case where both happen together.

Nothing enumerates the set of Forms. Any function returning one participates
identically, including one you write. A bare `GeometryFn<T>` converts to a
`Form<T>` with default topology, default capacity, and no interaction, so
custom geometry passes to `create` unchanged.

Every field is public and overridable before `create`. To keep the geometry
but supply your own interaction:

```cpp
auto form = Portal::Forma::Geometry::horizontal_fader(track, 0.04F);
form.wire = {};

auto el = Portal::Forma::create(surface, std::move(form), 0.5F);

surface.ctx().on_drag(el.element.id, IO::MouseButtons::Left,
    [state = el.state](uint32_t, glm::vec2 ndc) { /* your mapping */ });
```

`Form::capacity` remains in the type for compatibility. The current buffer
starts at a fixed size and grows on flush when its slices need more space.

---

## Geometry functions

`Portal::Forma::Geometry` provides ready-made Forms. Each closes over its
configuration parameters and carries the interaction inverse to the placement
it performs.

### Controls

| Function                                    | T               | Carries                                                                                                                                  |
| ------------------------------------------- | --------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `horizontal_fader(bounds, handle_w)`        | `float [0,1]`   | Drag mapping cursor x to handle position, offset by half a handle so the handle centre tracks the cursor. Hit region follows the handle. |
| `vertical_fader(bounds, handle_h)`          | `float [0,1]`   | Same, y axis.                                                                                                                            |
| `stroke_slider(path, handle_buf, ...)`      | `float [0,1]`   | Drag mapping cursor to nearest arc-length position on the path. Requires a separate POINT_LIST handle buffer.                            |
| `toggle(region, color_off, color_on)`       | `bool`          | Press flipping the value.                                                                                                                |
| `drawable_canvas(bounds, color, thickness)` | `vector<float>` | Drag painting sample values, interpolating across indices under fast drag.                                                               |

### Readouts

| Function                                       | T             | Carries                                                                                                         |
| ---------------------------------------------- | ------------- | --------------------------------------------------------------------------------------------------------------- |
| `level_meter(bounds, horizontal, fill, track)` | `float [0,1]` | Nothing. Driven from a node or a Bridge write.                                                                  |
| `radial(region, start, end, color)`            | `float [0,1]` | Drag mapping cursor angle about the region's centre onto the sweep. Radius is the region's smaller half-extent. |

### Positional

| Function                                           | T                  | Carries                                                         |
| -------------------------------------------------- | ------------------ | --------------------------------------------------------------- |
| `point(color, size, hit_radius)`                   | `glm::vec2` (NDC)  | Hover, writing the cursor position. Hit region is a circle.     |
| `crosshair(arm_len, color, thickness, hit_radius)` | `glm::vec2` (NDC)  | Hover, writing the cursor position.                             |
| `position_picker(bounds, color, size)`             | `glm::vec2 [0,1]²` | Drag mapping cursor to unit-square coordinates over the bounds. |

---

Any factory whose first parameter is an `AABB2D` accepts an anchor instead
through `Geometry::at`:

```cpp
auto fader = Portal::Forma::create(surface, Geometry::horizontal_fader(track, 0.04F), 0.5F);

// a meter occupying the fader's own region
auto meter = Portal::Forma::create(surface, Geometry::at(fader, Geometry::level_meter), 0.F);
```

`at` works for any factory, including yours. Anything exposing `bounds()`
serves as the anchor: `Mapped<T>`, `Collapsible`, `Scrollable`, `Entry`, `EntryGroup`.

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
surface.ctx().on_press  (id, IO::MouseButtons::Left,  [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_release(id, IO::MouseButtons::Left,  [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_drag   (id, IO::MouseButtons::Left,  [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_enter  (id, [](uint32_t id) { });
surface.ctx().on_leave  (id, [](uint32_t id) { });
surface.ctx().on_move   (id, [](uint32_t id, glm::vec2 ndc) { });
surface.ctx().on_scroll (id, [](uint32_t id, glm::vec2 ndc, double dx, double dy) { });
```

`ndc` is the cursor position in NDC space at the time of the event. Callbacks run on the graphics thread.

`on_drag` is the preferred callback for continuous gestures (sliders, faders, canvas drawing). It tracks the originating element even when the cursor leaves its bounds, and fires on every motion event while the button is held. It does not require a separate press flag. Use `on_press`/`on_release` only when you need to detect the transition itself.

```cpp
// Fader using on_drag, which needs no pressed flag
surface.ctx().on_drag(el.element.id, IO::MouseButtons::Left,
    [state = el.state, track](uint32_t, glm::vec2 ndc) {
        state->write(std::clamp(
            (ndc.x - track.min.x) / track.width(), 0.F, 1.F));
    });
```

---

## Keyboard focus and key events

Keyboard focus transfers to an element on mouse press. Key events route only to the focused element.

```cpp
// Key pressed once (fires on initial press, not on repeat)
surface.ctx().on_press(id, IO::Keys::Enter, [](uint32_t id) { });

// Key released
surface.ctx().on_release(id, IO::Keys::Escape, [](uint32_t id) { });

// Key held, fires on initial press and on each OS repeat tick
// Useful for continuous adjustment (arrow key nudge, value increment)
surface.ctx().on_held(id, IO::Keys::ArrowUp, [](uint32_t id) { });

// Focus lifecycle
surface.ctx().on_focus_gained(id, [](uint32_t id) { /* highlight */ });
surface.ctx().on_focus_lost  (id, [](uint32_t id) { /* unhighlight */ });

// Query and clear focus
auto focused_id = surface.ctx().focused(); // std::optional<uint32_t>
surface.ctx().clear_focus();
```

Focus transfers automatically on mouse press. `unbind(id)` clears focus if that element holds it. Multiple key codes can be registered on the same element; each is a separate `on_press`/`on_held`/`on_release` call.

Arrow-key nudge on a fader:

```cpp
surface.ctx().on_held(el.element.id, IO::Keys::ArrowRight,
    [state = el.state](uint32_t) {
        state->write(std::clamp(state->value + 0.01F, 0.F, 1.F));
    });
surface.ctx().on_held(el.element.id, IO::Keys::ArrowLeft,
    [state = el.state](uint32_t) {
        state->write(std::clamp(state->value - 0.01F, 0.F, 1.F));
    });
```

---

## Text and textures on an Element

`with_text`, `set_text`, and `retarget` draw glyph quads into the element's
`FormaTarget`. Text slices in one buffer share a glyph atlas image at texture
slot 0. Set up that slot in `RenderConfig` before creating the buffer:

```cpp
constexpr Kinesis::AABB2D box { glm::vec2(-0.3F, -0.1F), glm::vec2(0.3F, 0.1F) };

auto render = surface.render_config();
render.additional_textures.emplace_back("text", nullptr);
auto text_buf = Portal::Forma::create_buffer(std::move(render));
auto target = Portal::Forma::FormaTarget::slice_of(
    text_buf, Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST);

auto el = Portal::Forma::Element {}
    .non_interactive()
    .with_buffer(target)
    .with_text("Click me",
        Portal::Text::PressParams { .color = { 1.F, 1.F, 1.F, 1.F },
                                    .render_bounds = { 256, 48 } },
        box);

surface.layer().add(el);
el.set_text("Active", Portal::Text::PressParams { .render_bounds = { 256, 48 } });
```

Forma keeps one `AtlasImage` per atlas and refreshes stale text slices when
that atlas grows. A buffer's texture bindings belong to the whole buffer, so
its text slices must use the same atlas and reserve slot 0 for it. A row
shorter than one line squeezes its text to fit (`TextRun::extent`).

`with_texture` still draws a loaded or render-target image. Give its buffer
an `additional_textures` slot and use a separate slice for each image quad.
Several slices of one buffer can share an image bound at the same slot:

```cpp
auto image_render = surface.render_config();
image_render.additional_textures.emplace_back("image", nullptr);
auto image_buf = Portal::Forma::create_buffer(std::move(image_render));
auto image_target = Portal::Forma::FormaTarget::slice_of(image_buf);
surface.layer().add(
    Portal::Forma::Element {}
        .non_interactive()
        .with_buffer(image_target)
        .with_texture(some_vkimage, box));
```

`Element::text_slice` retains the glyph-quad path. See `Portal-Text.md` for
`TextRun`, `GlyphMap`, and `AtlasImage`.

---

## Mapped\<T\> : typed dynamic geometry

When geometry must update in response to a changing value, write a geometry function and pass it to create.

```cpp
auto el = Portal::Forma::create<float>(
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
    0.0F);
```

The geometry function receives the current value, a byte buffer to write into, and a mutable `Element&`.
Writing `elem.bounds_hint` or `elem.contains` inside the geometry function updates the spatial description as the shape changes.
A raw geometry function converts to a `Form<T>` with default topology and capacity and no interaction,
so pass topology explicitly by constructing the Form yourself when the vertex kind is not `TRIANGLE_STRIP`:

```cpp
Portal::Forma::Geometry::Form<float> form {
    std::move(my_geometry_fn),
    Portal::Graphics::PrimitiveTopology::LINE_LIST,
    4 * sizeof(Kakshya::LineVertex),
};
auto el = Portal::Forma::create(surface, std::move(form), 0.F);
```

`el.state` is the `shared_ptr<MappedState<T>>`. Write a new value from the
graphics thread via `el.state->write(v)`; every Context callback and Bridge
task already runs there. From any other thread, hand the value over through
`Bridge::bind` with a callable. The geometry function reruns on the next
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

**`relate_to`**: visibility, z-order, and removal on the parent cascade to
this element via `Layer::closure`. Used to group labels, backgrounds, or
body panels under a controlling element.

```cpp
const uint32_t geo_id = surface.layer().add(
    Portal::Forma::Element {}
        .with_name("knob")
        .with_bounds(region)
        .with_buffer(geo_buf))
    .relate_to(bg_id)
    .id();
```

Hiding `bg_id` now cascades to `geo_id`. To remove the whole group, call
`Portal::Forma::destroy(surface, bg_id)` rather than `surface.layer().remove(bg_id)`
directly: `destroy` walks the same closure but also unbinds each id's
`Context`/`Bridge` state first, so a torn-down element can't be left as a
stale hover/drag target or a dangling inbound/outbound binding. `Layer::remove`
alone only clears the `Layer` side.

---

## Bridge : two-way binding

`Portal::Forma::bridge()` returns the application-wide `Bridge`. It connects
`Mapped<T>` elements to the node graph in both directions.

Bindings use an `ElementRef`, a weak pointer to the owning `Layer` and an
element id. Ids are
unique within one layer, so identical ids in different layers remain distinct.
`bridge().at(...)` accepts either an `ElementRef` or a `MappedState` that
already carries its layer. The layer must have a `shared_ptr` owner.

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

// Route to an AudioWriteProcessor (scalar state: single-sample span)
Portal::Forma::bridge().at(el.state).write(audio_write_processor);

// Route vector<float> state to any bulk float consumer
Portal::Forma::bridge().at(el.state).write(
    [](std::span<const float> s) {
        // s.data() and s.size() give the full vector for vector<float> state,
        // single element for scalar state
    });
```

The `write(span sink)` overload is the general bulk outbound path. For `MappedState<vector<float>>`, `MappedState<vector<double>>`, or a `MappedState` of `glm::vec2`, `glm::vec3` or `glm::vec4`, the full value is forwarded each frame. For scalar state a single-element span is constructed. The conversion from `float` to whatever the consumer needs is the caller's responsibility.

The push-constant `write(target, shader_path, offset)` overload writes a
`float` by default. Pass a `Kakshya::GpuDataFormat` as the fourth argument when
the GPU slot has another format. If the `MappedState<T>` value has that exact
GPU format, Bridge writes the full value, including vector components;
otherwise it converts the projected scalar to a single-component format.

### Chaining both directions

`bridge().at(...)` returns a `Binding` handle. Every method returns `Binding&`
for chaining:

```cpp
Portal::Forma::bridge().at(el.state)
    .bind(envelope_node, [](double x) { return static_cast<float>(x); })
    .write(constant_node)
    .write(compute_buf, "effect.frag.spv", offsetof(PC, cutoff));
```

### Tend\<T\> : place and bridge in one chain

`create` and `bridge().at(...)` are two separate calls because they are two
separate concerns: realizing a `Form<T>` on a surface, and wiring the result
into the graph. `Tend<T>` is the everyday path when both happen together. It
follows the same `place()` pattern `Collapsible`, `Scrollable`, and
`TextField` already use for their own construction:

```cpp
auto constant = vega.Constant(0.0) | Audio[0];

Portal::Forma::Tend<float> {
    .form = Portal::Forma::Geometry::horizontal_fader(
        { glm::vec2(-0.8F, -0.08F), glm::vec2(0.8F, 0.08F) }, 0.04F),
    .initial = 0.5F,
}.place(surface).bridge().write(constant);
```

`place(surface)` calls `Portal::Forma::create` exactly as the fader example
above does, and keeps a copy of `surface` so `result`, `surface`, `bridge()`,
and `track()` all stay reachable afterward. `bridge()` returns the same
`Binding` that `bridge().at(...)` does, so every `bind`/`write` overload
above chains directly off it.

---

## Layout

`Kinesis::place` computes a region positioned against an anchor.

```cpp
constexpr Kinesis::AABB2D fader { { -0.8F, -0.05F }, { 0.8F, 0.05F } };

const auto label = Kinesis::place(fader, Kinesis::Side::Below, 0.F, 0.05F,
    0.01F, Kinesis::Align::Span);

const auto readout = Kinesis::place(fader, Kinesis::Side::Right, 0.2F, 0.1F, 0.02F);
```

`Side` is `Right`, `Left`, `Above`, `Below`, or `Over`. `Align` controls the
cross axis: `Center`, `Start`, `End`, or `Span`, where `Span` matches the
anchor's full cross-axis extent and ignores the corresponding size argument.

The anchor may be any object exposing `bounds()`, so a placed element anchors
the next one directly:

```cpp
auto fader = Portal::Forma::create(surface, Geometry::horizontal_fader(track, 0.04F), 0.5F);
const auto below = Kinesis::place(fader, Kinesis::Side::Below, 0.F, 0.05F, 0.01F, Kinesis::Align::Span);
```

`LayoutCursor` advances a Y baseline and can be scoped to a column:

```cpp
Portal::Forma::LayoutCursor left  { 1.F, -0.95F, -0.05F };
Portal::Forma::LayoutCursor right { 1.F,  0.05F,  0.95F };

const auto a = left.advance(0.08F);
const auto b = right.advance(0.08F);

left.sync_to(right);   // both resume from the lower of the two baselines
```

`Surface` produces named screen regions without NDC arithmetic:

```cpp
const auto corner = surface.top_right(0.28F, 0.13F);
const auto status = surface.bottom_strip(0.04F);
const auto panel  = surface.top_left(0.35F, 0.8F);

Portal::Forma::LayoutCursor cursor { panel.max.y, panel.min.x, panel.max.x };
```

`top_left`, `top_right`, `bottom_left`, `bottom_right`, `top_strip`,
`bottom_strip`, `left_strip`, `right_strip`, `center_rect`, and `full`.

`Scrollable` scissors its buffer to the viewport. Give the viewport and its
tracked children slices of one buffer to clip them as one batch; keep content
that must draw outside the viewport on another buffer.

## Quick examples

### Mouse follower

The Form carries hover-follow, so the point tracks the cursor with no callback registration.

```cpp
auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::point({ 0.2F, 0.8F, 1.0F }, 14.0F, 0.04F),
    glm::vec2 { 0.F, 0.F });
```

### Radial indicator

The Form carries angular drag, so dragging anywhere sweeps the indicator to the cursor's angle.

```cpp
auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::radial(
        Kinesis::AABB2D::from_ndc({ 0.F, 0.F }, glm::vec2(0.3F)),
        glm::radians(225.F), glm::radians(-45.F),
        { 1.F, 0.8F, 0.2F }),
    0.5F);
```

### Horizontal fader

```cpp
constexpr Kinesis::AABB2D track { glm::vec2(-0.6F, -0.05F), glm::vec2(0.6F, 0.05F) };

auto el = Portal::Forma::create(
    surface, Portal::Forma::Geometry::horizontal_fader(track, 0.04F), 0.5F);
```

### 2D position picker

```cpp
constexpr Kinesis::AABB2D area { glm::vec2(-0.5F, -0.5F), glm::vec2(0.5F, 0.5F) };

auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::position_picker(area, { 0.9F, 0.4F, 0.1F }, 10.F),
    glm::vec2 { 0.5F, 0.5F });
```

### Labeled interactive button

`PressParams::background` draws a flat quad beneath the glyphs in the same
text slice. Changing the label's parameters updates that slice:

```cpp
constexpr Kinesis::AABB2D box { glm::vec2(-0.3F, -0.1F), glm::vec2(0.3F, 0.1F) };
constexpr glm::vec4 k_rest  { 0.2F, 0.2F, 0.2F, 1.F };
constexpr glm::vec4 k_hover { 0.4F, 0.4F, 0.4F, 1.F };
constexpr glm::vec4 k_press { 0.8F, 0.3F, 0.2F, 1.F };

auto render = surface.render_config();
render.additional_textures.emplace_back("text", nullptr);
auto text_buf = Portal::Forma::create_buffer(std::move(render));
auto target = Portal::Forma::FormaTarget::slice_of(
    text_buf, Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST);

auto el = Portal::Forma::Element {}
    .with_bounds(box)
    .with_buffer(target)
    .with_text("Click me",
        Portal::Text::PressParams { .color = { 1.F, 1.F, 1.F, 1.F }, .background = k_rest, .render_bounds = { 256, 48 } },
        box);

const uint32_t id = surface.layer().add(el).id();

auto paint = [el](glm::vec4 bg) mutable {
    el.set_text("Click me", Portal::Text::PressParams {
        .color = { 1.F, 1.F, 1.F, 1.F }, .background = bg,
        .render_bounds = { 256, 48 } });
};

surface.ctx().on_press  (id, IO::MouseButtons::Left, [paint](uint32_t, glm::vec2) mutable { paint(k_press); });
surface.ctx().on_release(id, IO::MouseButtons::Left, [paint](uint32_t, glm::vec2) mutable { paint(k_rest); });
surface.ctx().on_enter  (id, [paint](uint32_t) mutable { paint(k_hover); });
surface.ctx().on_leave  (id, [paint](uint32_t) mutable { paint(k_rest); });
```

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
