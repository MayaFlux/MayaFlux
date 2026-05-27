# Portal::Text

Portal::Text composites UTF-8 strings into GPU-resident textures that the buffer
pipeline renders as textured quads.

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

`set_position` takes the **centre** of the quad in NDC. To place the
**top-left corner** of the text at pixel `(px, py)`:

```cpp
// 1. decide the pixel footprint of the text
uint32_t rw = 400, rh = 50;

// 2. compute NDC scale from pixel footprint
glm::vec2 ndc_scale = {
    (static_cast<float>(rw) / static_cast<float>(win_w)) * 2.f,
    (static_cast<float>(rh) / static_cast<float>(win_h)) * 2.f,
};

// 3. press with tight render_bounds
auto text_buf = Portal::Text::press("Hello", {
    .color = { 1.f, 1.f, 1.f, 1.f },
    .render_bounds = { rw, rh },
}) | Graphics;
text_buf->setup_rendering({ .target_window = window });
text_buf->set_scale(ndc_scale.x, ndc_scale.y);

// 4. convert the desired top-left pixel to NDC, then shift by half-extent
//    to get the quad centre (NDC +Y is up, so half-extent subtracts in Y)
auto tl = normalize_coords(px, py, win_w, win_h);
text_buf->set_position(tl.x + ndc_scale.x * 0.5f,
                       tl.y - ndc_scale.y * 0.5f);
```

### Common placements

```cpp
// top-left of screen with 20px margin
auto tl = normalize_coords(20, 20, win_w, win_h);
text_buf->set_position(tl.x + ndc_scale.x * 0.5f,
                       tl.y - ndc_scale.y * 0.5f);

// screen centre
text_buf->set_position(0.f, 0.f);

// bottom-left with 20px margin (bottom in pixels = win_h - rh - 20)
auto bl = normalize_coords(20, win_h - rh - 20, win_w, win_h);
text_buf->set_position(bl.x + ndc_scale.x * 0.5f,
                       bl.y - ndc_scale.y * 0.5f);

// right-aligned, 20px from right edge
auto tr = normalize_coords(win_w - rw - 20, 20, win_w, win_h);
text_buf->set_position(tr.x + ndc_scale.x * 0.5f,
                       tr.y - ndc_scale.y * 0.5f);
```

---

## Working with a window object

All of the above can use the `window` overloads directly when dimensions are
not known at compile time or when supporting resize:

```cpp
auto tl = normalize_coords(20.0, 20.0, window);
auto px_dims = normalized_size_to_pixels({ ndc_scale.x, ndc_scale.y }, window);
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

---

## press as VKImage (for FormaBuffer)

The second `press` overload returns a raw `VKImage` with no quad or
`TextBuffer` involved. Use this when text is a texture input to a
`FormaBuffer`:

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
