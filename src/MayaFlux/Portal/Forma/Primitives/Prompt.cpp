#include "Prompt.hpp"

#include "TextField.hpp"

#include "MayaFlux/Portal/Forma/Internal/Atelier.hpp"
#include "MayaFlux/Portal/Forma/Internal/TextRow.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    constexpr uint32_t k_width_px = 520;
    constexpr uint32_t k_min_row_px = 48;
    constexpr uint32_t k_row_padding_px = 16;

    constexpr glm::vec4 k_accepted_color { 1.F, 1.F, 1.F, 1.F };
    constexpr glm::vec4 k_refused_color { 1.F, 0.45F, 0.4F, 1.F };

    struct PromptState {
        AskCallback callback;
        AskValidator validator;
        std::weak_ptr<Core::Window> window;
        bool done {};

        [[nodiscard]] bool accepts(std::string_view text) const
        {
            return !validator || validator(text);
        }

        void finish(std::optional<std::string> result, bool close_window)
        {
            if (done) {
                return;
            }
            done = true;

            if (close_window) {
                if (auto w = window.lock()) {
                    w->request_close();
                }
            }
            if (callback) {
                callback(std::move(result));
            }
        }
    };

} // namespace

namespace internal {

    std::function<void()> ask_in(
        Surface& surface,
        FormaTarget target,
        uint32_t row_px,
        std::string initial,
        AskValidator validator,
        AskCallback callback)
    {
        auto state = std::make_shared<PromptState>();
        state->callback = std::move(callback);
        state->validator = std::move(validator);
        state->window = surface.window();

        const glm::uvec2 dims { surface.extent().x, row_px };
        auto params = std::make_shared<Text::PressParams>(row_press_params(dims, row_idle_color()));

        const auto tint = [params, state](std::string_view text) {
            params->color = state->accepts(text) ? k_accepted_color : k_refused_color;
        };
        tint(initial);

        TextField field;
        field.single_line()
            .on_change([tint](const std::string& text) { tint(text); })
            .on_commit([state](const std::string& text) {
                if (state->accepts(text)) {
                    state->finish(text, true);
                }
            })
            .on_cancel([state] { state->finish(std::nullopt, true); });

        field.place(std::move(target), surface, surface.full(), params, std::move(initial));
        surface.ctx().set_focus(field.element_id);

        return [state] { state->finish(std::nullopt, false); };
    }

} // namespace internal

void ask(
    std::string title,
    std::string initial,
    AskValidator validator,
    AskCallback callback)
{
    uint32_t row_px = k_min_row_px;
    if (auto atlas = Text::TypeFaceFoundry::instance().get_default_glyph_atlas()) {
        row_px = std::max(row_px, static_cast<uint32_t>(atlas->pixel_size()) + k_row_padding_px);
    }

    auto& atelier = internal::atelier();

    auto window = atelier.create_window(Core::WindowCreateInfo {
        .title = std::move(title) + "  (Enter accepts, Escape cancels)",
        .width = k_width_px,
        .height = row_px,
        .resizable = false,
    });

    auto cancel = std::make_shared<std::function<void()>>();

    Surface surface = atelier.create_surface(SurfaceConfig {
        .window = window,
        .name = window->get_create_info().title,
        .on_close = [cancel] {
            if (*cancel) {
                (*cancel)();
            }
        },
    });

    *cancel = internal::ask_in(
        surface,
        atelier.batch_slice(surface, Graphics::PrimitiveTopology::TRIANGLE_LIST),
        row_px, std::move(initial), std::move(validator), std::move(callback));
}

void ask(
    std::string title,
    std::string initial,
    AskCallback callback)
{
    ask(std::move(title), std::move(initial), AskValidator {}, std::move(callback));
}

} // namespace MayaFlux::Portal::Forma
