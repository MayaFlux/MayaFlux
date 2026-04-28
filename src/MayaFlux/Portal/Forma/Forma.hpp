#pragma once

#include "Context.hpp"
#include "Layer.hpp"
#include "Primitives/Mapped.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class EventManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Portal::Forma {

/**
 * @file Forma.hpp
 * @brief Factory free functions for the Forma surface system.
 *
 * Forma is not a class. It is a set of free functions that construct
 * the right objects in the right order and return them to the caller.
 * The caller owns everything. Nothing is tracked centrally.
 *
 * Typical sequence:
 * @code
 * auto [layer, ctx] = create_layer(window, "hud", event_manager);
 *
 * auto el = create_element<float>(
 *     *layer, window, buffer_manager, geom_fn, 0.5f);
 *
 * ctx->on_press(el.element.id, IO::MouseButtons::LEFT,
 *     [](uint32_t, glm::vec2){});
 *
 * bridge.bind(el.state, envelope);
 * bridge.write(el.state, compute_proc, offsetof(PC, cutoff));
 * @endcode
 */
// =============================================================================
// Layer
// =============================================================================

/**
 * @brief Construct a Layer and a Context wired to @p window.
 *
 * The caller owns both. The Context cancels its event coroutines on
 * destruction. The Layer and Context must outlive any element ids
 * registered against them.
 *
 * @param window        Target window surface.
 * @param name          Unique name scoping the Context's event coroutines.
 * @param event_manager Engine EventManager for Context coroutine registration.
 * @return Pair of { shared_ptr<Layer>, shared_ptr<Context> }.
 */
[[nodiscard]] MAYAFLUX_API
    std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
    create_layer(
        const std::shared_ptr<Core::Window>& window,
        std::string name,
        Vruta::EventManager& event_manager);

// =============================================================================
// Element
// =============================================================================

/**
 * @brief Build a FormaBuffer, register it, construct a Mapped<T>, and add
 *        the element to @p layer.
 *
 * Returns the fully constructed Mapped<T>. The caller holds it.
 * element.id is stable and can be passed to Context callbacks.
 * state is what Bridge::bind() and Bridge::write() accept.
 *
 * @tparam T         MappedState value type: float, glm::vec2, etc.
 * @param layer          Layer to register the element on.
 * @param window         Target window for rendering.
 * @param buffer_manager Engine BufferManager for buffer registration.
 * @param geom           Geometry function producing vertex bytes from T.
 * @param initial        Starting value written into MappedState.
 * @param capacity       Initial FormaBuffer capacity in bytes.
 * @param topology       Primitive topology for the FormaBuffer.
 * @return Fully constructed Mapped<T> with element registered in @p layer.
 */
template <typename T>
[[nodiscard]] Mapped<T> create_element(
    Layer& layer,
    std::shared_ptr<Core::Window> window,
    Buffers::BufferManager& buffer_manager,
    GeometryFn<T> geom,
    T initial,
    size_t capacity = 4096,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP);

// =============================================================================
// Standalone buffer
// =============================================================================

/**
 * @brief Construct and register a FormaBuffer without creating a Mapped<T>.
 *
 * For callers that manage Mapped<T> themselves or use FormaBuffer in a
 * custom pipeline.
 *
 * @param window         Target window.
 * @param buffer_manager Engine BufferManager.
 * @param capacity       Buffer capacity in bytes.
 * @param topology       Primitive topology.
 * @return Registered, render-ready FormaBuffer.
 */
[[nodiscard]] MAYAFLUX_API
    std::shared_ptr<Buffers::FormaBuffer>
    create_buffer(
        std::shared_ptr<Core::Window> window,
        Buffers::BufferManager& buffer_manager,
        size_t capacity,
        Graphics::PrimitiveTopology topology);

// =============================================================================
// create_element — template body
// =============================================================================

template <typename T>
Mapped<T> create_element(
    Layer& layer,
    const std::shared_ptr<Core::Window>& window,
    Buffers::BufferManager& buffer_manager,
    GeometryFn<T> geom,
    T initial,
    size_t capacity,
    Graphics::PrimitiveTopology topology)
{
    auto buf = create_buffer(window, buffer_manager, capacity, topology);

    auto mapped = make_mapped<T>(initial, std::move(geom), buf);
    layer.add(mapped.element);

    return mapped;
}

} // namespace MayaFlux::Portal::Forma
