#include "ViewportPreset.hpp"

#include "Chronie.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Kinesis/NavigationState.hpp"

namespace MayaFlux {

namespace {

    struct PresetRecord {
        Core::InputConfig saved_config;
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

} // namespace

void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Buffers::RenderProcessor>& processor,
    const ViewportPresetConfig& config,
    const std::string& name)
{
    s_registry[make_key(window, name)] = PresetRecord { .saved_config = window->get_input_config() };

    auto st = std::make_shared<Kinesis::NavigationState>(
        Kinesis::make_navigation_state(config));

    on_mouse_pressed(window, IO::MouseButtons::Right, [st](double /*x*/, double /*y*/) {
        st->rmb_held    = true;
        st->first_mouse = true; }, event_name(name, "rmb_dn"));

    on_mouse_released(window, IO::MouseButtons::Right, [st](double /*x*/, double /*y*/) { st->rmb_held = false; }, event_name(name, "rmb_up"));

    on_key_pressed(window, IO::Keys::W, [st] { st->forward_held = true; }, event_name(name, "w_dn"));
    on_key_released(window, IO::Keys::W, [st] { st->forward_held = false; }, event_name(name, "w_up"));
    on_key_pressed(window, IO::Keys::S, [st] { st->back_held = true; }, event_name(name, "s_dn"));
    on_key_released(window, IO::Keys::S, [st] { st->back_held = false; }, event_name(name, "s_up"));
    on_key_pressed(window, IO::Keys::A, [st] { st->left_held = true; }, event_name(name, "a_dn"));
    on_key_released(window, IO::Keys::A, [st] { st->left_held = false; }, event_name(name, "a_up"));
    on_key_pressed(window, IO::Keys::D, [st] { st->right_held = true; }, event_name(name, "d_dn"));
    on_key_released(window, IO::Keys::D, [st] { st->right_held = false; }, event_name(name, "d_up"));
    on_key_pressed(window, IO::Keys::Q, [st] { st->down_held = true; }, event_name(name, "q_dn"));
    on_key_released(window, IO::Keys::Q, [st] { st->down_held = false; }, event_name(name, "q_up"));
    on_key_pressed(window, IO::Keys::E, [st] { st->up_held = true; }, event_name(name, "e_dn"));
    on_key_released(window, IO::Keys::E, [st] { st->up_held = false; }, event_name(name, "e_up"));

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

    on_key_pressed(window, IO::Keys::KP1, [st] { Kinesis::snap_ortho(*st, 0); }, event_name(name, "kp1"));
    on_key_pressed(window, IO::Keys::KP3, [st] { Kinesis::snap_ortho(*st, 1); }, event_name(name, "kp3"));
    on_key_pressed(window, IO::Keys::KP7, [st] { Kinesis::snap_ortho(*st, 2); }, event_name(name, "kp7"));
    on_key_pressed(window, IO::Keys::KP9, [st] { Kinesis::snap_ortho(*st, 3); }, event_name(name, "kp9"));

    processor->set_view_transform_source(
        [st, window_weak = std::weak_ptr<Core::Window>(window)]() -> Kinesis::ViewTransform {
            auto win = window_weak.lock();
            if (!win) {
                return {};
            }
            const auto& ws = win->get_state();
            const float aspect = (ws.current_height > 0)
                ? static_cast<float>(ws.current_width) / static_cast<float>(ws.current_height)
                : 1.0F;
            return Kinesis::compute_view_transform(*st, aspect);
        });
}

void bind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const ViewportPresetConfig& config,
    const std::string& name)
{
    for (const auto& buf : window->get_rendering_buffers()) {
        auto rp = buf->get_render_processor();
        if (!rp) {
            continue;
        }
        bind_viewport_preset(window, rp, config, name);
    }
}

void unbind_viewport_preset(
    const std::shared_ptr<Core::Window>& window,
    const std::string& name)
{
    const std::string key = make_key(window, name);
    auto it = s_registry.find(key);
    if (it == s_registry.end()) {
        return;
    }
    const Core::InputConfig saved = it->second.saved_config;
    s_registry.erase(it);

    window->set_input_config(saved);

    static const char* const k_suffixes[] = {
        "w_dn", "w_up", "s_dn", "s_up",
        "a_dn", "a_up", "d_dn", "d_up",
        "q_dn", "q_up", "e_dn", "e_up",
        "rmb_dn", "rmb_up",
        "mouse", "scroll",
        "kp1", "kp3", "kp7", "kp9"
    };

    for (const char* sfx : k_suffixes) {
        cancel_event_handler(event_name(name, sfx));
    }
}

} // namespace MayaFlux
