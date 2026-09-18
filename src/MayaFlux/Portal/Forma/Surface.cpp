#include "Surface.hpp"

#include "Internal/Atelier.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux::Portal::Forma {

Surface::Surface(std::shared_ptr<Core::Window> window,
    std::shared_ptr<Layer> layer,
    std::shared_ptr<Context> ctx)
    : m_window_ownership(std::make_shared<WindowOwnership>(std::move(window)))
    , m_layer(std::move(layer))
    , m_ctx(std::move(ctx))
{
    auto close_ownership = m_window_ownership;
    auto close_layer = m_layer;
    auto* event_source = &close_ownership->window->get_event_source();

    m_ctx->on_close(0, [ownership = std::move(close_ownership), layer = std::move(close_layer), event_source]() {
        if (layer)
            internal::atelier().bridge().stop_sync(*layer);

        event_source->defer([ownership]() { ownership->window.reset(); });
    });
}

} // namespace MayaFlux::Portal::Forma
