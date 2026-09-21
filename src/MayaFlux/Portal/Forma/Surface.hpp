#pragma once

#include "Context.hpp"
#include "Layer.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Forma {

/**
 * @struct SurfaceConfig
 * @brief The single argument every Surface-creating function in this module
 *        takes, at every level of abstraction.
 *
 * Surface, Atelier::create_surface, and Portal::Forma::create_surface all
 * take this instead of a loose (window, name) pair: when @c layer/@c ctx are
 * unset, a fresh Layer and Context are built from @c window/@c name against
 * the global EventManager and filled in; when both are already set (the
 * power-tinkerer case) they are used as-is and @c name is ignored. This type
 * must not itself reach into internal::atelier() - only Forma.hpp/.cpp touch
 * that singleton - building stays the job of whichever function is given
 * the config.
 */
struct SurfaceConfig {
    /// @brief Target window. Must outlive the Surface.
    std::shared_ptr<Core::Window> window;

    /**
     * @brief Unique name scoping the Context's event coroutines.
     *
     * Only consulted when @c layer/@c ctx are unset and need to be built.
     * Ignored when both are already supplied.
     */
    std::string name;

    /// @brief Pre-built Layer, constructed against @c window. Optional -
    ///        built automatically from @c name when unset.
    std::shared_ptr<Layer> layer;

    /// @brief Pre-built Context, already wired to @c window. Optional -
    ///        built automatically from @c name when unset.
    std::shared_ptr<Context> ctx;

    /**
     * @brief Whether the built-in close handling (Bridge::stop_sync plus a
     *        deferred window release, see Surface::Surface) runs. Forced
     *        true whenever on_close is set - see should_detach_on_close().
     */
    bool detach_on_close { true };

    /**
     * @brief Runs in addition to, never instead of, the built-in close
     *        handling. Setting this forces should_detach_on_close() true
     *        regardless of detach_on_close, so supplying a hook can never
     *        silently lose the window-release cleanup.
     *
     *        Context::on_close is a single slot per id, last write wins -
     *        a bare replacement field here would silently clobber
     *        Surface's own close handler on first use. This exists so that
     *        can't happen.
     */
    std::function<void()> on_close;

    /// @brief Whether the built-in detach behavior actually runs.
    [[nodiscard]] bool should_detach_on_close() const noexcept
    {
        return detach_on_close || static_cast<bool>(on_close);
    }
};

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
 * Surface(SurfaceConfig) never creates the Layer or Context itself - only
 * ever wraps ones already built. Portal::Forma::create_surface (in
 * Forma.hpp) also takes a SurfaceConfig: the default path leaves layer/ctx
 * unset, so a fresh Layer and Context are built against the global
 * EventManager from window/name and the Surface is constructed from those.
 * Set layer/ctx yourself in the same SurfaceConfig - the power-tinkerer path
 * - when you need a custom Context subclass, want to share one Layer across
 * multiple Contexts (split-pane editing), or are constructing in a test
 * against a non-global EventManager.
 *
 * @code
 * // Default path
 * auto surface = Portal::Forma::create_surface(
 *     SurfaceConfig { .window = window, .name = "plot_live" });
 *
 * auto el = Portal::Forma::create_element<float>(surface, geom, 0.5F);
 * surface.ctx().on_press(el.element.id, IO::MouseButtons::Left, ...);
 *
 * // Power-tinkerer path: pre-built layer and context
 * auto layer = std::make_shared<Layer>();
 * auto ctx = std::make_shared<MyCustomContext>(layer, window, em, "custom");
 * Surface surface(SurfaceConfig { .window = window, .layer = layer, .ctx = ctx });
 * @endcode
 */
class MAYAFLUX_API Surface {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Surface from a SurfaceConfig.
     *
     * Surface takes shared ownership of all three components. The caller is
     * responsible for ensuring the Layer and Context were constructed
     * against @c config.window. Wires the built-in close handling
     * (Bridge::stop_sync plus a deferred window release) per
     * @c config.should_detach_on_close(), then runs @c config.on_close if
     * set - see SurfaceConfig's own doc for the composition rule between
     * the two.
     *
     * @param config Window, Layer, Context, and close-handling behavior.
     */
    explicit Surface(SurfaceConfig config);

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
        return m_window_ownership->window;
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

    /**
     * @brief Forward an id to layer().remove().
     * @param id Element id to remove from the layer.
     */
    void remove(uint32_t id) { m_layer->remove(id); }

    // =========================================================================
    // Named regions
    // =========================================================================

    /**
     * @brief Rect of size (@p w, @p h) at the top-left corner, inset by @p margin.
     * @param w      Width in NDC units.
     * @param h      Height in NDC units.
     * @param margin Inset from both screen edges in NDC units.
     */
    [[nodiscard]] Kinesis::AABB2D top_left(float w, float h, float margin = 0.02F) const noexcept
    {
        return { .min = { -1.F + margin, 1.F - margin - h },
            .max = { -1.F + margin + w, 1.F - margin } };
    }

    /// @copydoc top_left
    [[nodiscard]] Kinesis::AABB2D top_right(float w, float h, float margin = 0.02F) const noexcept
    {
        return { .min = { 1.F - margin - w, 1.F - margin - h },
            .max = { 1.F - margin, 1.F - margin } };
    }

    /// @copydoc top_left
    [[nodiscard]] Kinesis::AABB2D bottom_left(float w, float h, float margin = 0.02F) const noexcept
    {
        return { .min = { -1.F + margin, -1.F + margin },
            .max = { -1.F + margin + w, -1.F + margin + h } };
    }

    /// @copydoc top_left
    [[nodiscard]] Kinesis::AABB2D bottom_right(float w, float h, float margin = 0.02F) const noexcept
    {
        return { .min = { 1.F - margin - w, -1.F + margin },
            .max = { 1.F - margin, -1.F + margin + h } };
    }

    /**
     * @brief Full-width strip of height @p h along the top edge.
     * @param h      Strip height in NDC units.
     * @param margin Inset from the top and side edges in NDC units.
     */
    [[nodiscard]] Kinesis::AABB2D top_strip(float h, float margin = 0.F) const noexcept
    {
        return { .min = { -1.F + margin, 1.F - margin - h },
            .max = { 1.F - margin, 1.F - margin } };
    }

    /// @copydoc top_strip
    [[nodiscard]] Kinesis::AABB2D bottom_strip(float h, float margin = 0.F) const noexcept
    {
        return { .min = { -1.F + margin, -1.F + margin },
            .max = { 1.F - margin, -1.F + margin + h } };
    }

    /**
     * @brief Full-height strip of width @p w along the left edge.
     * @param w      Strip width in NDC units.
     * @param margin Inset from the left and vertical edges in NDC units.
     */
    [[nodiscard]] Kinesis::AABB2D left_strip(float w, float margin = 0.F) const noexcept
    {
        return { .min = { -1.F + margin, -1.F + margin },
            .max = { -1.F + margin + w, 1.F - margin } };
    }

    /// @copydoc left_strip
    [[nodiscard]] Kinesis::AABB2D right_strip(float w, float margin = 0.F) const noexcept
    {
        return { .min = { 1.F - margin - w, -1.F + margin },
            .max = { 1.F - margin, 1.F - margin } };
    }

    /// @brief Rect of size (@p w, @p h) centered on the NDC origin.
    [[nodiscard]] Kinesis::AABB2D center_rect(float w, float h) const noexcept
    {
        return { .min = { -w * 0.5F, -h * 0.5F }, .max = { w * 0.5F, h * 0.5F } };
    }

    /// @brief The entire NDC surface.
    [[nodiscard]] Kinesis::AABB2D full() const noexcept
    {
        return { .min = glm::vec2(-1.F), .max = glm::vec2(1.F) };
    }

private:
    struct WindowOwnership {
        explicit WindowOwnership(std::shared_ptr<Core::Window> value)
            : window(std::move(value))
        {
        }

        std::shared_ptr<Core::Window> window;
    };

    std::shared_ptr<WindowOwnership> m_window_ownership;
    std::shared_ptr<Layer> m_layer;
    std::shared_ptr<Context> m_ctx;
};

} // namespace MayaFlux::Portal::Forma
