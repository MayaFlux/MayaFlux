# ViewTransform

MayaFlux has no camera object. `ViewTransform` is two `glm::mat4` fields, 128 bytes, uploaded as a push constant every frame. The vertex shader reads `view` and `projection`. That is the complete contract.

---

## The coordinate system

World space in MayaFlux is right-handed with Y up:

- `+X` points right
- `+Y` points up
- `+Z` points toward the viewer (out of the screen when facing the default direction)

Geometry at the origin is at `{0, 0, 0}`. A camera placed at `{0, 0, 5}` looking toward the origin sits 5 units in front of it along `+Z`. This is the natural starting position for anything placed at or near the origin.

---

## eye, target, up

`look_at_perspective` takes three positional arguments before the projection parameters:

```cpp
Kinesis::look_at_perspective(eye, target, fov, aspect, near, far, up)
```

`eye` is where the viewpoint sits in world space. `target` is the point it looks at. The view direction is `normalize(target - eye)`.

`up` defaults to `{0, 1, 0}`. It must not be parallel to the view direction. The one case where it is: a camera placed exactly above the origin looking straight down has view direction `{0, -1, 0}`, which is parallel to the default up vector `{0, 1, 0}`. The result is a degenerate view matrix and nothing renders. Pass `{0, 0, 1}` or `{1, 0, 0}` as the explicit up in that case.

---

## FOV, aspect, near, far

`fov_radians` is the vertical angle of the frustum. 60 degrees (`glm::radians(60.0F)`) is a natural starting point. Lower values compress the scene and flatten depth. Higher values exaggerate perspective and make nearby geometry appear larger relative to distant geometry.

`aspect` is width divided by height. For a 1920x1080 window: `1920.0F / 1080.0F`. Getting this wrong stretches or squashes everything horizontally.

`near` and `far` define the depth range that the GPU resolves. Anything closer than `near` or further than `far` is clipped and does not render. The ratio `far / near` determines depth precision: a ratio of 10000 (near=0.1, far=1000) causes z-fighting on geometry that is close together in depth. Keep `near` as large as the scene allows. `0.1` works for most scenes where geometry starts no closer than 0.1 world units from the eye. Use `0.01` only when geometry is very close.

---

## Placing the camera to see your geometry

The most common failure mode is geometry that exists in the scene but is outside the frustum or behind the camera. Before reaching for `bind_viewport_preset`, reason through where things are:

If geometry is at the origin, a camera at `{0, 2, 6}` looking at `{0, 0, 0}` is 6 units away and slightly elevated. That angle sees the origin from above and slightly in front, which works for most static scenes.

```cpp
// Geometry near origin, camera slightly elevated and back
Kinesis::look_at_perspective(
    { 0.0F, 2.0F, 6.0F },
    { 0.0F, 0.0F, 0.0F },
    glm::radians(60.0F), 1920.0F / 1080.0F, 0.1F, 100.0F)
```

If geometry spans a large area (a grid, a parametric surface), the eye needs to be far enough back that the full extent fits in the FOV. At FOV 60 degrees and distance `d`, the vertical extent visible is `2 * d * tan(30 degrees)` which is roughly `d * 1.15`. A 10-unit grid needs the eye at least 9 units back to fit vertically.

A flat XZ plane (the default `generate_grid` orientation) viewed with the eye at the same Y is edge-on and invisible. The eye must be above it and angled down:

```cpp
// Floor grid at Y=0, camera above and angled down
Kinesis::look_at_perspective(
    { 0.0F, 4.0F, 6.0F },
    { 0.0F, 0.0F, 0.0F },
    glm::radians(60.0F), 1920.0F / 1080.0F, 0.1F, 100.0F)
```

A camera at `{0, 4, 6}` looking at origin sits at roughly 34 degrees of downward tilt, which gives a natural floor-level view. Pushing the eye further back and higher flattens the angle and shows more of the floor.

---

## Aligning geometry to the camera

`generate_grid` accepts a `normal` parameter. The default is `{0, 1, 0}`, which produces a horizontal floor. To make the grid face the camera directly, pass the direction from grid center toward the eye as the normal:

```cpp
// Grid facing camera at {0, 0, 6}
auto grid = Kinesis::generate_grid(
    { 0.0F, 0.0F, 0.0F }, 4.0F, 4.0F, 16, 16,
    glm::vec3(0.0F, 0.0F, 1.0F));   // face toward +Z where camera is
```

This is useful for backgrounds, projection screens, or any surface that should always face a fixed viewpoint. For a surface that needs to face a dynamic camera, regenerate the grid each frame with the updated normal, or use `generate_parametric_surface` which computes normals from the surface function and handles this automatically for closed surfaces.

---

## Multiple buffers, one viewpoint

Every `RenderProcessor` that should share a viewpoint takes the same callable:

```cpp
auto view_source = []() -> Kinesis::ViewTransform {
    return Kinesis::look_at_perspective(
        { 0.0F, 2.0F, 6.0F }, { 0.0F, 0.0F, 0.0F },
        glm::radians(60.0F), 1920.0F / 1080.0F, 0.1F, 100.0F);
};

buf_a->get_render_processor()->set_view_transform_source(view_source);
buf_b->get_render_processor()->set_view_transform_source(view_source);
buf_c->get_render_processor()->set_view_transform_source(view_source);
```

If the callable closes over mutable state (a `NavigationState`, a Nexus position, a node output), all buffers that share it move together.

---

## Interactive navigation: bind_viewport_preset

`bind_viewport_preset` installs a `Fly` navigation controller on a window. It wires keyboard and mouse input to a `NavigationState` and drives `set_view_transform_source` automatically.

```cpp
bind_viewport_preset(wn, buf->get_render_processor());
```

To drive all buffers registered to the window at once:

```cpp
bind_viewport_preset(wn);
```

Controls installed by `Fly` mode:

- W / S / A / D: forward, backward, strafe left, strafe right
- Q / E: move down, move up along world Y
- Right mouse drag: yaw and pitch
- Scroll: dolly along view direction
- KP1 / KP3 / KP7 / KP9: front / right / top / flip ortho snaps

`NavigationConfig` tunes the initial position and feel:

```cpp
bind_viewport_preset(wn, buf->get_render_processor(),
    ViewportPresetMode::Fly,
    {
        .initial_eye    = { 0.0F, 2.0F, 6.0F },
        .initial_target = { 0.0F, 0.0F, 0.0F },
        .fov_radians    = glm::radians(60.0F),
        .move_speed     = 5.0F,
    });
```

To stop driving:

```cpp
unbind_viewport_preset(wn);
```

---

## Nexus: Locus

`Locus` is a `Nexus::Agent` whose position in the spatial graph tracks a `NavigationState`. It drives one or more `RenderProcessor` view transforms from the spatial graph, so the viewpoint participates in proximity queries and causal loops.

```cpp
auto locus = std::make_shared<Nexus::Locus>(
    Kinesis::NavigationConfig {
        .initial_eye    = { 0.0F, 1.0F, 4.0F },
        .initial_target = { 0.0F, 0.0F, 0.0F },
    },
    5.0F,
    [](const Nexus::PerceptionContext&) {},
    [](const Nexus::InfluenceContext&)  {});

locus->add_view_target(buf->get_render_processor());
locus->set_aspect(1920.0F / 1080.0F);
fabric->wire(locus).every(1.0 / 60.0).finalise();
```

Movement flags are set the same way as any other Nexus emitter binding:

```cpp
fabric->wire(std::make_shared<Nexus::Emitter>(
    [locus](const Nexus::InfluenceContext&) { locus->nav().forward_held = true; }))
    .on(wn, IO::Keys::W, true).finalise();
```

---

## Audio-driven viewpoint

`set_view_transform_source` accepts any callable. Closing over node outputs makes the viewpoint a live signal. A `Phasor` drives the orbit angle; a low-frequency `Sine` modulates the `Phasor` rate so the orbit speed breathes:

```cpp
auto lfo     = vega.Sine(0.05F, 0.08F) | Audio;
auto azimuth = vega.Phasor(lfo, 0.1F) | Audio;

buf->get_render_processor()->set_view_transform_source(
    [azimuth]() -> Kinesis::ViewTransform {
        const float a = static_cast<float>(azimuth->get_last_output()) * glm::two_pi<float>();
        const glm::vec3 eye { std::sin(a) * 5.0F, 1.5F, std::cos(a) * 5.0F };
        return Kinesis::look_at_perspective(
            eye, glm::vec3(0.0F),
            glm::radians(60.0F), 1920.0F / 1080.0F, 0.1F, 100.0F);
    });
```
