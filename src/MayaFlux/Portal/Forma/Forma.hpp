#pragma once

#include "Bridge.hpp"
#include "Plot/Plot.hpp"
#include "Surface.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
}

namespace MayaFlux::Core {
class Window;
class WindowManager;
struct WindowCreateInfo;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
class Event;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Portal::Forma {

class Inspector;

// =============================================================================
// Internal
// =============================================================================

namespace internal {

    /**
     * @brief Core buffer construction — capacity-explicit path for internal use.
     *
     * Called by public create_buffer overloads and by internal .cpp callers
     * (plot, Collapsible) that know their capacity upfront.
     */
    [[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::FormaBuffer> create_buffer_impl(
        std::shared_ptr<Core::Window> window,
        size_t capacity,
        Graphics::PrimitiveTopology topology,
        const std::string& texture_binding = {},
        std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures = {});

    constexpr size_t k_capacity_bytes = 4096;

} // namespace internal

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
 * @param node_graph_manager Engine NodeGraphManager. Must outlive all Forma objects.
 * @param buffer_manager  Engine BufferManager. Must outlive all Forma objects.
 * @param scheduler       Engine TaskScheduler. Must outlive all Bridge instances.
 * @param event_manager   Engine EventManager. Must outlive all Context instances.
 * @param window_manager  Engine WindowManager. Must outlive all Surface instances.
 * @return True on success, false if any argument is null.
 */
MAYAFLUX_API bool initialize(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager,
    std::shared_ptr<Core::WindowManager> window_manager);

/**
 * @brief Release stored references. Does not destroy any Forma objects.
 *
 * Call after all Forma objects have been destroyed, before engine shutdown.
 */
MAYAFLUX_API void shutdown();

/** @brief Whether initialize() has been called successfully. */
MAYAFLUX_API bool is_initialized();

/**
 * @brief Return the application-level Bridge instance.
 *
 * Valid only after initialize(). Lifetime is tied to the Forma module.
 */
[[nodiscard]] MAYAFLUX_API Bridge& bridge();

/**
 * @brief Access the Forma introspection subsystem.
 *
 * Returns the Inspector instance initialized alongside Bridge during
 * Portal::Forma::initialize(). The Inspector holds no state beyond
 * references to BufferManager and NodeGraphManager; it is a stable
 * entry point into the Inspect:: query functions.
 *
 * @pre Portal::Forma::initialize() must have been called.
 * @return Reference to the singleton Inspector. Lifetime is the Forma module lifetime.
 * @throws std::runtime_error if called before initialize().
 */
[[nodiscard]] MAYAFLUX_API Inspector& inspector();

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
 * @param topology  Primitive topology.
 * @param texture_binding Optional descriptor name for a single texture binding
 * @return Registered, render-ready FormaBuffer.
 */
[[nodiscard]] MAYAFLUX_API
    std::shared_ptr<Buffers::FormaBuffer>
    create_buffer(
        std::shared_ptr<Core::Window> window,
        Graphics::PrimitiveTopology topology,
        const std::string& texture_binding = {});

/**
 * @brief Construct and register a FormaBuffer with additional texture bindings.
 *
 * BufferManager is taken from the stored initialize() state.
 *
 * @param window            Target window.
 * @param topology          Primitive topology.
 * @param additional_textures  Vector of { descriptor name, image } pairs for
 *                             additional texture bindings. These are in
 *                             addition to any default_texture_binding set
 *                             in the RenderConfig passed to setup_rendering().
 * @return Registered, render-ready FormaBuffer.
 */
[[nodiscard]] MAYAFLUX_API
    std::shared_ptr<Buffers::FormaBuffer>
    create_buffer(
        std::shared_ptr<Core::Window> window,
        Graphics::PrimitiveTopology topology,
        std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures);

/**
 * @brief Construct, register, and immediately submit a FormaBuffer from vertices.
 *
 * Deduces capacity and topology from the vertex type. PointVertex yields
 * POINT_LIST, LineVertex yields LINE_LIST, MeshVertex yields TRIANGLE_STRIP.
 * Topology may be overridden explicitly for cases like LINE_STRIP or
 * TRIANGLE_LIST from MeshVertex data.
 *
 * @tparam V    Vertex type: PointVertex, LineVertex, or MeshVertex.
 * @param window    Target window.
 * @param vertices  Vertices to submit immediately after construction.
 * @param topology  Primitive topology. Defaults to the canonical topology for V.
 * @return Registered, render-ready FormaBuffer with initial geometry submitted.
 */
template <typename V>
    requires std::ranges::contiguous_range<V>
    && std::is_trivially_copyable_v<std::ranges::range_value_t<V>>
[[nodiscard]] std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    const V& vertices,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP)
{
    using Vertex = std::ranges::range_value_t<V>;
    const size_t cap = std::ranges::size(vertices) * sizeof(Vertex);
    auto buf = internal::create_buffer_impl(std::move(window), cap, topology);
    buf->submit(vertices);
    return buf;
}

template <typename V>
    requires std::is_trivially_copyable_v<V>
    && (!std::ranges::range<V>)
[[nodiscard]] std::shared_ptr<Buffers::FormaBuffer> create_buffer(
    std::shared_ptr<Core::Window> window,
    const V& vertex,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP)
{
    auto buf = internal::create_buffer_impl(std::move(window), sizeof(V), topology);
    buf->submit(vertex);
    return buf;
}

// =============================================================================
// Element
// =============================================================================

/**
 * @brief Construct a Surface, creating Layer and Context internally.
 *
 * Builds a fresh Layer and a Context wired to @p window using the
 * EventManager stored by Portal::Forma::initialize. Equivalent to
 * Portal::Forma::create_layer(window, name) plus owning the window
 * pointer alongside.
 *
 * For the power-tinkerer case (custom Context subclass, shared Layer
 * across multiple Contexts, etc.), construct Surface directly via its
 * (Window, Layer, Context) constructor.
 *
 * @param window  Target window. Must outlive the Surface.
 * @param name    Unique name scoping the Context's event coroutines.
 *                Must be unique across all live Contexts.
 * @pre Portal::Forma::initialize() must have been called.
 * @return A new Surface owning the window pointer plus the freshly
 *         created Layer and Context.
 */
[[nodiscard]] MAYAFLUX_API Surface create_surface(
    std::shared_ptr<Core::Window> window,
    std::string name);

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
 * @param topology   Primitive topology for the FormaBuffer.
 * @param capacity   Initial FormaBuffer capacity in bytes.
 * @param project    Optional T -> float projection for outbound readers.
 * @return Fully constructed Mapped<T> with element registered in @p layer.
 */
template <typename T>
[[nodiscard]] Mapped<T> create_element(
    Layer& layer,
    std::shared_ptr<Core::Window> window,
    GeometryFn<T> geom,
    T initial,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP,
    size_t capacity = internal::k_capacity_bytes,
    std::function<float(T)> project = {})
{
    auto buf = create_buffer(std::move(window), capacity, topology);
    auto mapped = make_mapped<T>(initial, std::move(geom), buf);
    mapped.element.id = layer.add(mapped.element);
    bridge().register_element(mapped, std::move(project));
    return mapped;
}

/**
 * @brief Build a FormaBuffer, register it, construct a Mapped<T>, add the
 *        element to @p surface's layer, and register it with the
 *        application Bridge.
 *
 * Surface-accepting overload of create_element. Reads the layer and
 * window from @p surface; everything else matches the existing
 * (Layer&, Window) overload.
 *
 * After registration, one sync() is run so that bounds_hint and contains
 * populated by the geometry function are visible on the Element before
 * the first frame. This removes the manual
 * @code
 *   layer->set_bounds(el.element.id, ...);
 *   layer->set_contains(el.element.id, ...);
 * @endcode
 * boilerplate seen at fader-style call sites: those values now arrive
 * directly from the geometry function on construction. The geometry
 * function remains the user's; the sync is the same one that runs every
 * frame.
 *
 * @tparam T        MappedState value type.
 * @param surface   Canvas to register the element on.
 * @param geom      Geometry function producing vertex bytes from T.
 * @param initial   Starting value written into MappedState.
 * @param topology  Primitive topology for the FormaBuffer.
 * @param project   Optional T -> float projection for outbound readers.
 * @return Fully constructed Mapped<T> with element registered.
 */
template <typename T>
[[nodiscard]] Mapped<T> create_element(
    Surface& surface,
    GeometryFn<T> geom,
    T initial,
    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP,
    std::function<float(T)> project = {})
{
    auto mapped = create_element<T>(
        surface.layer(), surface.window(),
        std::move(geom), std::move(initial),
        topology, internal::k_capacity_bytes, std::move(project));

    mapped.sync();
    if (mapped.element.bounds_hint)
        surface.layer().set_bounds(mapped.element.id, *mapped.element.bounds_hint);
    if (mapped.element.contains)
        surface.layer().set_contains(mapped.element.id, mapped.element.contains);

    return mapped;
}

/**
 * @brief Create a live plot in a new window.
 *
 * Creates the window, shows it, constructs the Surface, builds the
 * FormaBuffer from the spec capacity and topology, and calls Plot::place.
 * Returns the Mapped<shared_ptr<PlotContainer>> from Plot::place, plus the
 * Surface owning the window and layer.
 *
 * @param title      Window title.
 * @param width      Window width in pixels.
 * @param height     Window height in pixels.
 * @param container  Ready PlotContainer from Plot::source().build().
 * @param spec       SeriesSpec from Plot::series()...done().
 * @return Pair of { Mapped<shared_ptr<PlotContainer>>, Surface } for the created plot.
 */
[[nodiscard]] MAYAFLUX_API
    std::pair<Mapped<std::shared_ptr<Kakshya::PlotContainer>>, Surface>
    plot(
        std::string title,
        uint32_t width,
        uint32_t height,
        std::shared_ptr<Kakshya::PlotContainer> container,
        Plot::SeriesSpec spec);

// =============================================================================
// Introspection
// =============================================================================

/**
 * @brief Open or show the NodeGraphManager inspection window.
 *
 * First call creates a dedicated window, builds the InspectResult, and
 * schedules tap_all() via Bridge. Subsequent calls show() the existing window.
 */
MAYAFLUX_API void inspect_node_graph();

/**
 * @brief Open or show the BufferManager inspection window.
 *
 * First call creates a dedicated window, builds the InspectResult, and
 * schedules tap_all() via Bridge. Subsequent calls show() the existing window.
 */
MAYAFLUX_API void inspect_buffers();

/**
 * @brief Open or show the TaskScheduler inspection window.
 *
 * First call creates a dedicated window, builds the InspectResult, and
 * schedules tap_all() via Bridge. Subsequent calls show() the existing window.
 */
MAYAFLUX_API void inspect_scheduler();

/**
 * @brief Open or show the EventManager inspection window.
 *
 * First call creates a dedicated window, builds the InspectResult, and
 * schedules tap_all() via Bridge. Subsequent calls show() the existing window.
 */
MAYAFLUX_API void inspect_events();

/**
 * @brief Open a dedicated window inspecting a single Node and its modulator tree.
 */
MAYAFLUX_API void inspect(const std::shared_ptr<Nodes::Node>& node);

/**
 * @brief Open a dedicated window inspecting a single Buffer and its processing chain.
 */
MAYAFLUX_API void inspect(const std::shared_ptr<Buffers::Buffer>& buf);

/**
 * @brief Open a dedicated window inspecting a NodeNetwork.
 */
MAYAFLUX_API void inspect(const std::shared_ptr<Nodes::Network::NodeNetwork>& net);

/**
 * @brief Open a dedicated window inspecting a single Event.
 */
MAYAFLUX_API void inspect(const std::shared_ptr<Vruta::Event>& ev, std::string_view name = {});

} // namespace MayaFlux::Portal::Forma
