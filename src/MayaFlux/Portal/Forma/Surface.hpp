#pragma once

#include "Context.hpp"
#include "Layer.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Forma {

/**
 * @class Surface
 * @brief Named owner of a (Window, Layer, Context) triple - the Forma canvas.
 *
 * In Forma, three things always travel together: a Window (the rendering
 * target and coordinate space), a Layer (the spatial registry of elements),
 * and a Context (the event router that hit-tests against the layer). They
 * are the three faces of one concept: the canvas. Surface names that
 * concept and owns the triple, so that downstream construction functions
 * accept one argument instead of three.
 *
 * Surface is not a widget toolkit, not a layout engine, not an event
 * dispatcher. It does not know what an element looks like, how a buffer
 * is built, or where bindings are routed. Construction of FormaBuffers,
 * Mapped<T>s, and Bridge registrations is the job of the free functions
 * in Forma.hpp, which take a Surface& as their canvas argument and read
 * the BufferManager, Bridge, and other module-level state from the
 * singletons established by Portal::Forma::initialize().
 *
 * The Layer and Context owned by Surface remain fully accessible through
 * the layer() and ctx() accessors. Surface is a named owner, not a wall.
 * Anything that worked against Layer or Context before continues to work
 * against surface.layer() and surface.ctx().
 *
 * Surface has two constructors:
 *   1. The default: takes Window + name. Internally creates Layer and
 *      Context. This is the path covered by Portal::Forma::create_surface
 *      in Forma.hpp.
 *   2. The power-tinkerer: takes Window + pre-built Layer + pre-built
 *      Context. Use this when you need a custom Context subclass, want
 *      to share one Layer across multiple Contexts (split-pane editing),
 *      or are constructing in a test against a non-global EventManager.
 *
 * @code
 * // Default path
 * auto surface = Portal::Forma::create_surface(window, "plot_live");
 *
 * auto el = Portal::Forma::create_element<float>(surface, geom, 0.5F);
 * surface.ctx().on_press(el.element.id, IO::MouseButtons::Left, ...);
 *
 * // Power-tinkerer path: pre-built layer and context
 * auto layer = std::make_shared<Layer>();
 * auto ctx = std::make_shared<MyCustomContext>(layer, window, em, "custom");
 * Surface surface(window, layer, ctx);
 * @endcode
 */
class MAYAFLUX_API Surface {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Surface around a pre-built Layer and Context.
     *
     * Surface takes shared ownership of all three components. The caller is
     * responsible for ensuring the Layer and Context were constructed
     * against @p window.
     *
     * The default factory Portal::Forma::create_surface delegates here
     * after building a fresh Layer and Context against the global
     * EventManager. Call this constructor directly when you need a custom
     * Context subclass or want to share a Layer across multiple Contexts.
     *
     * @param window  Target window. Must match the window the Context was
     *                constructed against.
     * @param layer   Pre-built Layer.
     * @param ctx     Pre-built Context, already wired to @p window.
     */
    Surface(std::shared_ptr<Core::Window> window,
        std::shared_ptr<Layer> layer,
        std::shared_ptr<Context> ctx)
        : m_window(std::move(window))
        , m_layer(std::move(layer))
        , m_ctx(std::move(ctx))
    {
    }

    ~Surface() = default;

    Surface(const Surface&) = default;
    Surface& operator=(const Surface&) = default;
    Surface(Surface&&) noexcept = default;
    Surface& operator=(Surface&&) noexcept = default;

    // =========================================================================
    // Accessors - the canvas is never walled off
    // =========================================================================

    /**
     * @brief Access the spatial registry.
     *
     * Anything supported by Layer is reachable here: add, remove, relate,
     * set_visible, set_bounds, set_contains, hit_test, etc.
     */
    [[nodiscard]] Layer& layer() noexcept { return *m_layer; }

    /// @copydoc layer()
    [[nodiscard]] const Layer& layer() const noexcept { return *m_layer; }

    /**
     * @brief Access the event router.
     *
     * Anything supported by Context is reachable here: on_press,
     * on_release, on_move, on_enter, on_leave, on_scroll.
     */
    [[nodiscard]] Context& ctx() noexcept { return *m_ctx; }

    /// @copydoc ctx()
    [[nodiscard]] const Context& ctx() const noexcept { return *m_ctx; }

    /**
     * @brief Access the rendering target window.
     */
    [[nodiscard]] const std::shared_ptr<Core::Window>& window() const noexcept
    {
        return m_window;
    }

    /**
     * @brief Shared handles for callers that need to keep components alive
     *        independently of the Surface (background tasks, escapes into
     *        coroutines, etc.).
     */
    [[nodiscard]] const std::shared_ptr<Layer>& layer_ptr() const noexcept { return m_layer; }

    /// @copydoc layer_ptr()
    [[nodiscard]] const std::shared_ptr<Context>& ctx_ptr() const noexcept { return m_ctx; }

    // =========================================================================
    // Element registration passthrough
    // =========================================================================

    /**
     * @brief Forward an Element to layer().add() and return the Slot.
     *
     * Pure passthrough. Present so that existing patterns like
     * @code
     *   const uint32_t id = layer->add(std::move(el)).relate_to(parent).id();
     * @endcode
     * read as
     * @code
     *   const uint32_t id = surface.add(std::move(el)).relate_to(parent).id();
     * @endcode
     * without forcing the caller to reach through layer() at every step.
     */
    Layer::Slot add(Element element) { return m_layer->add(std::move(element)); }

private:
    std::shared_ptr<Core::Window> m_window;
    std::shared_ptr<Layer> m_layer;
    std::shared_ptr<Context> m_ctx;
};

} // namespace MayaFlux::Portal::Forma
