# Portal::Forma: UI Patterns

Forma has no widget hierarchy, no retained tree, no retained state beyond
what you explicitly create. If you are coming from ImGui, that is the core
difference: ImGui owns your layout and state management. Forma does not. You
own layout, Forma owns rendering and hit testing.

This guide maps common ImGui patterns to their Forma equivalents.

Forma has one further difference from ImGui that this guide leans on
throughout. A `Geometry::` factory returns a `Form<T>`: a geometry function
together with the topology, buffer capacity, and interaction its realization
requires. `Portal::Forma::create` realizes one. So the ImGui equivalents below
are usually a single call, not because Forma grew a widget layer, but because
the geometry function already knew its own topology and its own inverse
mapping and now carries them.

Every field of a `Form` is public. Clearing `wire` hands interaction back to
you, which several patterns below do.

Most patterns below construct a `Form<T>` and then bind it to something with
`Bridge`. `Tend<T>` chains both into one statement (`place(surface).bridge()`)
whenever they happen together, see [Portal-Forma.md](Portal-Forma.md)'s
"Tend\<T\>" section. Patterns that construct and stop there, with nothing to
bind or nothing reused past construction, are left as plain `create` calls
below, since `Tend<T>` only shortens the cases with something to chain onto.

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
- Layout is explicit and done once at construction: `LayoutCursor` to advance
  rows, `Kinesis::place` to position one region against another, `Surface`
  named regions for screen anchors.

The frame loop runs invisibly. You do not call `Begin`, you do not loop over
widgets. You write a value and the geometry function reacts.

---

## Layout

ImGui handles cursor advancement internally. Forma uses `LayoutCursor` to
advance a baseline, `Kinesis::place` to position one region against another,
and `Surface` named regions (`top_left`, `bottom_strip`, ...) for screen
anchors with no NDC arithmetic. See [Portal-Forma.md](Portal-Forma.md)'s "Layout" section for
the full API. From ImGui's perspective the shift is that layout happens once
at construction, not every frame.

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

For live-updating readouts driven by a reader function, `make_entry` (from
`Primitives/Entry.hpp`) does the repress loop for you. The row is one
fully-textured quad, with background and label composited together using the
`PressParams::background` trick above.

```cpp
auto row_buf = Inspector::make_row_buffer(window, "Cutoff", row_pixel_dims(window, cursor, row_h));

auto row = make_entry(
    EntrySpec {
        .label = "Cutoff",
        .reader = [&freq] { return std::format("{:.2f} Hz", freq.load()); },
    },
    std::move(row_buf),
    surface, cursor, row_h);

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
A labeled button is one element, not two: `with_text`'s `PressParams::background`
composites the fill colour beneath the label into the same texture, so state
changes are one `set_text` call, not a separate quad submit plus an overlay
relate.

```cpp
constexpr glm::vec4 k_rest  { 0.2F, 0.2F, 0.2F, 1.F };
constexpr glm::vec4 k_press { 0.7F, 0.3F, 0.2F, 1.F };
constexpr glm::vec4 k_hover { 0.35F, 0.35F, 0.35F, 1.F };

auto buf = Portal::Forma::create_buffer(
    window,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST,
    std::vector{ std::pair{ std::string("text"), std::shared_ptr<Core::VKImage>{} } });

auto el = Portal::Forma::Element {}
    .with_bounds(box)
    .with_buffer(buf)
    .with_text("Trigger",
        Portal::Text::PressParams { .color = { 1.F, 1.F, 1.F, 1.F }, .background = k_rest, .render_bounds = { 256, 48 } },
        box);

const uint32_t id = surface.layer().add(el).id();

auto paint = [el](glm::vec4 bg) mutable {
    el.set_text("Trigger", Portal::Text::PressParams { .background = bg, .render_bounds = { 256, 48 } });
};

surface.ctx().on_press  (id, IO::MouseButtons::Left, [paint](uint32_t, glm::vec2) mutable { paint(k_press); fire(); });
surface.ctx().on_release(id, IO::MouseButtons::Left, [paint](uint32_t, glm::vec2) mutable { paint(k_rest); });
surface.ctx().on_enter  (id, [paint](uint32_t) mutable { paint(k_hover); });
surface.ctx().on_leave  (id, [paint](uint32_t) mutable { paint(k_rest); });
```

---

## ImGui::Checkbox / toggle

ImGui:

```cpp
ImGui::Checkbox("Enable", &enabled);
```

Forma: the `toggle` Form carries the press that flips the value.

```cpp
auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::toggle(box, { 0.2F, 0.2F, 0.2F }, { 0.2F, 0.7F, 0.4F }),
    false);

// Read current value anywhere:
const bool enabled = el.state->value;
```

To act on the change rather than poll, take the wire back. Registering a
second `on_press` on the same button replaces the Form's, so combine both in
one handler:

```cpp
auto form = Portal::Forma::Geometry::toggle(box, { 0.2F, 0.2F, 0.2F }, { 0.2F, 0.7F, 0.4F });
form.wire = {};

auto el = Portal::Forma::create(surface, std::move(form), false);

surface.ctx().on_press(el.element.id, IO::MouseButtons::Left,
    [state = el.state](uint32_t, glm::vec2) {
        state->write(!state->value);
        on_toggled(state->value);
    });
```

---

## ImGui::SliderFloat / horizontal fader

ImGui:

```cpp
ImGui::SliderFloat("Gain", &gain, 0.0f, 1.0f);
```

Forma: the `horizontal_fader` Form carries the drag. `Tend<T>` chains its
construction straight into the bridge binding:

```cpp
constexpr Kinesis::AABB2D track { { -0.8F, -0.05F }, { 0.8F, 0.05F } };
auto constant = vega.Constant(0.5) | Audio[0];

Portal::Forma::Tend<float> fader {
    .form = Portal::Forma::Geometry::horizontal_fader(track, 0.04F),
    .initial = 0.5F,
};
fader.place(surface).bridge().write(constant);
```

The drag maps the cursor onto the handle's travel, which is shorter than the
track by one handle width. Writing that mapping by hand is where a
hand-rolled fader usually goes half a handle out of alignment at the extremes.

Arrow-key fine adjustment is separate, since the Form carries pointer
interaction only. Focus transfers on click:

```cpp
surface.ctx().key_step(fader.result->element.id, fader.result->state,
    IO::Keys::ArrowLeft, IO::Keys::ArrowRight, 0.005F);
```

For a knob, `stroke_slider` along an `arc_path`. The Form carries the
arc-length projection that the previous version of this guide left as a
comment:

```cpp
auto path = Kinesis::arc_path({ 0.F, 0.F }, 0.3F, 0.3F,
    glm::radians(215.F), glm::radians(-35.F), 64);

auto handle_buf = Portal::Forma::create_buffer(
    window, Portal::Graphics::PrimitiveTopology::POINT_LIST);

auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::stroke_slider(path, handle_buf,
        0.02F, { 0.2F, 0.2F, 0.25F }, { 0.3F, 0.6F, 1.0F }),
    0.5F);
```

For a scaled range rather than [0,1], compose the projection and replace the
wire:

```cpp
auto form = Portal::Forma::Geometry::horizontal_fader(track, 0.04F);
form.wire = Portal::Forma::Geometry::drag_with<float>(
    Kinesis::scaled(Kinesis::axis_fraction(track, 0.04F), 20.F, 20000.F));

auto el = Portal::Forma::create(surface, std::move(form), 440.F);
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
constexpr float row_h = 0.055F;
LayoutCursor cursor { 1.F, -0.95F, 0.95F };

auto hbuf = Portal::Forma::create_buffer(window,
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

auto col = Collapsible {}
    .initially_open(true)
    .closed_color({ 0.2F, 0.2F, 0.22F })
    .open_color({ 0.28F, 0.28F, 0.32F })
    .label("Oscillator")
    .place(std::move(hbuf), surface, cursor, row_h);

auto fader_el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::horizontal_fader(cursor.advance(row_h), 0.04F),
    0.5F);

col.attach(surface.layer(), fader_el.element.id);
```

`attach` relates the body element to the header and syncs initial visibility
to the header's open state. Any number of body elements can be attached.

---

## ImGui grouped value readout panel

A scrollable inspector-style panel with live-updating labeled rows grouped
under collapsible headers. This is the pattern the internal Inspector uses.

```cpp
constexpr float row_h = 0.05F;
LayoutCursor cursor { 1.F, -0.95F, 0.95F };

const glm::uvec2 dims = row_pixel_dims(window, cursor, row_h);

// Pre-create one EntryBuffer per data field. The header needs only a plain
// buffer, since Collapsible::label() composites its own text and needs no
// pre-pressed image.
auto freq_buf  = Inspector::make_row_buffer(window, "Freq",  dims);
auto amp_buf   = Inspector::make_row_buffer(window, "Amp",   dims);
auto phase_buf = Inspector::make_row_buffer(window, "Phase", dims);
auto hdr_buf   = Inspector::make_header_buffer(window);

std::array<EntryBuffer, 3> row_bufs {
    std::move(freq_buf), std::move(amp_buf), std::move(phase_buf)
};

std::array<EntrySpec, 3> specs { {
    { "Freq",  [&] { return std::format("{:.1f} Hz", freq.load()); } },
    { "Amp",   [&] { return std::format("{:.3f}",    amp.load());  } },
    { "Phase", [&] { return std::format("{:.2f} rad", ph.load());  } },
} };

auto group = make_entry_group(
    specs, "Oscillator", std::move(hdr_buf), row_bufs,
    surface, cursor, row_h, true);

// Each graphics tick, tap all links to repress live values
schedule_metro(1.0 / 30.0, [g = std::move(group)]() mutable {
    for (auto& row : g.rows)
        row.link.tap();
});
```

For deeply nested trees (node graph inspection, buffer chains), use
`InspectResult` which nests `EntryGroup` recursively and exposes `tap_all()`.

---

## Drawable canvas / array editor

ImGui has no direct equivalent. The closest approximation is a sequence of
`SliderFloat` calls, which produces N discrete sliders with no drawn curve,
no drag interpolation, and no direct routing to an audio buffer.

Forma's `drawable_canvas` renders a `vector<float>` of N samples as a
continuous polyline. Its Form carries the painting interaction: NDC to sample
index, amplitude mapping, gap interpolation under fast drag, and the version
increment that triggers `sync()`. The state vector routes directly to any bulk
float consumer.

`Geometry::wire_canvas_drag(ctx, id, state, bounds)` registers that same
interaction directly, for a canvas assembled by hand or after clearing the
Form's `wire`.

```cpp
constexpr Kinesis::AABB2D bounds { glm::vec2(-0.8F, -0.6F), glm::vec2(0.8F, 0.6F) };
constexpr uint32_t k_n = 256;

auto el = Portal::Forma::create(
    surface,
    Portal::Forma::Geometry::drawable_canvas(bounds),
    std::vector<float>(k_n, 0.5F));
```

Routing to consumers:

```cpp
// Wavetable or envelope written directly to audio output
auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
auto audio_buf = std::make_shared<Buffers::AudioBuffer>(0, k_n);
audio_buf->set_default_processor(writer);
register_audio_buffer(audio_buf, 0);
Portal::Forma::bridge().at(el.state).write(writer);

// IIR feedforward coefficients
auto iir = vega.IIR(rand, a_coefs, b_coefs) | Audio[0];
Portal::Forma::bridge().at(el.state).write(
    [iir](std::span<const float> s) {
        iir->setBCoefficients({ s.begin(), s.end() });
    });

// Any N-element float consumer
Portal::Forma::bridge().at(el.state).write(
    [](std::span<const float> s) { /* s.data(), s.size() */ });
```

Two canvases on one surface give independent control over separate arrays,
for example IIR a-coefs and b-coefs simultaneously:

```cpp
Portal::Forma::Tend<std::vector<float>> b_canvas {
    .form = Portal::Forma::Geometry::drawable_canvas(b_bounds, { 0.3F, 0.8F, 0.4F }),
    .initial = b_init,
};
b_canvas.place(surface).bridge().write([iir](std::span<const float> s) {
    iir->setBCoefficients({ s.begin(), s.end() });
});

Portal::Forma::Tend<std::vector<float>> a_canvas {
    .form = Portal::Forma::Geometry::drawable_canvas(a_bounds, { 0.8F, 0.4F, 0.3F }),
    .initial = a_init,
};
a_canvas.place(surface).bridge().write([iir](std::span<const float> s) {
    if (!s.empty() && s[0] != 0.F)
        iir->setACoefficients({ s.begin(), s.end() });
});
```

---

## Visibility and z-order

ImGui visibility is implicit (don't call the widget). Forma is explicit.

```cpp
// Show / hide an element
surface.layer().set_visible(id, false);
surface.layer().set_visible(id, true);

// Cascades to all related children automatically when using relate_to:
surface.layer().set_visible(parent_id, false);  // hides all attached children

// Z-order now also reorders paint order (RootGraphicsBuffer draw_priority),
// not just hit-test order, and it cascades to every related() element the
// same way visibility does.
surface.layer().send_to_back(id);
surface.layer().bring_to_front(id);
```

---

## Scrollable regions

`Scrollable` is a clipped, scroll-offset viewport over content taller than
it: a background/hit-test element, wheel input wired by default, and a
`LayoutCursor` (`cursor_out`) already bound to the live offset, so content
placed through it lands correctly whether or not a scroll already happened.
It does not know what kind of content lives inside it. `track()` registers an
element id plus your own reposition callback, invoked with that element's
scrolled bounds on every scroll, the same shape as `Collapsible::attach()`.

```cpp
auto buf = Portal::Forma::create_buffer(window, Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);

auto panel = Scrollable {}
    .background_color({ 0.08F, 0.08F, 0.1F })
    .place(buf, surface, viewport_bounds);

const auto row_bounds = panel.cursor_out.advance(row_h);
auto row_buf = Portal::Forma::create_buffer(
    window, Kinesis::filled_rect(row_bounds, color),
    Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP);
const uint32_t row_id = surface.layer().add(
    Portal::Forma::Element {}.with_bounds(row_bounds).with_buffer(row_buf).non_interactive());

panel.track(surface.layer(), row_id, row_bounds,
    [row_buf, color](Kinesis::AABB2D shifted) {
        row_buf->submit(Kinesis::filled_rect(shifted, color));
    },
    row_buf); // clip_buf: same buffer, scissored to the viewport
```

`indicator()` attaches a caller-drawn `Mapped<glm::vec2>` as a live, draggable
scrollbar. Scrollable drives it with `(position_frac, extent_frac)` and draws
nothing itself; `Geometry::scroll_indicator(track, color)` is the ready-made
bar. `scroll_by(layer, delta)` is the input-agnostic core wheel input already
calls, so any other source (a dragged indicator, a key, MIDI) can drive the
same viewport directly.

---

## Key differences from ImGui

| ImGui                                             | Forma                                                                                                  |
| ------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Immediate: re-emit every frame                    | Retained: construct once, update on change                                                             |
| Implicit layout engine                            | Explicit NDC arithmetic + LayoutCursor                                                                 |
| Internal widget state                             | Your state, written via `MappedState<T>::write`                                                        |
| `if (Button(...))` inline action                  | `on_press` callback registered once                                                                    |
| `SliderFloat` with press+drag implicit            | `on_drag` callback with no press flag                                                                  |
| No keyboard routing to widgets                    | Per-element key focus: `on_press(key)`, `on_held(key)`, `on_release(key)`                              |
| Style stack                                       | Per-element color/geometry at construction                                                             |
| Docking, tables built-in                          | Build from primitives; no built-in containers. `Scrollable` ships a scroll viewport                    |
| Thread: main/render thread only                   | Callbacks on graphics thread; writes from any thread                                                   |
| Immediate feedback                                | Geometry updates on next `sync()` after `write()`                                                      |
| No curve/array editor primitive                   | `drawable_canvas` + `wire_canvas_drag`: drawn curve routes directly to audio or any N-element consumer |
| No audio graph wiring                             | `Bridge::write` routes element value to nodes, shaders, audio processors, or arbitrary span sinks      |
| No way to build a widget the library did not ship | Any function returning a Form<T> is treated identically to the shipped ones                            |
