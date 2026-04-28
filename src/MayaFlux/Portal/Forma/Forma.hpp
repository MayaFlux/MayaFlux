#pragma once

#include "Bridge.hpp"
#include "Context.hpp"
#include "Layer.hpp"
#include "Primitives/Mapped.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
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
 * Call initialize() once after engine startup to store BufferManager,
 * TaskScheduler, and EventManager references. Subsequent factory calls
 * require only the arguments that legitimately vary per call (Window,
 * geometry function, initial value). This matches the Portal::Text and
 * Portal::Graphics initialization contracts.
 *
 * Typical sequence:
 * @code
 * Portal::Forma::initialize(buffer_manager, scheduler, event_manager);
 *
 * auto [layer, ctx] = Forma::create_layer(window, "hud");
 *
 * auto el = Forma::create_element<float>(
 *     *layer, window, geom_fn, 0.5f);
 *
 * auto bridge = Forma::create_bridge();
 * bridge.bind(el.state, envelope);
 * bridge.write(el.state, compute_proc, offsetof(PC, cutoff));
 *
 * ctx->on_press(el.element.id, IO::MouseButtons::LEFT,
 *     [](uint32_t, glm::vec2){});
 * @endcode
 */

// =============================================================================
// Lifecycle
// =============================================================================

/**
 * @brief Store engine-level references for use by all subsequent Forma calls.
 *
 * Must be called before any create_* call. Safe to call multiple times;
 * subsequent calls are no-ops and log a warning.
 *
 * @param buffer_manager  Engine BufferManager. Must outlive all Forma objects.
 * @param scheduler       Engine TaskScheduler. Must outlive all Bridge instances.
 * @param event_manager   Engine EventManager. Must outlive all Context instances.
 * @return True on success, false if any argument is null.
 */
MAYAFLUX_API bool initialize(
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager);

/**
 * @brief Release stored references. Does not destroy any Forma objects.
 *
 * Call after all Forma objects have been destroyed, before engine shutdown.
 */
MAYAFLUX_API void shutdown();

/** @brief Whether initialize() has been called successfully. */
MAYAFLUX_API bool is_initialized();

// =============================================================================
// Layer
// =============================================================================

/**
 * @brief Construct a Layer and a Context wired to @p window.
 *
 * EventManager is taken from the stored initialize() state.
 * The caller owns both returned objects. The Context cancels its event
 * coroutines on destruction.
 *
 * @param window  Target window surface.
 * @param name    Unique name scoping the Context's event coroutines.
 * @return Pair of { shared_ptr<Layer>, shared_ptr<Context> }.
 */
[[nodiscard]] MAYAFLUX_API
    std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
    create_layer(
        const std::shared_ptr<Core::Window>& window,
        std::string name);

// =============================================================================
// Standalone buffer
// =============================================================================

/**
 * @brief Construct and register a FormaBuffer without creating a Mapped<T>.
 *
 * BufferManager is taken from the stored initialize() state.
 *
 * @param window    Target window.
 * @param capacity  Buffer capacity in bytes.
 * @param topology  Primitive topology.
 * @return Registered, render-ready FormaBuffer.
 */
[[nodiscard]] MAYAFLUX_API
    std::shared_ptr<Buffers::FormaBuffer>
    create_buffer(
        std::shared_ptr<Core::Window> window,
        size_t capacity,
        Graphics::PrimitiveTopology topology);

// =============================================================================
// Bridge
// =============================================================================

/**
 * @brief Construct a Bridge using the stored TaskScheduler and BufferManager.
 *
 * One Bridge instance typically serves the full application.
 *
 * @return Fully constructed Bridge.
 */
[[nodiscard]] MAYAFLUX_API Bridge create_bridge();

// =============================================================================
// Element
// =============================================================================

/**
 * @brief Build a FormaBuffer, register it, construct a Mapped<T>, and add
 *        the element to @p layer.
 *
 * BufferManager is taken from the stored initialize() state.
 * Returns the fully constructed Mapped<T>. The caller holds it.
 * element.id is stable and can be passed to Context callbacks and Bridge.
 *
 * @tparam T         MappedState value type: float, glm::vec2, etc.
 * @param layer      Layer to register the element on.
 * @param window     Target window for rendering.
 * @param geom       Geometry function producing vertex bytes from T.
 * @param initial    Starting value written into MappedState.
 * @param capacity   Initial FormaBuffer capacity in bytes.
 * @param topology   Primitive topology for the FormaBuffer.
 * @return Fully constructed Mapped<T> with element registered in @p layer.
 */
template <typename T>
[[nodiscard]] Mapped<T> create_element(
    Layer& layer,
    std::shared_ptr<Core::Window> window,
    GeometryFn<T> geom,
    T initial,
    size_t capacity = 4096,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP)
{
    auto buf = create_buffer(std::move(window), capacity, topology);
    auto mapped = make_mapped<T>(initial, std::move(geom), buf);
    layer.add(mapped.element);
    return mapped;
}

} // namespace MayaFlux::Portal::Forma
