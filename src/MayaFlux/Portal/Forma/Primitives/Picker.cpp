#include "Picker.hpp"

#include "MayaFlux/Portal/Forma/Internal/Atelier.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    constexpr uint32_t k_max_visible_rows = 10;
    constexpr uint32_t k_width_px = 520;
    constexpr uint32_t k_min_row_px = 48;
    constexpr uint32_t k_row_padding_px = 16;
    constexpr size_t k_row_buffer_capacity = static_cast<size_t>(6) * sizeof(Kakshya::MeshVertex);
    glm::vec4 idle_color() { return { 0.18F, 0.18F, 0.20F, 1.F }; }
    glm::vec4 hover_color() { return { 0.32F, 0.32F, 0.36F, 1.F }; }

    struct PickerState {
        PickCallback callback;
        std::weak_ptr<Core::Window> window;
        bool done {};

        void finish(std::optional<size_t> index)
        {
            if (done) {
                return;
            }
            done = true;

            if (index) {
                if (auto w = window.lock()) {
                    w->hide();
                }
            }
            if (callback) {
                callback(index);
            }
        }
    };

} // namespace

void pick(
    std::string title,
    std::vector<std::string> labels,
    PickCallback callback)
{
    if (labels.empty()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "Forma::pick: empty list");
        if (callback) {
            callback(std::nullopt);
        }
        return;
    }

    uint32_t row_px = k_min_row_px;
    if (auto atlas = Text::TypeFaceFoundry::instance().get_default_glyph_atlas()) {
        row_px = std::max(row_px, static_cast<uint32_t>(atlas->pixel_size()) + k_row_padding_px);
    }

    const auto rows = static_cast<uint32_t>(labels.size());
    const uint32_t visible = std::min(rows, k_max_visible_rows);
    const bool scrolling = rows > visible;
    const uint32_t window_h = row_px * visible + (scrolling ? row_px / 2 : 0);

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(Core::WindowCreateInfo {
        .title = std::move(title),
        .width = k_width_px,
        .height = window_h,
        .resizable = false,
    });

    auto state = std::make_shared<PickerState>();
    state->callback = std::move(callback);
    state->window = window;

    Surface surface = atelier.create_surface(SurfaceConfig {
        .window = window,
        .name = window->get_create_info().title,
        .on_close = [state] { state->finish(std::nullopt); },
    });

    std::optional<Scrollable> panel;
    std::function<void(uint32_t, glm::vec2, double, double)> wheel;
    if (scrolling) {
        auto panel_buf = atelier.create_buffer(
            window,
            internal::k_capacity_bytes,
            Graphics::PrimitiveTopology::TRIANGLE_STRIP);
        panel = make_scrollable(std::move(panel_buf), surface, surface.full(), glm::vec3(idle_color()));
        wheel = panel->wheel_handler(surface);
    }

    const float row_h = 2.F * static_cast<float>(row_px) / static_cast<float>(window_h);
    const float gap = 2.F / static_cast<float>(window_h);

    for (uint32_t i = 0; i < rows; ++i) {
        const float top = 1.F - row_h * static_cast<float>(i);
        const Kinesis::AABB2D bounds {
            .min = { -1.F, top - row_h + gap },
            .max = { 1.F, top - gap },
        };

        auto buf = atelier.create_buffer(
            window,
            k_row_buffer_capacity,
            Graphics::PrimitiveTopology::TRIANGLE_LIST,
            {},
            { { "text", nullptr } });

        const Text::PressParams idle_params {
            .color = { 1.F, 1.F, 1.F, 1.F },
            .background = idle_color(),
            .render_bounds = { k_width_px, row_px },
            .budget_h = row_px,
        };
        Text::PressParams hover_params = idle_params;
        hover_params.background = hover_color();

        const std::string text = "  " + labels.at(i);

        auto row = std::make_shared<Element>();
        row->with_name(labels.at(i))
            .with_bounds(bounds)
            .with_buffer(buf)
            .with_text(text, idle_params, bounds);

        const uint32_t id = surface.add(*row);

        surface.ctx().on_enter(id, [row, text, hover_params](uint32_t) {
            row->set_text(text, hover_params);
        });
        surface.ctx().on_leave(id, [row, text, idle_params](uint32_t) {
            row->set_text(text, idle_params);
        });
        surface.ctx().on_press(id, IO::MouseButtons::Left, [state, i](uint32_t, glm::vec2) {
            state->finish(i);
        });

        if (panel) {
            surface.ctx().on_scroll(id, wheel);
            panel->track(
                surface.layer(), id, bounds,
                [row](Kinesis::AABB2D shifted) { row->retarget(shifted); },
                buf);
        }
    }
}

} // namespace MayaFlux::Portal::Forma
