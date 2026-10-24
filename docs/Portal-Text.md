# Portal::Text

Portal::Text turns UTF-8 strings into GPU-drawable text in two ways:

- **Pressed images** (`press`, `repress`, `impress`): the string is rasterized
  into a GPU-resident texture that the buffer pipeline draws as a textured
  quad. A `TextBuffer` keeps one image as its text is replaced or appended.
- **Glyph quads** (`TextRun`, `AtlasImage`, `GlyphMesh`): the string is laid out
  as quads for visible glyphs and drawn as triangle-list vertices that sample
  coverage from the shared glyph atlas image. No per-string image exists.
  This is what Forma draws text with.

Both use `GlyphAtlas` and the same layout engine. `TextRun` and `press` accept
`PressParams`; `repress` and `impress` take their update options separately.
Use the pressed path for a single standalone text buffer. Use glyph quads when
text is one of many elements sharing a buffer, or when it needs to be drawn in
a space other than the window (a world plane, a 3D scene).

---

## Coordinate system

Vulkan NDC has `(-1,-1)` at bottom-left and `(1,1)` at top-right, with `+Y`
pointing **down** in clip space. MayaFlux applies a negative-height viewport
flip in `RenderProcessor`, which inverts that. The result matches OpenGL/GLM:

```
(-1,  1) ─────────── ( 1,  1)   top
    │                    │
    │        (0, 0)       │   centre
    │                    │
(-1, -1) ─────────── ( 1, -1)   bottom
```

- `+Y` is **up** in MayaFlux NDC
- Window pixel space has `+Y` **down**, origin at top-left
- `set_position(x, y)` on a `TextBuffer` sets the **centre** of the quad in NDC
- `set_scale(w, h)` sets the full NDC extent. Base vertices sit at `±1`, so
  the default `scale = {1, 1}` spans the entire screen

---

## Coordinate conversion, the API functions

All conversion helpers are in `MayaFlux/API/Windowing.hpp` and accept either
raw dimensions or a `shared_ptr<Window>`.

### Pixel position → NDC position

```cpp
// top-left pixel (0,0) → NDC (-1, 1)
// centre pixel (960,540) on 1920×1080 → NDC (0, 0)
glm::vec3 ndc = normalize_coords(pixel_x, pixel_y, win_w, win_h);
glm::vec3 ndc = normalize_coords(pixel_x, pixel_y, window);
```

### NDC position → pixel position

```cpp
glm::vec2 px = window_coords(ndc_x, ndc_y, 0.0, win_w, win_h);
glm::vec2 px = window_coords(ndc_pos, window);
```

### NDC extent → pixel dimensions (for render_bounds)

```cpp
// how many pixels does an NDC region of size (0.4, 0.1) cover?
glm::uvec2 px = normalized_size_to_pixels({ 0.4f, 0.1f }, win_w, win_h);
glm::uvec2 px = normalized_size_to_pixels({ 0.4f, 0.1f }, window);
// also accepts an AABB2D directly
glm::uvec2 px = normalized_size_to_pixels(aabb, window);
```

---

## render_bounds vs. set_scale

`render_bounds` in `PressParams` is the **pixel size of the raster buffer**.
It controls where line wrapping happens and how tall the texture is.
It does not control how large the text appears on screen.

Glyphs are painted starting from the **top-left of that buffer** regardless of
where the quad is placed.

`set_scale(w, h)` sets the NDC extent of the quad. The unit quad base vertices
sit at `±1`, so the default `scale = {1, 1}` produces a fullscreen quad.

**If `render_bounds` is large and `set_scale` is large, the texture covers the
screen and the glyphs are visible only in its top-left corner.**

Rule: match `render_bounds` to the expected pixel footprint of the text, then
use `set_scale` to control display size.

A concrete illustration. With `render_bounds = {1920, 1080}` and default
`scale = {1, 1}`:

- the quad is fullscreen
- the texture is 1920×1080
- glyphs are painted into the top-left of that texture
- the text appears near the top-left of the screen with a small Y gap (the
  font's ascender offset from the texture top)

With `render_bounds = {400, 50}` and `scale = {ndc_w, ndc_h}` sized to match:

- the quad is tight around the text
- `set_position` moves the text predictably

---

## Placing text at a specific pixel position

`set_position` takes the **centre** of the quad in NDC, `set_scale` its full
NDC extent. Converting both corners of the target pixel rect gives you both
in one step, with no separate ratio math or sign-sensitive half-extent shift:

```cpp
uint32_t rw = 400, rh = 50;
uint32_t px = 20, py = 20; // top-left of the pixel rect

auto text_buf = Portal::Text::press("Hello", {
    .color = { 1.f, 1.f, 1.f, 1.f },
    .render_bounds = { rw, rh },
}) | Graphics;
text_buf->setup_rendering({ .target_window = window });

auto tl = normalize_coords(px, py, win_w, win_h);
auto br = normalize_coords(px + rw, py + rh, win_w, win_h);
text_buf->set_scale(br.x - tl.x, tl.y - br.y); // NDC +Y is up, so tl.y > br.y
text_buf->set_position((tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f);
```

Any pixel rect placement (top-left margin, bottom-left, right-aligned, ...)
is just a different `(px, py, rw, rh)` fed through the same two conversions -
screen centre alone needs neither, since `set_position(0.f, 0.f)` already is
NDC centre. `Portal::Forma::Context::to_ndc_rect` does this same two-corner
conversion for callers already inside a Forma `Surface`.

---

## Working with a window object

All of the above can use the `window` overloads directly when dimensions are
not known at compile time or when supporting resize:

```cpp
auto tl = normalize_coords(20.0, 20.0, window);
auto br = normalize_coords(20.0 + rw, 20.0 + rh, window);
```

---

## Moving text

Animate `set_position` from `schedule_metro`. The metro callback runs on the
audio thread. `set_position` only marks a dirty flag and is safe to call from
any thread:

```cpp
uint32_t rw = 400, rh = 50;
float ndc_w = (static_cast<float>(rw) / 1920.f) * 2.f;
float ndc_h = (static_cast<float>(rh) / 1080.f) * 2.f;

auto text_buf = Portal::Text::press("Hello, MayaFlux!", {
    .color = { 1.f, 0.5f, 0.25f, 1.f },
    .render_bounds = { rw, rh },
}) | Graphics;
text_buf->setup_rendering({ .target_window = window });
text_buf->set_scale(ndc_w, ndc_h);

auto& px = make_persistent(0.f);
auto& py = make_persistent(0.f);

schedule_metro(1 / 60.f, [text_buf, ndc_w, ndc_h, &px, &py]() {
    px += 2.f;
    py += 2.f;
    auto tl = normalize_coords(px, py, 1920u, 1080u);
    text_buf->set_position(tl.x + ndc_w * 0.5f,
                           tl.y - ndc_h * 0.5f);
});
```

---

## Updating text content

Use `repress` to replace the string without reallocation when the new content
fits within the original budget:

```cpp
Portal::Text::repress(text_buf, "Updated string", { 1.f, 1.f, 1.f, 1.f });
```

Use `impress` to append at the current cursor without clearing:

```cpp
Portal::Text::impress(text_buf, " appended", { 0.8f, 0.8f, 0.8f, 1.f });
```

`impress` returns `ImpressResult::Overflow` when the vertical budget is
exhausted. The texture is reallocated and all previous content is cleared.
Rebuild from `get_accumulated_text()` if continuity is required.

`repress` takes an optional `RedrawPolicy` (default `Clip`, which truncates
text that exceeds the existing budget; `Fit` reallocates the GPU texture
instead) and an optional `std::span<const StyleSpan>` for per-byte-range
color overrides:

```cpp
Portal::Text::repress(text_buf, "Updated string", { 1.f, 1.f, 1.f, 1.f },
    Portal::Text::RedrawPolicy::Fit);
```

`impress` takes the same `spans` argument, offset into the string passed to
that call (not the buffer's accumulated history).

---

## Background fill

`PressParams::background` (default fully transparent) is composited beneath
every glyph and persists on the `TextBuffer` across `repress()`/`impress()`
calls, so it only needs to be set once at `press()` time:

```cpp
auto text_buf = Portal::Text::press("Hello", {
    .color = { 1.f, 1.f, 1.f, 1.f },
    .background = { 0.1f, 0.1f, 0.1f, 0.8f },
}) | Graphics;
```

The `VKImage`-returning `press`/`repress` overloads have no buffer to persist
it on, so those read `background` fresh from `PressParams` every call.

---

## Per-range color styling

`StyleSpan { start, end, color }` overrides the base color for a byte range
of the string passed to that call (`start`/`end` are byte offsets, matching
`GlyphQuad::byte_offset`). Later entries win where spans overlap:

```cpp
Portal::Text::repress(text_buf, "ERROR: disk full", { 1.f, 1.f, 1.f, 1.f },
    Portal::Text::RedrawPolicy::Clip,
    { { .start = 0, .end = 5, .color = { 1.f, 0.2f, 0.2f, 1.f } } });
```

---

## Hit-testing and extraction

`create_layout()` returns a `LayoutResult` whose `GlyphQuad`s carry the byte
offset each quad was produced from. Three functions resolve between screen
position, byte offset, and substrings against that layout:

- `x_at(text, atlas, byte_offset, ...)` returns the pen position immediately
  before a byte offset. Used for caret placement.
- `index_at(text, layout, atlas, x, y, ...)` returns the nearest byte offset
  to a point in layout coordinates. `TextRun::index_at` wraps this for text
  drawn as glyph quads; Forma's `TextField` also checks empty lines when
  placing the cursor by click or Up/Down movement.
- `copy(text, layout, a, b)` extracts the substring and matching quads for a
  byte range, for selection or clipboard use.

All three take the same `pen_x`/`pen_y`/`wrap_w`/`tab_width` arguments the
original `lay_out()`/`create_layout()` call used, since the answer depends on
where wrapping happened.

```cpp
auto layout = Portal::Text::create_layout(text, 0.F, 0.F, wrap_w);
size_t byte_off = Portal::Text::index_at(text, *layout,
    Portal::Text::get_default_atlas(), click_x, click_y, 0.F, 0.F, wrap_w);
auto pen = Portal::Text::x_at(text, Portal::Text::get_default_atlas(), byte_off,
    0.F, 0.F, wrap_w);
```

---

## Editable text buffers

`EditableText { text; cursor; }` pairs a mutable UTF-8 string with a
byte-offset cursor that is always kept on a codepoint boundary. Operations
are built on `is_control()`/`encode_utf8()`/`next_codepoint_offset()`/
`previous_codepoint_offset()`, with no atlas or layout dependency:

```cpp
Portal::Text::EditableText edit { .text = "hello", .cursor = 5 };
Portal::Text::insert_codepoint(edit, U'!');   // "hello!" cursor=6
Portal::Text::move(edit, -3);                 // cursor=3
Portal::Text::erase(edit, -1);                // "helo!" cursor=2
```

`move`/`erase` take a signed codepoint count: positive moves/erases toward
the end, negative toward the start, clamped at whichever text boundary is
reached first. `insert_literal` takes a raw ASCII control byte (`\t`, `\n`)
for cases that never reach `WindowEventType::TEXT_INPUT` and so cannot go
through `insert_codepoint`.

Wiring `EditableText` up to keystrokes, a caret quad, click/wheel/Up-Down
input, and scroll clipping is exactly what `Portal::Forma::TextField` does -
see `Portal-Forma.md` for the ready-made widget instead of rebuilding this
by hand.

---

## press as VKImage

The second `press` overload returns a raw `VKImage` with no quad or
`TextBuffer` involved. Use this when a buffer needs text as an image texture.
Forma's built-in text elements use glyph quads instead:

```cpp
auto text_image = Portal::Text::press("label",
    { 256, 32 },
    { .color = { 1.f, 1.f, 1.f, 1.f } });

buf->setup_rendering({
    .target_window = window,
    .additional_textures = { { "text", text_image } },
});
```

---

## Drawing text as glyph quads

The image-returning `press` overload creates a texture for each call. For
many small strings (labels, rows, a text field with a caret), glyph quads draw
them as vertices that can share one atlas image.

Three types work together:

| Type | Job |
|---|---|
| `TextRun` | Owns a string, its `PressParams` and the layout. Emits vertices, answers caret and hit queries. |
| `GlyphMap` | A linear map from layout pixels (y down) to the space the vertices are drawn in. |
| `AtlasImage` | The GPU image of a `GlyphAtlas`. Keeps it current and reports when it was replaced. |

### TextRun

```cpp
Portal::Text::TextRun run({ .color = { 1.f, 1.f, 1.f, 1.f },
                            .render_bounds = { 256, 32 } });
run.set("Hello");
run.assign("Hello again", new_params);

glm::vec2 size = run.extent();
float height = run.content_height();
```

Layout starts from the same origin as the image-returning `press` overload:
x at zero, baseline at the atlas ascender, wrapping at `render_bounds.x`.
Of `PressParams` the run reads `atlas`, `color`, `spans`, `background` and
`render_bounds`. `budget_h` has no meaning here. A `background` with alpha
above zero is drawn as one
opaque flat quad over `extent()`, beneath the glyphs. Its alpha is not retained.

`extent()` is `render_bounds.x` wide and at least one line high. Fit your
destination region onto `extent()`, not onto `render_bounds`, so a row shorter
than a line shows its text squeezed to fit instead of spilling.

### Producing vertices

```cpp
auto map  = Portal::Text::GlyphMap::fit(region, run.extent());
auto verts = run.vertices(map, 0);
```

`vertices()` returns triangle-list `MeshVertex` data, six per quad. Colour
comes from the last `StyleSpan` covering a quad's byte offset, else the
`PressParams` colour (rgb only). If the atlas was replaced or grew since the
layout was built, `vertices()` lays out again first, so its output is always
current. `run.stale()` tells you when that would happen.

`GlyphMap` takes layout pixels to output space as `origin + pixel * scale`. Use
`GlyphMap::fit(region, source_px)` to place the run in a y-up region (pixel
(0,0) lands on the region's top left), or construct one directly for a world
plane or any other space.

### Caret and hit-testing

```cpp
Kinesis::AABB2D box = run.caret(byte_offset, 2.f);
size_t index        = run.index_at(pixel_in_layout_space);
```

Both work in layout pixels with y down, so `box.min.y` is the top of the
caret. `index_at` returns the byte offset of the codepoint boundary nearest
the point. Use `copy` separately to extract a text range.

### The atlas image

```cpp
Portal::Text::AtlasImage atlas_image;
auto image = atlas_image.sync(*run.atlas());
```

`sync` creates the image on first use, uploads in place when glyphs were
rasterized since the last call, and recreates it when the atlas is a different
one, a different size, or was released at texture-manager shutdown. It returns
null if the texture manager is not initialized.

Keep **one `AtlasImage` per atlas**: syncing consumes the atlas's dirty flag.

When the image object changes, `generation()` increments and the image must
be bound again wherever it was bound. If the atlas grew, its glyph UVs also
change, so vertices must be re-emitted. Forma coordinates this for its text
slices.

### Telling the shader a vertex is glyph coverage

A buffer's `MeshVertex::weight` normally selects a texture slot as an image.
Glyph vertices use a negative weight instead:

| Weight | Meaning |
|---|---|
| positive | texture slot sampled as an image |
| zero | flat colour |
| negative | slot `-weight - 1` read as glyph coverage |

```cpp
float w = Portal::Text::coverage_weight(slot);
```

For coverage the fragment shader outputs the vertex colour with the slot's red
channel as alpha. `forma_multi.frag` handles this through
`include/glyph_coverage.glsl` (`glyph_is_coverage`, `glyph_slot`), which your own
fragment shaders can include. A buffer that carries a texture slot reads vertex
weight as an image index or glyph coverage, so a shape batch that should not
sample textures must not use one (Forma's plot uses its own shader for this).

### In Forma

Text drawn by Forma elements uses this path. `Element::with_text`, `set_text`
and `retarget` draw a `TextRun` into a buffer slice, with its atlas image bound
at texture slot 0. Forma keeps one `AtlasImage` per atlas and refreshes stale
slices when that atlas grows. A buffer's texture slots are shared by all its
slices, so text slices in the same buffer must use the same atlas. Bind text
using different atlases in separate buffers, and reserve slot 0 for the atlas
when a buffer contains text. `Element::text_slice` holds the glyph-quad route
for text. See `Portal-Forma.md` for the slice and buffer model.

---

## budget_h

`budget_h` pre-allocates vertical texture space beyond the initial content
height. Set it when the text will grow via `impress`:

```cpp
Portal::Text::press("", {
    .render_bounds = { 600, 400 },
    .budget_h = 400,
});
```

When `budget_h` is zero, `press` applies an 8x growth heuristic over the
initial content height.
