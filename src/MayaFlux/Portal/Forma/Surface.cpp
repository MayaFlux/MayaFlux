#include "Surface.hpp"

#include "Internal/Atelier.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /**
     * Places pointer positions of the host element on the image it shows, read
     * live so a moved element or a resized target stays right.
     */
    struct ElementImageMap {
        std::weak_ptr<Layer> layer;
        uint32_t element {};
        std::shared_ptr<Portal::Graphics::ImageTarget> image;

        [[nodiscard]] std::optional<glm::vec2> unit(glm::vec2 ndc) const
        {
            const auto host = layer.lock();
            if (!host)
                return std::nullopt;

            const Element* shown = host->get(element);
            if (!shown || !shown->bounds_hint)
                return std::nullopt;

            const glm::vec2 size = shown->bounds_hint->max - shown->bounds_hint->min;
            if (size.x <= 0.0F || size.y <= 0.0F)
                return std::nullopt;

            return (ndc - shown->bounds_hint->min) / size;
        }

        [[nodiscard]] std::optional<glm::dvec2> pixel(glm::vec2 ndc) const
        {
            const auto uv = unit(ndc);
            if (!uv)
                return std::nullopt;

            return glm::dvec2 {
                static_cast<double>(uv->x) * image->width(),
                (1.0 - static_cast<double>(uv->y)) * image->height(),
            };
        }

        [[nodiscard]] bool inside(glm::vec2 ndc) const
        {
            const auto uv = unit(ndc);
            return uv && uv->x >= 0.0F && uv->x <= 1.0F && uv->y >= 0.0F && uv->y <= 1.0F;
        }
    };

    constexpr std::array<IO::MouseButtons, 3> k_buttons {
        IO::MouseButtons::Left, IO::MouseButtons::Right, IO::MouseButtons::Middle
    };

    /**
     * A release that happened off the element never reaches it, so a button the
     * Surface still holds is let go quietly once the host reports it up.
     */
    void settle_buttons(Context& fed, const Context& host)
    {
        for (const auto btn : k_buttons) {
            if (fed.button_down(btn) && !host.button_down(btn))
                fed.feed_cancel(btn);
        }
    }

}

glm::uvec2 Surface::extent_of(
    const std::shared_ptr<Core::Window>& window,
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image) noexcept
{
    if (window) {
        const auto& state = window->get_state();
        return { state.current_width, state.current_height };
    }
    if (image) {
        return { image->width(), image->height() };
    }
    return { 0U, 0U };
}

Surface::Surface(SurfaceConfig config)
    : m_window_ownership(std::make_shared<WindowOwnership>(std::move(config.window)))
    , m_image(std::move(config.image))
    , m_layer(std::move(config.layer))
    , m_ctx(std::move(config.ctx))
{
    if (m_window_ownership->window) {
        m_image.reset();
    }

    if (!m_window_ownership->window) {
        return;
    }

    const bool detach = config.should_detach_on_close();

    auto close_ownership = m_window_ownership;
    auto close_layer = m_layer;
    auto* event_source = &close_ownership->window->get_event_source();

    m_ctx->on_close(0, [ownership = std::move(close_ownership), layer = std::move(close_layer), event_source, detach, on_close = std::move(config.on_close)]() {
        if (on_close)
            on_close();

        if (detach) {
            if (layer)
                internal::atelier().bridge().stop_sync(*layer);

            event_source->defer([ownership]() { ownership->window.reset(); });
        }
    });
}

bool Surface::route_input_from(Surface& host, uint32_t element)
{
    using Type = Core::WindowEventType;

    if (!m_image || window())
        return false;

    const Element* shown = host.layer().get(element);
    if (!shown || !shown->bounds_hint)
        return false;

    const ElementImageMap map { .layer = host.layer_ptr(), .element = element, .image = m_image };
    const std::shared_ptr<Context> fed = m_ctx;
    const Context* host_ctx = &host.ctx();

    const auto subscribe = [&](Type type, std::optional<IO::MouseButtons> button, Context::SubscriberFn fn) {
        Vruta::WindowEventFilter filter;
        filter.event_type = type;
        filter.button = button;
        host.ctx().subscribe(element, filter, std::move(fn));
    };

    subscribe(Type::MOUSE_MOTION, std::nullopt,
        [map, fed, host_ctx](uint32_t, const Core::WindowEvent&, glm::vec2 ndc) {
            settle_buttons(*fed, *host_ctx);
            if (const auto at = map.pixel(ndc))
                fed->feed_move(at->x, at->y);
        });

    for (const auto btn : k_buttons) {
        subscribe(Type::MOUSE_MOTION, btn,
            [map, fed](uint32_t, const Core::WindowEvent&, glm::vec2 ndc) {
                if (map.inside(ndc))
                    return;
                if (const auto at = map.pixel(ndc))
                    fed->feed_move(at->x, at->y);
            });
    }

    subscribe(Type::MOUSE_BUTTON_PRESSED, std::nullopt,
        [map, fed](uint32_t, const Core::WindowEvent& event, glm::vec2 ndc) {
            const auto* data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data);
            const auto at = map.pixel(ndc);
            if (data && at)
                fed->feed_press(at->x, at->y, static_cast<IO::MouseButtons>(data->button));
        });

    subscribe(Type::MOUSE_BUTTON_RELEASED, std::nullopt,
        [map, fed](uint32_t, const Core::WindowEvent& event, glm::vec2 ndc) {
            const auto* data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data);
            const auto at = map.pixel(ndc);
            if (data && at)
                fed->feed_release(at->x, at->y, static_cast<IO::MouseButtons>(data->button));
        });

    subscribe(Type::MOUSE_SCROLLED, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent& event, glm::vec2) {
            if (const auto* data = std::get_if<Core::WindowEvent::ScrollData>(&event.data))
                fed->feed_scroll(data->x_offset, data->y_offset);
        });

    subscribe(Type::MOUSE_EXITED, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent&, glm::vec2) { fed->feed_leave(); });

    subscribe(Type::TEXT_INPUT, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent& event, glm::vec2) {
            if (const auto* data = std::get_if<Core::WindowEvent::TextData>(&event.data))
                fed->feed_text(data->codepoint);
        });

    subscribe(Type::WINDOW_FOCUS_LOST, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent&, glm::vec2) { fed->clear_focus(); });

    return true;
}

} // namespace MayaFlux::Portal::Forma
