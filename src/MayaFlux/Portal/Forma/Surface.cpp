#include "Surface.hpp"

#include "Internal/Atelier.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"
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
    const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
    const Kinesis::ViewportArea& area) noexcept
{
    if (window) {
        const auto& state = window->get_state();
        return { state.current_width, state.current_height };
    }
    if (image) {
        const auto filled = area.pixels(image->width(), image->height());
        return { filled.width(), filled.height() };
    }
    return { 0U, 0U };
}

void Surface::set_view(const std::optional<Kinesis::ViewTransform>& view)
{
    m_layer->set_view(view);
    if (m_image) {
        m_image->request_redraw();
    }
}

void Surface::set_placement(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> placement)
{
    m_layer->set_placement(std::move(view), std::move(placement));
    if (m_image) {
        m_image->request_redraw();
    }
}

Surface::Surface(SurfaceConfig config)
    : m_window_ownership(std::make_shared<WindowOwnership>(std::move(config.window)))
    , m_image(std::move(config.image))
    , m_area(config.area)
    , m_layer(std::move(config.layer))
    , m_ctx(std::move(config.ctx))
{
    if (m_window_ownership->window) {
        m_image.reset();
        m_area = {};
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
            fed->feed_modifiers(host_ctx->modifiers());
            settle_buttons(*fed, *host_ctx);
            if (const auto at = map.pixel(ndc))
                fed->feed_move(at->x, at->y);
        });

    for (const auto btn : k_buttons) {
        subscribe(Type::MOUSE_MOTION, btn,
            [map, fed, host_ctx](uint32_t, const Core::WindowEvent&, glm::vec2 ndc) {
                if (map.inside(ndc))
                    return;
                fed->feed_modifiers(host_ctx->modifiers());
                if (const auto at = map.pixel(ndc))
                    fed->feed_move(at->x, at->y);
            });
    }

    subscribe(Type::MOUSE_BUTTON_PRESSED, std::nullopt,
        [map, fed, host_ctx](uint32_t, const Core::WindowEvent& event, glm::vec2 ndc) {
            fed->feed_modifiers(host_ctx->modifiers());
            const auto* data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data);
            const auto at = map.pixel(ndc);
            if (data && at)
                fed->feed_press(at->x, at->y, static_cast<IO::MouseButtons>(data->button));
        });

    subscribe(Type::MOUSE_BUTTON_RELEASED, std::nullopt,
        [map, fed, host_ctx](uint32_t, const Core::WindowEvent& event, glm::vec2 ndc) {
            fed->feed_modifiers(host_ctx->modifiers());
            const auto* data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data);
            const auto at = map.pixel(ndc);
            if (data && at)
                fed->feed_release(at->x, at->y, static_cast<IO::MouseButtons>(data->button));
        });

    subscribe(Type::MOUSE_SCROLLED, std::nullopt,
        [fed, host_ctx](uint32_t, const Core::WindowEvent& event, glm::vec2) {
            fed->feed_modifiers(host_ctx->modifiers());
            if (const auto* data = std::get_if<Core::WindowEvent::ScrollData>(&event.data))
                fed->feed_scroll(data->x_offset, data->y_offset);
        });

    subscribe(Type::MOUSE_EXITED, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent&, glm::vec2) { fed->feed_leave(); });

    subscribe(Type::TEXT_INPUT, std::nullopt,
        [fed, host_ctx](uint32_t, const Core::WindowEvent& event, glm::vec2) {
            fed->feed_modifiers(host_ctx->modifiers());
            if (const auto* data = std::get_if<Core::WindowEvent::TextData>(&event.data))
                fed->feed_text(data->codepoint);
        });

    for (const auto type : { Type::KEY_PRESSED, Type::KEY_RELEASED, Type::KEY_REPEAT }) {
        subscribe(type, std::nullopt,
            [fed](uint32_t, const Core::WindowEvent& event, glm::vec2) {
                if (const auto* data = std::get_if<Core::WindowEvent::KeyData>(&event.data))
                    fed->feed_key(static_cast<IO::Keys>(data->key), event.type, data->mods);
            });
    }

    subscribe(Type::WINDOW_FOCUS_LOST, std::nullopt,
        [fed](uint32_t, const Core::WindowEvent&, glm::vec2) { fed->clear_focus(); });

    return true;
}

glm::uvec2 row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h)
{
    const auto& ws = window->get_state();
    return row_pixel_dims(glm::uvec2 { ws.current_width, ws.current_height }, x_min, x_max, row_h);
}

glm::uvec2 row_pixel_dims(
    glm::uvec2 extent,
    float x_min, float x_max, float row_h)
{
    const auto w = static_cast<uint32_t>(
        (x_max - x_min) * 0.5F * static_cast<float>(extent.x));
    const auto h = static_cast<uint32_t>(
        row_h * 0.5F * static_cast<float>(extent.y));
    auto atlas = Text::TypeFaceFoundry::instance().get_default_glyph_atlas();
    if (!atlas) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::Runtime,
            std::source_location::current(),
            "Failed to get default glyph atlas for row pixel dimension calculation");
    }
    const uint32_t min_h = atlas->pixel_size();

    return { std::max(w, 1U), std::max(h, min_h) };
}

} // namespace MayaFlux::Portal::Forma
