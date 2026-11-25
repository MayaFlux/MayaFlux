#include "ViewportPreset.hpp"

#include "Chronie.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux {

namespace {

    struct PresetRecord {
        Core::InputConfig saved_config;
        std::vector<std::string> registered_events;
    };

    std::unordered_map<std::string, PresetRecord> s_registry;

    std::string make_key(const std::shared_ptr<Core::Window>& window, const std::string& name)
    {
        return std::to_string(reinterpret_cast<uintptr_t>(window.get())) + "_" + name;
    }

    std::string event_name(const std::string& prefix, const char* suffix)
    {
        return "vp_" + prefix + "_" + suffix;
    }

    using Processors = std::vector<std::shared_ptr<Buffers::RenderProcessor>>;

    Processors processors_of(const std::shared_ptr<Core::Window>& window)
    {
        Processors out;
        for (const auto& buf : window->get_rendering_buffers()) {
            if (auto rp = buf->get_render_processor()) {
                out.push_back(std::move(rp));
            }
        }
        return out;
    }

    /** One viewpoint for every processor, driven by a controller's state. */
    template <typename State, typename Advance>
    ViewportBinding hold(const std::shared_ptr<State>& state, const Processors& processors, Advance advance)
    {
        auto view = std::make_shared<Memory::OwnedCell<Kinesis::Viewpoint>>(Kinesis::viewpoint_of(*state));
        const auto driver = view->join(
            [state](const Kinesis::Viewpoint& current) { Kinesis::set_viewpoint(*state, current); },
            [state, advance] {
                advance(*state);
                return Kinesis::viewpoint_of(*state);
            });
        view->hand_to(driver);
        for (const auto& processor : processors) {
            processor->set_view_cell(view);
        }
        return { std::move(view), driver };
    }

    ViewportBinding install_fly(
        const std::shared_ptr<Core::Window>& window,
        const Processors& processors,
        const ViewportPresetConfig& config,
        const Kinesis::FlyKeyMap& key_map,
        const std::string& name)
    {
        auto& record = s_registry[make_key(window, name)];
        record.saved_config = window->get_input_config();
        record.registered_events.clear();

        auto st = std::make_shared<Kinesis::NavigationState>(
            Kinesis::make_navigation_state(config));

        on_mouse_pressed(window, IO::MouseButtons::Right, [st](double /*x*/, double /*y*/) {
            st->rmb_held    = true;
            st->first_mouse = true; }, event_name(name, "rmb_dn"));

        on_mouse_released(window, IO::MouseButtons::Right, [st](double /*x*/, double /*y*/) { st->rmb_held = false; }, event_name(name, "rmb_up"));

        on_key_pressed(window, key_map.forward, [st] { st->forward_held = true; }, event_name(name, "fwd_dn"));
        on_key_released(window, key_map.forward, [st] { st->forward_held = false; }, event_name(name, "fwd_up"));
        on_key_pressed(window, key_map.back, [st] { st->back_held = true; }, event_name(name, "bck_dn"));
        on_key_released(window, key_map.back, [st] { st->back_held = false; }, event_name(name, "bck_up"));
        on_key_pressed(window, key_map.left, [st] { st->left_held = true; }, event_name(name, "lft_dn"));
        on_key_released(window, key_map.left, [st] { st->left_held = false; }, event_name(name, "lft_up"));
        on_key_pressed(window, key_map.right, [st] { st->right_held = true; }, event_name(name, "rgt_dn"));
        on_key_released(window, key_map.right, [st] { st->right_held = false; }, event_name(name, "rgt_up"));
        on_key_pressed(window, key_map.down, [st] { st->down_held = true; }, event_name(name, "dwn_dn"));
        on_key_released(window, key_map.down, [st] { st->down_held = false; }, event_name(name, "dwn_up"));
        on_key_pressed(window, key_map.up, [st] { st->up_held = true; }, event_name(name, "up_dn"));
        on_key_released(window, key_map.up, [st] { st->up_held = false; }, event_name(name, "up_up"));

        if (key_map.ortho_front)
            on_key_pressed(window, *key_map.ortho_front, [st] { Kinesis::snap_ortho(*st, 0); }, event_name(name, "kp_front"));
        if (key_map.ortho_right)
            on_key_pressed(window, *key_map.ortho_right, [st] { Kinesis::snap_ortho(*st, 1); }, event_name(name, "kp_right"));
        if (key_map.ortho_top)
            on_key_pressed(window, *key_map.ortho_top, [st] { Kinesis::snap_ortho(*st, 2); }, event_name(name, "kp_top"));
        if (key_map.ortho_flip)
            on_key_pressed(window, *key_map.ortho_flip, [st] { Kinesis::snap_ortho(*st, 3); }, event_name(name, "kp_flip"));

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
            st->last_y = y; }, event_name(name, "mouse"));

        on_scroll(window, [st](double /*dx*/, double dy) { Kinesis::apply_scroll(*st, static_cast<float>(dy)); }, event_name(name, "scroll"));

        return hold(st, processors, [](Kinesis::NavigationState& state) { Kinesis::advance_navigation(state); });
    }

    ViewportBinding install_orbit(
        const std::shared_ptr<Core::Window>& window,
        const Processors& processors,
        const Kinesis::OrbitConfig& config,
        const Kinesis::OrbitKeyMap& key_map,
        const std::string& name)
    {
        auto& record = s_registry[make_key(window, name)];
        record.saved_config = window->get_input_config();
        record.registered_events.clear();

        auto st = std::make_shared<Kinesis::OrbitState>(Kinesis::make_orbit_state(config));

        on_mouse_pressed(window, IO::MouseButtons::Middle, [st](double /*x*/, double /*y*/) {
            st->mmb_held    = true;
            st->first_mouse = true; }, event_name(name, "mmb_dn"));

        on_mouse_released(window, IO::MouseButtons::Middle, [st](double /*x*/, double /*y*/) { st->mmb_held = false; }, event_name(name, "mmb_up"));

        const IO::Keys pan_key = key_map.pan_modifier;
        const IO::Modifiers pan_flag = IO::modifier_of(pan_key);
        const std::weak_ptr<Core::Window> weak_window = window;

        on_mouse_move(window, [st, weak_window, pan_key, pan_flag](double x, double y) {
            if (!st->mmb_held) {
                st->first_mouse = true;
                return;
            }
            if (st->first_mouse) {
                st->last_x      = x;
                st->last_y      = y;
                st->first_mouse = false;
                return;
            }
            const auto  dx = static_cast<float>(x - st->last_x);
            const auto  dy = static_cast<float>(y - st->last_y);
            st->last_x = x;
            st->last_y = y;

            if (const auto live_window = weak_window.lock()) {
                const auto& source = live_window->get_event_source();
                st->pan_held = pan_flag != IO::Modifiers::None
                    ? has_flag(source.modifiers(), pan_flag)
                    : source.is_key_pressed(pan_key);
            }

            if (st->pan_held) {
                Kinesis::apply_orbit_pan(*st, dx, dy);
            } else {
                Kinesis::apply_orbit_rotate(*st, dx, dy); 
    } }, event_name(name, "mouse"));

        on_scroll(window, [st](double /*dx*/, double dy) { Kinesis::apply_orbit_scroll(*st, static_cast<float>(dy)); }, event_name(name, "scroll"));

        if (key_map.ortho_front)
            on_key_pressed(window, *key_map.ortho_front, [st] { Kinesis::snap_orbit_ortho(*st, 0); }, event_name(name, "kp_front"));
        if (key_map.ortho_right)
            on_key_pressed(window, *key_map.ortho_right, [st] { Kinesis::snap_orbit_ortho(*st, 1); }, event_name(name, "kp_right"));
        if (key_map.ortho_top)
            on_key_pressed(window, *key_map.ortho_top, [st] { Kinesis::snap_orbit_ortho(*st, 2); }, event_name(name, "kp_top"));
        if (key_map.ortho_flip)
            on_key_pressed(window, *key_map.ortho_flip, [st] { Kinesis::snap_orbit_ortho(*st, 3); }, event_name(name, "kp_flip"));

        return hold(st, processors, [](auto&) { });
    }

    ViewportBinding install_pan_zoom(
        const std::shared_ptr<Core::Window>& window,
        const Processors& processors,
        const Kinesis::PanZoom2DConfig& config,
        const Kinesis::PanZoom2DKeyMap& key_map,
        const std::string& name)
    {
        auto& record = s_registry[make_key(window, name)];
        record.saved_config = window->get_input_config();
        record.registered_events.clear();

        auto st = std::make_shared<Kinesis::PanZoom2DState>(Kinesis::make_pan_zoom_state(config));

        on_mouse_pressed(window, key_map.drag_button, [st](double /*x*/, double /*y*/) {
            st->drag_held    = true;
            st->first_mouse  = true; }, event_name(name, "drag_dn"));

        on_mouse_released(window, key_map.drag_button, [st](double /*x*/, double /*y*/) { st->drag_held = false; }, event_name(name, "drag_up"));

        on_mouse_move(window, [st, window_weak = std::weak_ptr<Core::Window>(window)](double x, double y) {
            if (!st->drag_held) {
                st->first_mouse = true;
                return;
            }
            if (st->first_mouse) {
                st->last_x      = x;
                st->last_y      = y;
                st->first_mouse = false;
                return;
            }
            const auto  dx = static_cast<float>(x - st->last_x);
            const auto  dy = static_cast<float>(y - st->last_y);
            st->last_x = x;
            st->last_y = y;

            auto win = window_weak.lock();
            if (!win)
                return;
            const auto& ws = win->get_state();
            if (ws.current_width > 0 && ws.current_height > 0) {
                Kinesis::apply_pan_zoom_pan(*st, dx, dy,
                    static_cast<float>(ws.current_width),
                    static_cast<float>(ws.current_height));
            } }, event_name(name, "mouse"));

        on_scroll(window, [st](double /*dx*/, double dy) { Kinesis::apply_pan_zoom_scroll(*st, static_cast<float>(dy)); }, event_name(name, "scroll"));

        return hold(st, processors, [](auto&) { });
    }

    ViewportBinding install_screenspace(
        const std::shared_ptr<Core::Window>& window,
        const Processors& processors,
        const Kinesis::NavigationConfig& config,
        const Kinesis::ScreenspaceKeyMap& key_map,
        const std::string& name)
    {
        auto& record = s_registry[make_key(window, name)];
        record.saved_config = window->get_input_config();
        record.registered_events.clear();

        auto st = std::make_shared<Kinesis::NavigationState>(
            Kinesis::make_navigation_state(config));

        on_mouse_pressed(window, key_map.drag_button, [st](double /*x*/, double /*y*/) {
            st->rmb_held    = true;
            st->first_mouse = true; }, event_name(name, "drag_dn"));

        on_mouse_released(window, key_map.drag_button, [st](double /*x*/, double /*y*/) { st->rmb_held = false; }, event_name(name, "drag_up"));

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
            const auto  dx = static_cast<float>(x - st->last_x);
            const auto  dy = static_cast<float>(y - st->last_y);
            st->last_x = x;
            st->last_y = y;

            const glm::vec3 forward {
                std::cos(st->pitch) * std::sin(st->yaw),
                std::sin(st->pitch),
                std::cos(st->pitch) * std::cos(st->yaw)
            };
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
            const glm::vec3 up    = glm::normalize(glm::cross(right, forward));

            st->eye -= right * (dx * st->mouse_sensitivity);
            st->eye += up    * (dy * st->mouse_sensitivity); }, event_name(name, "mouse"));

        on_scroll(window, [st](double /*dx*/, double dy) { Kinesis::apply_scroll(*st, static_cast<float>(dy)); }, event_name(name, "scroll"));

        return hold(st, processors, [](auto&) { });
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
    return install_fly(window, { processor }, config, key_map, name);
}

ViewportBinding bind_fly_preset(
    const std::shared_ptr<Core::Window>& window,
    const ViewportPresetConfig& config,
    const Kinesis::FlyKeyMap& key_map,
    const std::string& name)
{
    return install_fly(window, processors_of(window), config, key_map, name);
}

ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::OrbitConfig& config,
    const Kinesis::OrbitKeyMap& key_map,
    const std::string& name)
{
    return install_orbit(window, { processor }, config, key_map, name);
}

ViewportBinding bind_orbit_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::OrbitConfig& config,
    const Kinesis::OrbitKeyMap& key_map,
    const std::string& name)
{
    return install_orbit(window, processors_of(window), config, key_map, name);
}

ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::PanZoom2DConfig& config,
    const Kinesis::PanZoom2DKeyMap& key_map,
    const std::string& name)
{
    return install_pan_zoom(window, { processor }, config, key_map, name);
}

ViewportBinding bind_pan_zoom_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::PanZoom2DConfig& config,
    const Kinesis::PanZoom2DKeyMap& key_map,
    const std::string& name)
{
    return install_pan_zoom(window, processors_of(window), config, key_map, name);
}

ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const Kinesis::NavigationConfig& config,
    const Kinesis::ScreenspaceKeyMap& key_map,
    const std::string& name)
{
    return install_screenspace(window, { processor }, config, key_map, name);
}

ViewportBinding bind_screenspace_preset(
    const std::shared_ptr<Core::Window>& window,
    const Kinesis::NavigationConfig& config,
    const Kinesis::ScreenspaceKeyMap& key_map,
    const std::string& name)
{
    return install_screenspace(window, processors_of(window), config, key_map, name);
}

void unbind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name)
{
    const std::string key = make_key(window, name);
    auto it = s_registry.find(key);
    if (it == s_registry.end())
        return;

    window->set_input_config(it->second.saved_config);

    for (const auto& ev : it->second.registered_events)
        cancel_event_handler(ev);

    s_registry.erase(it);
}

} // namespace MayaFlux
