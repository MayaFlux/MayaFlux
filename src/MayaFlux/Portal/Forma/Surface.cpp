#include "Surface.hpp"

#include "Internal/Atelier.hpp"

namespace MayaFlux::Portal::Forma {

Surface::Surface(std::shared_ptr<Core::Window> window,
    std::shared_ptr<Layer> layer,
    std::shared_ptr<Context> ctx)
    : m_window(std::move(window))
    , m_layer(std::move(layer))
    , m_ctx(std::move(ctx))
{
    m_ctx->on_close(0, [this]() { handle_close(); });
}

void Surface::handle_close()
{
    internal::atelier().bridge().stop_sync(*m_layer);

    m_window.reset();
}

} // namespace MayaFlux::Portal::Forma
