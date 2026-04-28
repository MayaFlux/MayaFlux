#include "Forma.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"

namespace MayaFlux::Portal::Forma {

// =============================================================================
// Layer
// =============================================================================

std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
create_layer(
    const std::shared_ptr<Core::Window>& window,
    std::string name,
    Vruta::EventManager& event_manager)
{
    auto layer = std::make_shared<Layer>();
    auto ctx = std::make_shared<Context>(layer, window, event_manager, std::move(name));
    MayaFlux::store(ctx);

    return { std::move(layer), std::move(ctx) };
}

// =============================================================================
// Standalone buffer
// =============================================================================

std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    Buffers::BufferManager& buffer_manager,
    size_t capacity,
    Graphics::PrimitiveTopology topology)
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(capacity, topology);

    buffer_manager.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    buf->setup_rendering({ .target_window = std::move(window) });

    return buf;
}

} // namespace MayaFlux::Portal::Forma
