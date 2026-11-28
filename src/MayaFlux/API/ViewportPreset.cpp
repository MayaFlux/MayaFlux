#include "ViewportPreset.hpp"

#include "Chronie.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux {

namespace {

    using ViewCell = std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>;
    using Processors = std::vector<std::shared_ptr<Buffers::RenderProcessor>>;

    using Region = std::optional<Kinesis::AABB2D>;

    using Shape = std::function<bool(glm::vec2)>;

    /** Presets stand below every Forma Context, which starts at 0. */
    constexpr int k_preset_priority = -1000;

    struct PresetRecord {
        Core::InputConfig saved_config;
        std::vector<std::string> registered_events;
        ViewCell view;
        uint32_t driver {};
        std::weak_ptr<Core::Window> window;
        uint32_t claim {};
        std::shared_ptr<Region> region;
        std::shared_ptr<Shape> shape;
    };

    std::unordered_map<std::string, PresetRecord> s_registry;

    std::string make_key(const std::shared_ptr<Core::Window>& window, const std::string& name)
    {
        return std::to_string(reinterpret_cast<uintptr_t>(window.get())) + "_" + name;
    }

    /** Cancel a record's handlers and stop driving, leaving the view where it stands. */
    void release(PresetRecord& record)
    {
        for (const auto& ev : record.registered_events) {
            cancel_event_handler(ev);
        }
        record.registered_events.clear();

        if (const auto window = record.window.lock(); window && record.claim != 0) {
            window->get_event_source().unclaim(record.claim);
        }
        record.claim = 0;

        if (record.view) {
            (void)record.view->read();
            record.view->leave(record.driver);
            record.view.reset();
        }
    }

    /**
     * Start the record of a preset on a window, ending whatever it still had bound.
     * The function returned names a handler, unique to the window, and records it.
     */
    auto open_record(const std::shared_ptr<Core::Window>& window, const std::string& name)
    {
        const std::string key = make_key(window, name);
        PresetRecord& record = s_registry[key];
        record.saved_config = window->get_input_config();
        release(record);

        return [&record, key](const char* suffix) {
            return record.registered_events.emplace_back("vp_" + key + "_" + suffix);
        };
    }

    /** Where a binding takes input from, and what it measures the pointer against. */
    struct Gate {
        std::weak_ptr<Core::Window> window;
        std::shared_ptr<Region> region;
        uint32_t claim {};

        [[nodiscard]] bool pointer() const
        {
            const auto live = window.lock();
            return live && live->get_event_source().owns_pointer(claim);
        }

        [[nodiscard]] bool key() const
        {
            const auto live = window.lock();
            return live && live->get_event_source().owns_key(claim);
        }

        /** Pixel size of the region, or of the window without one. */
        [[nodiscard]] glm::vec2 size() const
        {
            const auto live = window.lock();
            if (!live) {
                return glm::vec2(0.0F);
            }
            const auto& state = live->get_state();
            const glm::vec2 whole { static_cast<float>(state.current_width), static_cast<float>(state.current_height) };
            return *region ? whole * ((*region)->max - (*region)->min) * 0.5F : whole;
        }

        /** The pointer in the region's own NDC, or the window's without one. */
        [[nodiscard]] glm::vec2 cursor() const
        {
            const auto live = window.lock();
            if (!live) {
                return glm::vec2(0.0F);
            }
            const auto& state = live->get_state();
            const auto [px, py] = live->get_event_source().get_mouse_position();
            const glm::vec2 at(Kinesis::to_ndc(px, py, state.current_width, state.current_height));
            return *region ? (at - (*region)->min) / ((*region)->max - (*region)->min) * 2.0F - 1.0F : at;
        }
    };

    /** Claim the window for a binding that has opened its record. */
    Gate claim_input(
        const std::shared_ptr<Core::Window>& window,
        const std::string& name,
        std::vector<IO::Keys> keys)
    {
        auto region = std::make_shared<Region>();

        auto shape = std::make_shared<Shape>();

        const auto covers = [region, shape, weak = std::weak_ptr<Core::Window>(window)](
                                double px, double py) -> std::optional<float> {
            const auto live = weak.lock();
            if (!live) {
                return std::nullopt;
            }
            if (*region || *shape) {
                const auto& state = live->get_state();
                const glm::vec2 at(Kinesis::to_ndc(px, py, state.current_width, state.current_height));
                if ((*region && !(*region)->contains(at)) || (*shape && !(*shape)(at))) {
                    return std::nullopt;
                }
            }
            return std::numeric_limits<float>::max();
        };

        const auto wants = [keys = std::move(keys)](IO::Keys key) {
            return std::ranges::find(keys, key) != keys.end();
        };

        PresetRecord& record = s_registry[make_key(window, name)];
        record.window = window;
        record.region = region;
        record.shape = shape;
        record.claim = window->get_event_source().claim(k_preset_priority, covers, wants);

        return { .window = window, .region = region, .claim = record.claim };
    }

    std::vector<IO::Keys> keys_of(std::initializer_list<std::optional<IO::Keys>> keys)
    {
        std::vector<IO::Keys> named;
        for (const auto& key : keys) {
            if (key) {
                named.push_back(*key);
            }
        }
        return named;
    }

    /** Every buffer of the window that has a processor whose view is not pinned. */
    Processors processors_of(const std::shared_ptr<Core::Window>& window)
    {
        Processors out;
        for (const auto& buf : window->get_rendering_buffers()) {
            if (auto rp = buf->get_render_processor(); rp && !rp->view_pinned()) {
                out.push_back(std::move(rp));
            }
        }
        return out;
    }

    ViewportBinding draw_with(ViewportBinding bound, const Processors& processors)
    {
        for (const auto& processor : processors) {
            processor->set_view_cell(bound.first);
        }
        return bound;
    }

    /**
     * Drive a viewpoint, the given one or a new one holding @p initial. The
     * controller is handed the present value before it starts.
     */
    ViewportBinding take(
        const std::shared_ptr<Core::Window>& window,
        const std::string& name,
        ViewCell view,
        const Kinesis::Viewpoint& initial,
        std::function<void(const Kinesis::Viewpoint&)> resume,
        std::function<Kinesis::Viewpoint()> pull)
    {
        if (!view) {
            view = std::make_shared<Memory::OwnedCell<Kinesis::Viewpoint>>(initial);
        }

        const auto driver = view->join(std::move(resume), std::move(pull));
        view->hand_to(driver);

        PresetRecord& record = s_registry[make_key(window, name)];
        record.view = view;
        record.driver = driver;

        return { std::move(view), driver };
    }

    struct Drag {
        bool held {};
        bool first { true };
        double last_x {};
        double last_y {};

        /** Pixels moved since the last call while held. */
        std::optional<glm::vec2> advance(double x, double y)
        {
            if (!held) {
                first = true;
                return std::nullopt;
            }
            if (first) {
                last_x = x;
                last_y = y;
                first = false;
                return std::nullopt;
            }
            const glm::vec2 moved { static_cast<float>(x - last_x), static_cast<float>(y - last_y) };
            last_x = x;
            last_y = y;
            return moved;
        }
    };

    /** Hold a drag while a button pressed for this binding is down. */
    template <typename Named>
    void on_drag_button(
        const std::shared_ptr<Core::Window>& window,
        IO::MouseButtons button,
        const std::shared_ptr<Drag>& drag,
        const Gate& gate,
        const Named& named)
    {
        on_mouse_pressed(window, button, [drag, gate](double /*x*/, double /*y*/) {
            if (gate.pointer()) {
                drag->held = true;
                drag->first = true;
            } }, named("drag_dn"));

        on_mouse_released(window, button, [drag](double /*x*/, double /*y*/) { drag->held = false; }, named("drag_up"));
    }

    ViewportBinding install_fly(
        const std::shared_ptr<Core::Window>& window,
        const ViewCell& view,
        const ViewportPresetConfig& config,
        const Kinesis::FlyKeyMap& key_map,
        const std::string& name)
    {
        const auto named = open_record(window, name);

        auto st = std::make_shared<Kinesis::NavigationState>(
            Kinesis::make_navigation_state(config));

        const Gate gate = claim_input(window, name,
            keys_of({ key_map.forward, key_map.back, key_map.left, key_map.right, key_map.down, key_map.up,
                key_map.ortho_front, key_map.ortho_right, key_map.ortho_top, key_map.ortho_flip }));

        on_mouse_pressed(window, IO::MouseButtons::Right, [st, gate](double /*x*/, double /*y*/) {
            if (gate.pointer()) {
                st->rmb_held    = true;
                st->first_mouse = true;
            } }, named("rmb_dn"));

        on_mouse_released(window, IO::MouseButtons::Right, [st](double /*x*/, double /*y*/) { st->rmb_held = false; }, named("rmb_up"));

        const auto hold_key = [&](IO::Keys key, bool Kinesis::NavigationState::*held, const char* down, const char* up) {
            on_key_pressed(window, key, [st, held, gate] {
                if (gate.key()) {
                    (*st).*held = true;
                } }, named(down));
            on_key_released(window, key, [st, held] { (*st).*held = false; }, named(up));
        };

        hold_key(key_map.forward, &Kinesis::NavigationState::forward_held, "fwd_dn", "fwd_up");
        hold_key(key_map.back, &Kinesis::NavigationState::back_held, "bck_dn", "bck_up");
        hold_key(key_map.left, &Kinesis::NavigationState::left_held, "lft_dn", "lft_up");
        hold_key(key_map.right, &Kinesis::NavigationState::right_held, "rgt_dn", "rgt_up");
        hold_key(key_map.down, &Kinesis::NavigationState::down_held, "dwn_dn", "dwn_up");
        hold_key(key_map.up, &Kinesis::NavigationState::up_held, "up_dn", "up_up");

        const auto snap_key = [&](const std::optional<IO::Keys>& key, int which, const char* event) {
            if (!key) {
                return;
            }
            on_key_pressed(window, *key, [st, which, gate] {
                if (gate.key()) {
                    Kinesis::snap_ortho(*st, which);
                } }, named(event));
        };

        snap_key(key_map.ortho_front, 0, "kp_front");
        snap_key(key_map.ortho_right, 1, "kp_right");
        snap_key(key_map.ortho_top, 2, "kp_top");
        snap_key(key_map.ortho_flip, 3, "kp_flip");

        on_mouse_move(window, [st](double x, double y) {
            if (!st->rmb_held) {
                st->first_mouse = true;
                return;
            }
            if (st->first_mouse) {
                st->last_x      = x;
                st->last_y      = y;
                st->first_mouse = false;
                return;
            }
            Kinesis::apply_mouse_delta(*st,
                static_cast<float>(x - st->last_x),
                static_cast<float>(y - st->last_y));
            st->last_x = x;
            st->last_y = y; }, named("mouse"));

        on_scroll(window, [st, gate](double /*dx*/, double dy) {
            if (gate.pointer()) {
                Kinesis::apply_scroll(*st, static_cast<float>(dy));
            } }, named("scroll"));

        return take(window, name, view, Kinesis::viewpoint_of(*st),
            [st](const Kinesis::Viewpoint& current) { Kinesis::set_viewpoint(*st, current); },
            [st] {
                Kinesis::advance_navigation(*st);
                return Kinesis::viewpoint_of(*st);
            });
    }

    ViewportBinding install_orbit(
        const std::shared_ptr<Core::Window>& window,
        const ViewCell& view,
        const Kinesis::OrbitConfig& config,
        const Kinesis::OrbitKeyMap& key_map,
        const std::string& name)
    {
        const auto named = open_record(window, name);

        const glm::vec3 eye = config.focal_point
            + glm::vec3(std::cos(config.initial_elevation) * std::sin(config.initial_azimuth),
                  std::sin(config.initial_elevation),
                  std::cos(config.initial_elevation) * std::cos(config.initial_azimuth))
                * config.initial_distance;

        struct Orbit {
            Kinesis::Viewpoint view;
            glm::vec3 pivot;
        };

        auto st = std::make_shared<Orbit>(Orbit {
            .view = {
                glm::inverse(glm::lookAt(eye, config.focal_point, glm::vec3(0.0F, 1.0F, 0.0F))),
                Kinesis::frustum_from_fov(config.fov_radians, 1.5, config.near_plane, config.far_plane) },
            .pivot = config.focal_point });

        const Gate gate = claim_input(window, name,
            keys_of({ key_map.ortho_front, key_map.ortho_right, key_map.ortho_top, key_map.ortho_flip }));

        auto drag = std::make_shared<Drag>();
        on_drag_button(window, IO::MouseButtons::Middle, drag, gate, named);

        const IO::Keys pan_key = key_map.pan_modifier;
        const IO::Modifiers pan_flag = IO::modifier_of(pan_key);
        const std::weak_ptr<Core::Window> weak_window = window;

        on_mouse_move(window, [st, drag, weak_window, pan_key, pan_flag,
                                  rotate = config.rotate_sensitivity, pan = config.pan_sensitivity](double x, double y) {
            const auto moved = drag->advance(x, y);
            const auto live = weak_window.lock();
            if (!moved || !live) {
                return;
            }

            const auto& source = live->get_event_source();
            const bool panning = pan_flag != IO::Modifiers::None
                ? has_flag(source.modifiers(), pan_flag)
                : source.is_key_pressed(pan_key);

            if (!panning) {
                st->view = Kinesis::rotate_viewpoint_about(
                    st->view, st->pivot, moved->x * rotate, -moved->y * rotate);
                return;
            }

            const glm::vec3 before(st->view.first[3]);
            const float scale = glm::distance(before, st->pivot) * pan;
            st->view = Kinesis::translate_viewpoint(st->view, { -moved->x * scale, moved->y * scale, 0.0F });
            st->pivot += glm::vec3(st->view.first[3]) - before; }, named("mouse"));

        on_scroll(window, [st, gate, speed = config.scroll_speed,
                              nearest = config.min_distance, farthest = config.max_distance](double /*dx*/, double dy) {
            if (!gate.pointer()) {
                return;
            }

            const float distance = glm::distance(glm::vec3(st->view.first[3]), st->pivot);
            if (!(distance > 0.0F)) {
                return;
            }

            const float ratio = glm::clamp(
                distance * (1.0F - static_cast<float>(dy) * speed), nearest, farthest) / distance;
            st->view = Kinesis::translate_viewpoint_toward(st->view, st->pivot, 1.0F - ratio);

            if (st->view.second.orthographic) {
                const glm::vec2 size = gate.size();
                st->view = Kinesis::scale_viewpoint_window(st->view, ratio, glm::vec2(0.0F), size.x / size.y);
            } }, named("scroll"));

        const auto snap_key = [&](const std::optional<IO::Keys>& key, std::optional<glm::vec3> direction, const char* event) {
            if (!key) {
                return;
            }
            on_key_pressed(window, *key, [st, direction, gate] {
                if (gate.key()) {
                    st->view = Kinesis::align_viewpoint(st->view, st->pivot,
                        direction.value_or(glm::vec3(st->view.first[2])));
                } }, named(event));
        };

        snap_key(key_map.ortho_front, glm::vec3(0.0F, 0.0F, -1.0F), "kp_front");
        snap_key(key_map.ortho_right, glm::vec3(-1.0F, 0.0F, 0.0F), "kp_right");
        snap_key(key_map.ortho_top, glm::vec3(0.0F, -1.0F, 0.0F), "kp_top");
        snap_key(key_map.ortho_flip, std::nullopt, "kp_flip");

        return take(window, name, view, st->view,
            [st](const Kinesis::Viewpoint& current) {
                if (Kinesis::same_viewpoint(current, st->view)) {
                    return;
                }
                const float distance = glm::distance(glm::vec3(st->view.first[3]), st->pivot);
                st->view = current;
                st->pivot = glm::vec3(current.first[3])
                    - glm::normalize(glm::vec3(current.first[2])) * distance;
            },
            [st] { return st->view; });
    }

    ViewportBinding install_pan_zoom(
        const std::shared_ptr<Core::Window>& window,
        const ViewCell& view,
        const Kinesis::PanZoom2DConfig& config,
        const Kinesis::PanZoom2DKeyMap& key_map,
        const std::string& name)
    {
        const auto named = open_record(window, name);

        auto st = std::make_shared<Kinesis::Viewpoint>(
            glm::translate(glm::mat4(1.0F), glm::vec3(config.initial_pan, 0.0F)),
            Kinesis::Frustum {
                .window = { .min = glm::vec2(-config.initial_zoom), .max = glm::vec2(config.initial_zoom) },
                .near = config.near_plane,
                .far = config.far_plane,
                .orthographic = true,
                .fit = Kinesis::AspectFit::Height });

        const Gate gate = claim_input(window, name, {});

        auto drag = std::make_shared<Drag>();
        on_drag_button(window, key_map.drag_button, drag, gate, named);

        on_mouse_move(window, [st, drag, gate](double x, double y) {
            const auto moved = drag->advance(x, y);
            if (!moved) {
                return;
            }

            const glm::vec2 size = gate.size();
            if (!(size.x > 0.0F && size.y > 0.0F)) {
                return;
            }

            const float per_pixel = 2.0F
                / (Kinesis::frustum_projection(st->second, size.x / size.y)[1][1] * size.y);
            *st = Kinesis::translate_viewpoint(*st, { -moved->x * per_pixel, moved->y * per_pixel, 0.0F }); }, named("mouse"));

        on_scroll(window, [st, gate, speed = config.scroll_speed,
                              smallest = config.min_zoom, largest = config.max_zoom](double /*dx*/, double dy) {
            if (!gate.pointer()) {
                return;
            }

            const auto half = static_cast<float>(st->second.window.height()) * 0.5F;
            const glm::vec2 size = gate.size();
            if (!(half > 0.0F && size.x > 0.0F && size.y > 0.0F)) {
                return;
            }

            const float factor = glm::clamp(
                half * (1.0F - static_cast<float>(dy) * speed), smallest, largest) / half;
            *st = Kinesis::scale_viewpoint_window(*st, factor, gate.cursor(), size.x / size.y); }, named("scroll"));

        return take(window, name, view, *st,
            [st](const Kinesis::Viewpoint& current) {
                st->first = current.first;
                if (current.second.orthographic) {
                    st->second = current.second;
                }
            },
            [st] { return *st; });
    }

    ViewportBinding install_screenspace(
        const std::shared_ptr<Core::Window>& window,
        const ViewCell& view,
        const Kinesis::NavigationConfig& config,
        const Kinesis::ScreenspaceKeyMap& key_map,
        const std::string& name)
    {
        const auto named = open_record(window, name);

        auto st = std::make_shared<Kinesis::NavigationState>(
            Kinesis::make_navigation_state(config));

        const Gate gate = claim_input(window, name, {});

        auto drag = std::make_shared<Drag>();
        on_drag_button(window, key_map.drag_button, drag, gate, named);

        on_mouse_move(window, [st, drag](double x, double y) {
            if (const auto moved = drag->advance(x, y)) {
                const auto shifted = Kinesis::translate_viewpoint(Kinesis::viewpoint_of(*st),
                    { -moved->x * st->mouse_sensitivity, moved->y * st->mouse_sensitivity, 0.0F });
                st->eye = glm::vec3(shifted.first[3]);
            } }, named("mouse"));

        on_scroll(window, [st, gate](double /*dx*/, double dy) {
            if (gate.pointer()) {
                Kinesis::apply_scroll(*st, static_cast<float>(dy));
            } }, named("scroll"));

        return take(window, name, view, Kinesis::viewpoint_of(*st),
            [st](const Kinesis::Viewpoint& current) { Kinesis::set_viewpoint(*st, current); },
            [st] { return Kinesis::viewpoint_of(*st); });
    }

} // namespace

ViewportBinding bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    ViewportPresetMode mode,
    const std::string& name)
{
    switch (mode) {
    case ViewportPresetMode::Fly:
        return bind_fly_preset(window, processor, {}, {}, name);
    case ViewportPresetMode::Orbit:
        return bind_orbit_preset(window, processor, {}, {}, name);
    case ViewportPresetMode::PanZoom2D:
        return bind_pan_zoom_preset(window, processor, {}, {}, name);
    case ViewportPresetMode::Screenspace:
        return bind_screenspace_preset(window, processor, {}, {}, name);
    default:
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "ViewportPresetMode {} is not yet implemented",
            static_cast<int>(mode));
        return {};
    }
}

ViewportBinding bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    ViewportPresetMode mode,
    const std::string& name)
{
    switch (mode) {
    case ViewportPresetMode::Fly:
        return bind_fly_preset(window, {}, {}, name);
    case ViewportPresetMode::Orbit:
        return bind_orbit_preset(window, {}, {}, name);
    case ViewportPresetMode::PanZoom2D:
        return bind_pan_zoom_preset(window, {}, {}, name);
    case ViewportPresetMode::Screenspace:
        return bind_screenspace_preset(window, {}, {}, name);
    default:
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "ViewportPresetMode {} is not yet implemented",
            static_cast<int>(mode));
        return {};
    }
}

ViewportBinding bind_fly_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const ViewportPresetConfig& config,
    const Kinesis::FlyKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_fly(window, {}, config, key_map, name), { processor });
}

ViewportBinding bind_fly_preset(
    const std::shared_ptr<Core::Window>& window,
    const ViewportPresetConfig& config,
    const Kinesis::FlyKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_fly(window, {}, config, key_map, name), processors_of(window));
}

ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::OrbitConfig& config,
    const Kinesis::OrbitKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_orbit(window, {}, config, key_map, name), { processor });
}

ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::OrbitConfig& config,
    const Kinesis::OrbitKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_orbit(window, {}, config, key_map, name), processors_of(window));
}

ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::PanZoom2DConfig& config,
    const Kinesis::PanZoom2DKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_pan_zoom(window, {}, config, key_map, name), { processor });
}

ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::PanZoom2DConfig& config,
    const Kinesis::PanZoom2DKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_pan_zoom(window, {}, config, key_map, name), processors_of(window));
}

ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::NavigationConfig& config,
    const Kinesis::ScreenspaceKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_screenspace(window, {}, config, key_map, name), { processor });
}

ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::NavigationConfig& config,
    const Kinesis::ScreenspaceKeyMap& key_map,
    const std::string& name)
{
    return draw_with(install_screenspace(window, {}, config, key_map, name), processors_of(window));
}

ViewportBinding join_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view,
    ViewportPresetMode mode,
    const std::string& name)
{
    if (!view) {
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "join_viewport_preset needs a viewpoint to join");
        return {};
    }

    switch (mode) {
    case ViewportPresetMode::Fly:
        return install_fly(window, view, {}, {}, name);
    case ViewportPresetMode::Orbit:
        return install_orbit(window, view, {}, {}, name);
    case ViewportPresetMode::PanZoom2D:
        return install_pan_zoom(window, view, {}, {}, name);
    case ViewportPresetMode::Screenspace:
        return install_screenspace(window, view, {}, {}, name);
    default:
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "ViewportPresetMode {} is not yet implemented",
            static_cast<int>(mode));
        return {};
    }
}

void set_viewport_preset_region(
    const std::shared_ptr<Core::Window>& window,
    const std::optional<Kinesis::AABB2D>& region,
    const std::string& name)
{
    const auto it = s_registry.find(make_key(window, name));
    if (it == s_registry.end() || !it->second.region) {
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "set_viewport_preset_region: nothing is bound as '{}'", name);
        return;
    }

    if (region && !(region->max.x > region->min.x && region->max.y > region->min.y)) {
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "set_viewport_preset_region: the region has no size");
        return;
    }

    *it->second.region = region;
}

void set_viewport_preset_shape(
    const std::shared_ptr<Core::Window>& window,
    std::function<bool(glm::vec2)> within,
    const std::string& name)
{
    const auto it = s_registry.find(make_key(window, name));
    if (it == s_registry.end() || !it->second.shape) {
        MF_RT_ERROR(Journal::Component::API, Journal::Context::EventDispatch,
            "set_viewport_preset_shape: nothing is bound as '{}'", name);
        return;
    }

    *it->second.shape = std::move(within);
}

void raise_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name)
{
    const auto it = s_registry.find(make_key(window, name));
    if (it != s_registry.end() && it->second.claim != 0) {
        window->get_event_source().set_claim_priority(it->second.claim, k_preset_priority);
    }
}

void unbind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name)
{
    const auto it = s_registry.find(make_key(window, name));
    if (it == s_registry.end())
        return;

    window->set_input_config(it->second.saved_config);
    release(it->second);
    s_registry.erase(it);
}

} // namespace MayaFlux
