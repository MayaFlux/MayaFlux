# Portal::Forma - UI Patterns

Forma has no widget hierarchy, no retained tree, no retained state beyond
what you explicitly create. If you are coming from ImGui, that is the core
difference: ImGui owns your layout and state management. Forma does not. You
own layout, Forma owns rendering and hit testing.

This guide maps common ImGui patterns to their Forma equivalents.

---

## Mental model shift

ImGui:
- Call `Begin` / `End` each frame.
- Declare widgets inline. ImGui handles state, layout, and drawing.
- Nothing persists. Everything is re-emitted every frame.

Forma:
- Construct elements once at startup. Register them on a `Layer`.
- Write to `MappedState<T>` to change values. `sync()` redraws geometry.
- Hit testing and event dispatch run continuously via `Context`.
- Layout is your NDC arithmetic, done once at construction time.

The frame loop runs invisibly. You do not call `Begin`, you do not loop over
widgets. You write a value and the geometry function reacts.

---

## Layout

ImGui handles cursor advancement internally. Forma uses `LayoutCursor`.

`LayoutCursor` holds a shared `MappedState<float>` carrying the current NDC Y
baseline. NDC Y runs +1 (top) to -1 (bottom). `advance(height)` subtracts
height and returns the AABB just occupied.

```cpp
LayoutCursor cursor;             // starts at y = 1.0 (top)
cursor.skip(0.02F);              // padding

const auto row0 = cursor.advance(0.06F);  // AABB { -1, 0.92, 1, 1.0 } (approximately)
const auto row1 = cursor.advance(0.06F);
const auto row2 = cursor.advance(0.06F);
```

For columnar layouts, compute x ranges manually:

```cpp
constexpr float x_left  = -0.95F;
constexpr float x_mid   =  0.0F;
constexpr float x_right =  0.95F;

// Two columns side by side at the same row y
auto row = cursor.advance(0.06F);
Kinesis::AABB2D left_cell  { { x_left, row.min.y }, { x_mid,   row.max.y } };
Kinesis::AABB2D right_cell { { x_mid,  row.min.y }, { x_right, row.max.y } };
```

---

## ImGui::Text / label

ImGui:
```cpp
ImGui::Text("Cutoff: %.2f Hz", freq);
```

Forma: construct once, update via `set_text` or `repress`.

```cpp
// Construction
auto text_buf = Portal::Forma::create_buffer(
    window,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST,
    std::vector { std::pair { std::string("text"), std::shared_ptr<Core::VKImage>{} } });

auto el = Portal::Forma::Element {}
    .non_interactive()
    .with_buffer(text_buf)
    .with_text("Cutoff: 440.00 Hz",
        Portal::Text::PressParams {
            .color = { 0.85F, 0.85F, 0.85F, 1.F },
            .render_bounds = { 512, 48 },
        },
        label_bounds);

const uint32_t label_id = surface.layer().add(el);

// Update from any graphics-thread callback
el.set_text(std::format("Cutoff: {:.2f} Hz", freq),
    Portal::Text::PressParams { .render_bounds = { 512, 48 } });
```

For live-updating readouts driven by a reader function, `make_value_row`
(from `Inspect/QueryUtils.hpp`) does the repress loop for you:

```cpp
const glm::uvec2 dims = row_pixel_dims(window, x_min, x_max, row_h);
auto row_buf = Inspector::make_row_buffer(window, "Cutoff", dims);

auto row = make_value_row(
    ValueSpec {
        .label = "Cutoff",
        .reader = [&freq] { return std::format("{:.2f} Hz", freq.load()); },
    },
    std::move(row_buf),
    surface, cursor, x_min, x_max, row_h);

// Each graphics tick:
row.link.tap();
```

---

## ImGui::Button

ImGui:
```cpp
if (ImGui::Button("Trigger")) { fire(); }
```

Forma: element + `on_press` callback. State lives in your code, not in Forma.

```cpp
constexpr glm::vec3 k_rest  { 0.2F, 0.2F, 0.2F };
constexpr glm::vec3 k_press { 0.7F, 0.3F, 0.2F };

auto buf = Portal::Forma::create_buffer(
    window,
    Kinesis::filled_rect(box, k_rest),
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

const uint32_t id = surface.layer().add(
    Portal::Forma::Element {}
        .with_bounds(box)
        .with_buffer(buf));

surface.ctx().on_press  (id, IO::MouseButtons::Left,
    [buf, box](uint32_t, glm::vec2) {
        buf->submit(Kinesis::filled_rect(box, k_press));
        fire();
    });
surface.ctx().on_release(id, IO::MouseButtons::Left,
    [buf, box](uint32_t, glm::vec2) {
        buf->submit(Kinesis::filled_rect(box, k_rest));
    });
surface.ctx().on_enter  (id, [buf, box](uint32_t) {
    buf->submit(Kinesis::filled_rect(box, { 0.35F, 0.35F, 0.35F }));
});
surface.ctx().on_leave  (id, [buf, box](uint32_t) {
    buf->submit(Kinesis::filled_rect(box, k_rest));
});
```

---

## ImGui::Checkbox / toggle

ImGui:
```cpp
ImGui::Checkbox("Enable", &enabled);
```

Forma: `toggle` geometry function + `on_press` to flip state.

```cpp
auto el = Portal::Forma::create_element<bool>(
    surface,
    Portal::Forma::Geometry::toggle(box, { 0.2F, 0.2F, 0.2F }, { 0.2F, 0.7F, 0.4F }),
    false,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

surface.ctx().on_press(el.element.id, IO::MouseButtons::Left,
    [state = el.state](uint32_t, glm::vec2) {
        state->write(!state->value);
    });

// Read current value anywhere:
const bool enabled = el.state->value;
```

---

## ImGui::SliderFloat / horizontal fader

ImGui:
```cpp
ImGui::SliderFloat("Gain", &gain, 0.0f, 1.0f);
```

Forma: `horizontal_fader` geometry function + drag callbacks.

```cpp
constexpr Kinesis::AABB2D track { { -0.8F, -0.05F }, { 0.8F, 0.05F } };

auto el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::horizontal_fader(track, 0.04F),
    0.5F,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

auto pressed = make_persistent(false);
surface.ctx().on_press  (el.element.id, IO::MouseButtons::Left,
    [&pressed](uint32_t, glm::vec2) { pressed = true; });
surface.ctx().on_release(el.element.id, IO::MouseButtons::Left,
    [&pressed](uint32_t, glm::vec2) { pressed = false; });
surface.ctx().on_move   (el.element.id,
    [state = el.state, &pressed, track](uint32_t, glm::vec2 ndc) {
        if (!pressed) return;
        state->write(std::clamp((ndc.x - track.min.x) / track.width(), 0.F, 1.F));
    });

// Wire to a node:
auto constant = vega.Constant(0.5) | Audio[0];
Portal::Forma::bridge().at(el.state).write(constant);
```

For a knob (arc-based), use `stroke_slider` with an `arc_path`:

```cpp
auto path = Kinesis::arc_path({ 0.F, 0.F }, 0.3F, 0.3F,
    glm::radians(215.F), glm::radians(-35.F), 64);

auto handle_buf = Portal::Forma::create_buffer(
    window, Portal::Graphics::PrimitiveTopology::POINT_LIST);

auto el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::stroke_slider(path, handle_buf,
        0.02F, { 0.2F, 0.2F, 0.25F }, { 0.3F, 0.6F, 1.0F }),
    0.5F,
    Portal::Graphics::PrimitiveTopology::LINE_LIST);
```

---

## ImGui::CollapsingHeader / tree node

ImGui:
```cpp
if (ImGui::CollapsingHeader("Oscillator")) {
    ImGui::SliderFloat("Freq", ...);
}
```

Forma: `Collapsible` + `attach` for each body element.

```cpp
constexpr float x_min = -0.95F, x_max = 0.95F, row_h = 0.055F;

// Header buffer: plain color if no label image, textured if one is set
auto hbuf = Portal::Forma::create_buffer(window,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

auto col = Collapsible {}
    .initially_open(true)
    .closed_color({ 0.2F, 0.2F, 0.22F })
    .open_color({ 0.28F, 0.28F, 0.32F })
    .place(std::move(hbuf), surface, cursor, x_min, x_max, row_h);

// Body elements - each must be created after place() so cursor is advanced
auto fader_el = Portal::Forma::create_element<float>(
    surface,
    Portal::Forma::Geometry::horizontal_fader(
        cursor.advance(row_h), 0.04F),
    0.5F,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

col.attach(surface.layer(), fader_el.element.id);
```

`attach` relates the body element to the header and syncs initial visibility
to the header's open state. Any number of body elements can be attached.

---

## ImGui grouped value readout panel

A scrollable inspector-style panel with live-updating labeled rows grouped
under collapsible headers. This is the pattern the internal Inspector uses.

```cpp
constexpr float x_min = -0.95F, x_max = 0.95F, row_h = 0.05F;
LayoutCursor cursor;

const glm::uvec2 dims = row_pixel_dims(window, x_min, x_max, row_h);

// Pre-create one RowBuffer per data field
auto freq_buf  = Inspector::make_row_buffer(window, "Freq",  dims);
auto amp_buf   = Inspector::make_row_buffer(window, "Amp",   dims);
auto phase_buf = Inspector::make_row_buffer(window, "Phase", dims);
auto hdr_buf   = Inspector::make_row_buffer(window, "Oscillator", dims);

std::array<RowBuffer, 3> row_bufs {
    std::move(freq_buf), std::move(amp_buf), std::move(phase_buf)
};

std::array<ValueSpec, 3> specs { {
    { "Freq",  [&] { return std::format("{:.1f} Hz", freq.load()); } },
    { "Amp",   [&] { return std::format("{:.3f}",    amp.load());  } },
    { "Phase", [&] { return std::format("{:.2f} rad", ph.load());  } },
} };

auto group = make_value_group(
    specs, std::move(hdr_buf), row_bufs,
    surface, cursor, x_min, x_max, row_h, true);

// Each graphics tick - tap all links to repress live values
schedule_metro(1.0 / 30.0, [g = std::move(group)]() mutable {
    for (auto& row : g.rows)
        row.link.tap();
});
```

For deeply nested trees (node graph inspection, buffer chains), use
`InspectResult` which nests `ValueGroup` recursively and exposes `tap_all()`.

---

## Visibility and z-order

ImGui visibility is implicit (don't call the widget). Forma is explicit.

```cpp
// Show / hide an element
surface.layer().set_visible(id, false);
surface.layer().set_visible(id, true);

// Cascades to all related children automatically when using relate_to:
surface.layer().set_visible(parent_id, false);  // hides all attached children

// Z-order
surface.layer().send_to_back(id);
surface.layer().send_to_front(id);
```

---

## Scrollable regions

Forma has no built-in scroll container. The pattern is a `LayoutCursor` with a
float offset applied to all bounds, driven by scroll events on a background
hit region:

```cpp
auto scroll_offset = make_persistent(0.F);
constexpr float scroll_speed = 0.04F;

// Background hit region covers the panel area
const uint32_t panel_id = surface.layer().add(
    Portal::Forma::Element {}
        .with_bounds(panel_bounds)
        .non_interactive());    // non_interactive = no pointer grab, but scroll fires

surface.ctx().on_scroll(panel_id,
    [&scroll_offset](uint32_t, glm::vec2 delta) {
        scroll_offset = std::clamp(
            scroll_offset + delta.y * scroll_speed, -8.F, 0.F);
        // re-run layout with new offset, or drive LayoutCursor state
    });
```

Full scrollable panels require re-placing elements at new bounds when the
offset changes, or using a LayoutCursor whose `state()` is closed over by
all body geometry functions so they reflow on `state()->write(new_y)`.

---

## Key differences from ImGui

| ImGui | Forma |
|---|---|
| Immediate: re-emit every frame | Retained: construct once, update on change |
| Implicit layout engine | Explicit NDC arithmetic + LayoutCursor |
| Internal widget state | Your state, written via `MappedState<T>::write` |
| `if (Button(...))` inline action | `on_press` callback registered once |
| Style stack | Per-element color/geometry at construction |
| Docking, tables, scroll built-in | Build from primitives; no built-in containers |
| Thread: main/render thread only | Callbacks on graphics thread; writes from any thread |
| Immediate feedback | Geometry updates on next `sync()` after `write()` |
