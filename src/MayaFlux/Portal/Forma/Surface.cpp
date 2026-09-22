#include "Surface.hpp"

#include "Internal/Atelier.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux::Portal::Forma {

Surface::Surface(SurfaceConfig config)
    : m_window_ownership(std::make_shared<WindowOwnership>(std::move(config.window)))
    , m_layer(std::move(config.layer))
    , m_ctx(std::move(config.ctx))
{
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

} // namespace MayaFlux::Portal::Forma
