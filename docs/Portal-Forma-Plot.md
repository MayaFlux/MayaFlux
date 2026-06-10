# Portal::Forma::Plot

A plot is a `Mapped<shared_ptr<PlotContainer>>` on a `Surface`. The container
holds one or more named scalar series. A geometry function reads those series
each frame and writes vertex bytes into a `FormaBuffer`. `sync()` drives it.

Everything is `f(x)`: what the data is, where it comes from, how it maps to
screen space, how it is encoded into geometry, and when it updates. The builder
chain is a convenience surface over a raw `GeometryFn` lambda. When the builder
cannot express what you need, drop to the lambda directly.

---

## Building a data source

`Plot::source()` constructs a `PlotContainer` incrementally. Each `.as()`
adds a named series and `.from()` binds a data acquisition path to it.

```cpp
// from a Node
auto container = Plot::source()
    .as("osc", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sine_node)
    .build();

// from a callable
auto container = Plot::source()
    .as("sine", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D)
    .from([](std::vector<double>& s) {
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = std::sin(static_cast<double>(i) / s.size() * 2.0 * M_PI);
    })
    .build();

// from an AudioBuffer
auto container = Plot::source()
    .as("vol", 64, Role::SPATIAL_Y, DataModality::AUDIO_1D)
    .from(audio_buf)
    .build();

// multiple series in one container
auto container = Plot::source()
    .as("x", 1024, Role::SPATIAL_X, DataModality::TENSOR_ND)
    .from([](std::vector<double>& s) { /* fill X */ })
    .as("y", 1024, Role::SPATIAL_Y, DataModality::TENSOR_ND)
    .from([](std::vector<double>& s) { /* fill Y */ })
    .build();
```

`Role` is the semantic axis intent used by the geometry function to locate
series. Common values: `SPATIAL_X`, `SPATIAL_Y`, `SPATIAL_Z`, `FREQUENCY`,
`TIME`, `COLOR`, `CUSTOM`.

`DataModality` describes what the series represents: `AUDIO_1D` for
time-domain, `SPECTRAL_2D` for pre-computed frequency bins, `TENSOR_ND` for
anything else. It affects how `process_default()` acquires from node bindings.

---

## Describing the geometry

`Plot::series()` accumulates axis role mappings and an optional global palette,
then terminals produce a `SeriesSpec` carrying the geometry function, topology,
and buffer capacity arithmetic.

### Waveform (LINE_STRIP)

```cpp
auto spec = Plot::series()
    .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.8F, 1.0F) })
    .as_waveform()
    .thickness(1.5F)
    .done();
```

Y series from the container are drawn as separate LINE_STRIP runs joined by
degenerate vertices, sharing one buffer. X positions are uniform when no
`SPATIAL_X` series is present.

### Scatter (POINT_LIST)

```cpp
auto spec = Plot::series()
    .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))
    .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.9F, 0.3F, 0.8F) })
    .as_scatter()
    .point_size(3.F)
    .done();
```

X and Y series are paired by index. Z series map to vertex Z when present.

### Bars (TRIANGLE_LIST)

```cpp
auto spec = Plot::series()
    .x(Role::SPATIAL_X, AxisRange{}.range(-0.9F, 0.9F))
    .y(Role::SPATIAL_Y, AxisRange{}.range(0.F, 1.F), { glm::vec3(0.2F, 0.9F, 0.5F) })
    .as_bars()
    .done();
```

X range tiles bars uniformly when no explicit X series is present.

### Multiple series, multiple colors

```cpp
auto spec = Plot::series()
    .y(Role::SPATIAL_Y,
        AxisRange{}.range(-1.F, 1.F),
        { glm::vec3(0.2F, 0.8F, 1.F),
          glm::vec3(1.F, 0.5F, 0.1F),
          glm::vec3(0.5F, 1.F, 0.3F) })
    .as_waveform()
    .done();
```

The palette cycles across all `SPATIAL_Y` series in the container.

### Auto-scaling axis

```cpp
AxisRange{}.auto_scale()
AxisRange{}.scale_if([&active] { return active.load(); })
```

### Background quad

```cpp
Plot::series()
    .y(...)
    .background({ glm::vec2(-0.9F, -0.9F), glm::vec2(0.9F, 0.9F) })
    .as_bars().done();
```

Only has effect when using the `Portal::Forma::plot()` convenience overload,
which creates a background element automatically behind the plot geometry.

---

## Placing onto a surface

### The manual path

```cpp
constexpr uint64_t N = 512;

auto container = Plot::source()
    .as("osc", N, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sine)
    .build();

auto spec = Plot::series()
    .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.9F, 0.6F) })
    .as_waveform().thickness(1.5F).done();

auto buf = Portal::Forma::create_buffer(surface.window(), spec.capacity_for(N), spec.topology);

auto el = Plot::place(surface, buf, std::move(spec), std::move(container));

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

`spec.capacity_for(N)` and `spec.topology` encode the correct buffer size and
primitive type for the chosen encoding. You never supply these manually.

`sync()` reruns the geometry function unconditionally for plot elements
(`force_redraw_on_sync` is set by `Plot::place`). Call it as often as you
want new data to appear.

### The convenience overload

```cpp
auto [el, surface] = Portal::Forma::plot("Plot Window", 1280, 720,
    Plot::source()
        .as("osc", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sine)
        .build(),
    Plot::series()
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.9F, 0.6F) })
        .as_waveform().done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

Creates the window, shows it, builds the surface, constructs and registers the
buffer, runs the first sync. Returns the `Mapped` and the `Surface`.

---

## Raw geometry function

The builder produces a `GeometryFn<shared_ptr<PlotContainer>>`. The raw lambda
is always valid and preferred when the builder is insufficient:

```cpp
GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> geom =
    [](const std::shared_ptr<Kakshya::PlotContainer>& c,
       std::vector<uint8_t>& out,
       Portal::Forma::Element& el) {

        c->process_default();   // acquire from bound sources into processed_data

        auto y_series = Plot::series_by_role(*c, Role::SPATIAL_Y);
        // write vertex bytes into out manually
    };
```

When using the raw lambda, call `c->process_default()` at the top yourself.
The builder terminals do this internally; the raw lambda does not.

`series_by_role` returns `vector<span<const double>>`, one span per matching
series.

---

## Driving data

The data source is bound at container construction time. How often the series
updates depends on the source type:

**Node**: `process_default()` calls `extract_multiple_samples(node, N)` each
frame. The node must be processing (mock process enabled or connected to a
running audio graph).

**Callable**: `process_default()` calls `fn(series_vector)` each frame. The
callable owns the fill logic entirely.

**AudioBuffer**: `process_default()` copies `get_data()` span each frame. The
buffer can be filled externally at any rate.

`schedule_metro` at display rate (~60 Hz) is the standard driver for all three:

```cpp
schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

For a live audio source that must stay in sync with actual buffer output, call
`sync()` from a `GraphicsRoutine` instead, co-awaiting `FrameDelay { 1 }`.

---

## Existing surface

`Plot::place` places onto any `Surface` you already have. Use this when the
plot shares a window with other Forma elements:

```cpp
auto buf = Portal::Forma::create_buffer(surface.window(), spec.capacity_for(N), spec.topology);
auto el  = Plot::place(surface, std::move(buf), std::move(spec), std::move(container));
```

## Adornments

Adornments are text labels, axis tick labels, and legends associated with a
plot. They are described on the `Series` chain and materialized automatically
when the plot is placed via `Portal::Forma::plot()`.

### Bounds

`.bounds()` declares the NDC region occupied by the data area. All adornment
placement is relative to this. Required whenever ticks or legends are used.

```cpp
constexpr Kinesis::AABB2D plot_bounds { glm::vec2(-0.75F, -0.75F), glm::vec2(0.7F, 0.8F) };

Plot::series()
    .y(...)
    .bounds(plot_bounds)
    ...
```

### Tick labels

`.x_ticks()` and `.y_ticks()` request evenly-spaced numeric labels along a
plot edge. The range is derived from the axis mapping by default, or supplied
explicitly.

```cpp
// Y ticks on the left, 5 labels, 2 decimal places, range from mapping
.y_ticks(5, Plot::TickEdge::Left, 2)

// X ticks on the bottom, explicit range, integer labels
.x_ticks(Plot::AxisRange{}.range(0.F, 512.F), 5, Plot::TickEdge::Bottom, 0)
```

`.ticks()` places labels on any edge with an explicit range when neither X nor
Y axis inference is appropriate:

```cpp
.ticks(Plot::TickEdge::Right, Plot::AxisRange{}.range(0.F, 1.F), 4)
```

### Legend

`.legend()` adds a vertical legend. Pass an origin in NDC and a list of
entries, each carrying a label string and a color matching the corresponding
series palette entry.

```cpp
.legend(
    glm::vec2(0.72F, 0.8F),
    { Plot::LegendEntry { "sine_a", glm::vec3(0.2F, 0.6F, 1.0F) },
      Plot::LegendEntry { "sine_b", glm::vec3(0.9F, 0.4F, 0.1F) } })
```

### Free text labels

`.label()` places an arbitrary text string at a given NDC region:

```cpp
.label("Amplitude vs Sample",
    Kinesis::AABB2D { .min = { -0.15F, 0.83F }, .max = { 0.5F, 0.95F } })
```

Color defaults to `{ 0.85, 0.85, 0.85, 1.0 }`. An optional fourth argument
overrides it.

### Complete example

```cpp
constexpr Kinesis::AABB2D bounds { glm::vec2(-0.75F, -0.75F), glm::vec2(0.7F, 0.8F) };
constexpr uint64_t N = 512;

auto [el, surface] = Portal::Forma::plot("Annotated Waveform", 1280, 720,
    Plot::source()
        .as("sine_a", N, Role::SPATIAL_Y, DataModality::AUDIO_1D)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .as("sine_b", N, Role::SPATIAL_Y, DataModality::AUDIO_1D)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = 0.5 * std::sin(static_cast<double>(i) / s.size() * 4.0 * M_PI);
        })
        .build(),
    Plot::series()
        .y(Role::SPATIAL_Y,
            Plot::AxisRange{}.range(-1.F, 1.F),
            { glm::vec3(0.2F, 0.6F, 1.0F), glm::vec3(0.9F, 0.4F, 0.1F) })
        .background(bounds)
        .bounds(bounds)
        .y_ticks(5, Plot::TickEdge::Left, 2)
        .x_ticks(Plot::AxisRange{}.range(0.F, static_cast<float>(N)), 5, Plot::TickEdge::Bottom, 0)
        .legend(
            glm::vec2(0.72F, 0.8F),
            { Plot::LegendEntry { "sine_a", glm::vec3(0.2F, 0.6F, 1.0F) },
              Plot::LegendEntry { "sine_b", glm::vec3(0.9F, 0.4F, 0.1F) } })
        .label("Amplitude vs Sample",
            Kinesis::AABB2D { .min = { -0.15F, 0.83F }, .max = { 0.5F, 0.95F } })
        .as_filled_waveform()
        .baseline(0.F)
        .done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

Adornments are only materialized automatically when using `Portal::Forma::plot()`.
On the manual path (`Plot::place()` directly), adornment buffers must be
constructed and placed explicitly via `Plot::place_label()` and
`Plot::place_rect()`.

---

## Quick examples

### Static sine waveform

```cpp
auto [el, _] = Portal::Forma::plot("Sine", 1280, 720,
    Plot::source()
        .as("s", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(static_cast<double>(i) / s.size() * 2.0 * M_PI);
        }).build(),
    Plot::series()
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.8F, 1.F) })
        .as_waveform().done());
```

No `schedule_metro` needed for static data.

### Live FM oscillator

```cpp
auto mod  = vega.Random();
auto sine = vega.Sine(2) | Audio[0];
sine->enable_mock_process(true);
sine->set_amplitude_modulator(mod);

auto [el, _] = Portal::Forma::plot("FM", 1280, 720,
    Plot::source()
        .as("fm", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sine).build(),
    Plot::series()
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.9F, 0.6F) })
        .as_waveform().thickness(1.5F).done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

### Lissajous scatter

```cpp
constexpr uint64_t N = 1024;
auto [el, _] = Portal::Forma::plot("Lissajous", 1280, 720,
    Plot::source()
        .as("lx", N, Role::SPATIAL_X, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(3.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .as("ly", N, Role::SPATIAL_Y, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(2.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI + M_PI / 4.0);
        }).build(),
    Plot::series()
        .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.9F, 0.3F, 0.8F) })
        .as_scatter().point_size(3.F).done());
```

### Volume bars from mic input

```cpp
// create_input_listener_buffer gives a raw AudioBuffer whose get_data()
// is time-domain samples, not frequency bins. Use SPATIAL_Y + AUDIO_1D
// to plot raw amplitude as bars. For a real spectrum, pre-compute the FFT
// in a callable before binding.

auto [el, _] = Portal::Forma::plot("Volume", 1280, 720,
    Plot::source()
        .as("vol", 64, Role::SPATIAL_Y, DataModality::AUDIO_1D)
        .from(create_input_listener_buffer(0, false)).build(),
    Plot::series()
        .x(Role::SPATIAL_X, AxisRange{}.range(-0.9F, 0.9F))
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.2F, 0.9F, 0.5F) })
        .background({ glm::vec2(-0.9F, -0.9F), glm::vec2(0.9F, 0.9F) })
        .as_bars().done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

### Three oscillators, shared Y axis, auto-scale

```cpp
auto [el, _] = Portal::Forma::plot("Multi", 1280, 720,
    Plot::source()
        .as("a", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_a)
        .as("b", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_b)
        .as("c", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_c)
        .build(),
    Plot::series()
        .y(Role::SPATIAL_Y, AxisRange{}.auto_scale(),
            { glm::vec3(1.F, 0.3F, 0.2F),
              glm::vec3(0.2F, 0.8F, 1.F),
              glm::vec3(0.4F, 1.F, 0.3F) })
        .as_waveform().done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

### Multiple X and Y on the same plot

Each `.x()` and `.y()` call appends an independent axis mapping with its own
`AxisRange`. The geometry function pairs them by mapping index: first `.x()`
against first `.y()`, second `.x()` against second `.y()`, and so on. All
pairs land in one buffer and one draw call.

**Two Lissajous figures, one scatter, different frequencies**

```cpp
auto [el, _] = Portal::Forma::plot("Two Lissajous", 1280, 720,
    Plot::source()
        .as("x0", 512, Role::SPATIAL_X, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(3.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .as("y0", 512, Role::SPATIAL_Y, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(2.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .as("x1", 512, Role::SPATIAL_X, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(5.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .as("y1", 512, Role::SPATIAL_Y, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::sin(4.0 * static_cast<double>(i) / s.size() * 2.0 * M_PI);
        })
        .build(),
    Plot::series()
        .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F),
            { glm::vec3(0.9F, 0.3F, 0.8F), glm::vec3(0.2F, 0.9F, 0.5F) })
        .as_scatter().point_size(2.F).done());
```

Two `SPATIAL_X` series and two `SPATIAL_Y` series under one `.x()` and one
`.y()` call. The scatter builder pairs them by index within those mappings:
`(x0, y0)` in magenta, `(x1, y1)` in green.

**Two independent X ranges, two Y series**

Two separate `.x()` calls each carry their own `AxisRange`, letting two scatter
clouds with completely different domains overlay in the same draw call:

```cpp
auto [el, _] = Portal::Forma::plot("Dual Domain", 1280, 720,
    Plot::source()
        .as("x_audio", 256, Role::SPATIAL_X, DataModality::AUDIO_1D).from(osc_a)
        .as("y_audio", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_b)
        .as("x_ctrl",  256, Role::SPATIAL_X, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = static_cast<double>(i) / static_cast<double>(s.size() - 1);
        })
        .as("y_ctrl",  256, Role::SPATIAL_Y, DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::pow(static_cast<double>(i) / static_cast<double>(s.size() - 1), 2.0);
        })
        .build(),
    Plot::series()
        .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))   // audio domain
        .x(Role::SPATIAL_X, AxisRange{}.range( 0.F, 1.F))   // control domain
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { glm::vec3(0.9F, 0.3F, 0.2F) })
        .y(Role::SPATIAL_Y, AxisRange{}.range( 0.F, 1.F), { glm::vec3(0.2F, 0.7F, 1.F) })
        .as_scatter().point_size(3.F).done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

First `.x()` covers `x_audio`, second `.x()` covers `x_ctrl`. Each is paired
with its corresponding `.y()` mapping.

**Two oscillators against a shared time axis, waveform**

```cpp
auto [el, _] = Portal::Forma::plot("Two Osc", 1280, 720,
    Plot::source()
        .as("t",     256, Role::TIME,      DataModality::TENSOR_ND)
        .from([](std::vector<double>& s) {
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = static_cast<double>(i) / static_cast<double>(s.size() - 1);
        })
        .as("osc_a", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_a)
        .as("osc_b", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(osc_b)
        .build(),
    Plot::series()
        .x(Role::TIME, AxisRange{}.range(0.F, 1.F))
        .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F),
            { glm::vec3(1.F, 0.4F, 0.1F), glm::vec3(0.1F, 0.6F, 1.F) })
        .as_waveform().thickness(1.5F).done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

One `TIME` series shared as X for both `SPATIAL_Y` series. Both waveforms land
in the same LINE_STRIP buffer as separate runs separated by degenerate vertices.

**Three node phase portraits**

```cpp
auto [el, _] = Portal::Forma::plot("Phase Space", 1280, 720,
    Plot::source()
        .as("mod_x",     256, Role::SPATIAL_X, DataModality::AUDIO_1D).from(mod)
        .as("carrier_y", 256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(carrier)
        .as("carrier_x", 256, Role::SPATIAL_X, DataModality::AUDIO_1D).from(carrier)
        .as("sum_y",     256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sum)
        .as("sum_x",     256, Role::SPATIAL_X, DataModality::AUDIO_1D).from(sum)
        .as("mod_y",     256, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(mod)
        .build(),
    Plot::series()
        .x(Role::SPATIAL_X, AxisRange{}.auto_scale())
        .y(Role::SPATIAL_Y, AxisRange{}.auto_scale(),
            { glm::vec3(1.F, 0.5F, 0.1F),
              glm::vec3(0.1F, 0.7F, 1.F),
              glm::vec3(0.6F, 1.F, 0.2F) })
        .as_scatter().point_size(2.F).done());

schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```

Three X series and three Y series under one `.x()` and one `.y()` call.
Each pair is a phase-space portrait of two nodes against each other.

---

### Raw lambda

```cpp
constexpr uint64_t N = 256;

auto container = Plot::source()
    .as("data", N, Role::CUSTOM, DataModality::TENSOR_ND)
    .from([](std::vector<double>& s) { /* fill arbitrarily */ })
    .build();

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> geom =
    [](const std::shared_ptr<Kakshya::PlotContainer>& c,
       std::vector<uint8_t>& out, Portal::Forma::Element&) {
        c->process_default();
        auto series = Plot::series_by_role(*c, Role::CUSTOM);
        if (series.empty()) return;
        out.clear();
        for (size_t i = 0; i < series[0].size(); ++i) {
            float x = static_cast<float>(i) / static_cast<float>(series[0].size() - 1) * 2.F - 1.F;
            float y = static_cast<float>(series[0][i]);
            // write whatever vertex layout you need into out
        }
    };

SeriesSpec spec {
    .fn           = geom,
    .topology     = Graphics::PrimitiveTopology::POINT_LIST,
    .capacity_for = [](uint64_t n) { return n * 60; },
};

auto buf = Portal::Forma::create_buffer(surface.window(), spec.capacity_for(N), spec.topology);
auto el  = Plot::place(surface, std::move(buf), std::move(spec), std::move(container));
schedule_metro(1.0 / 60.0, [el = std::move(el)]() mutable { el.sync(); });
```
