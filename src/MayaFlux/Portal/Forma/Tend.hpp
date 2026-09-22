#pragma once

#include "Internal/Atelier.hpp"
#include "Primitives/Geometry.hpp"
#include "Primitives/TextField.hpp"
#include "Surface.hpp"

namespace MayaFlux::Portal::Forma {

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
    return internal::atelier().create_element<T>(
        layer, std::move(window), std::move(geom), std::move(initial),
        topology, capacity, std::move(project));
}

/**
 * @brief Realize a Form on @p surface.
 *
 * Builds the buffer at the Form's capacity and topology, registers the
 * element, syncs so the geometry function's bounds_hint and contains reach
 * the Layer before the first frame, then runs the Form's interaction wiring.
 *
 * A bare GeometryFn converts to a Form, so this also serves as the simplest
 * element construction path for a caller's own geometry.
 *
 * @tparam T       MappedState value type.
 * @param surface  Canvas to register on.
 * @param form     Geometry, topology, capacity, and interaction.
 * @param initial  Starting value written into MappedState.
 * @param project  Optional T to float projection for outbound readers.
 * @return Fully constructed Mapped<T>, wired.
 */
template <typename T>
[[nodiscard]] Mapped<T> create(
    Surface& surface,
    Geometry::Form<T> form,
    T initial,
    std::function<float(T)> project = {})
{
    auto mapped = internal::atelier().create_element<T>(
        surface.layer(), surface.window(),
        std::move(form.geometry), std::move(initial),
        form.topology, form.capacity, std::move(project));

    mapped.sync(&surface.layer());

    if (form.wire)
        form.wire(surface.ctx(), mapped.element.id, mapped.state);

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
    return internal::atelier().create_element<T>(
        surface, std::move(geom), std::move(initial), topology, std::move(project));
}

/**
 * @brief Build a text-capable FormaBuffer, register a TextField, and wire
 *        text editing onto it in one call.
 *
 * The one-call convenience over TextField::place(), the same split
 * Portal::Forma::create<T> already has with a bare Form<T>. This builds
 * the buffer (create_buffer, which TextField.cpp itself may never call;
 * see TextField.hpp) and delegates. Build the buffer and TextField by hand
 * instead for anything needing a non-default buffer setup (a shared
 * texture slot alongside a background quad, for instance).
 *
 * @p scrollable composes a second primitive in, the same way: false (the
 * default) matches every prior call exactly, and @p bounds is the field's
 * own region. True builds a plain Scrollable (Scrollable::place(), @p bounds
 * as its viewport) and calls TextField::scrollable() instead of place(),
 * the same composition test_text_input() (test_8.hpp) once wired by hand,
 * now living at the TextField primitive level; this is only the one-call
 * convenience over it. That Scrollable is local to this call and not
 * returned, so no indicator is added and nothing else can be tracked into
 * the same viewport. Build the Scrollable yourself and call
 * TextField::scrollable() directly (or place() + Scrollable::track() by
 * hand) for either of those.
 *
 * @param surface      Surface whose window, layer, and context own the field.
 * @param bounds       NDC region for both hit testing and the text quad,
 *                     or, when scrollable, the fixed clipped viewport.
 * @param params       Shared render params. A default-constructed one is
 *                     allocated if null.
 * @param initial_text Starting text. Cursor starts at its end.
 * @param scrollable   When true, wraps the field in a Scrollable viewport.
 * @return Registered, wired TextField. Portal::Forma::destroy(surface,
 *         field.element_id) is sufficient to tear the whole thing down,
 *         scrollable viewport included when @p scrollable is true. No
 *         manual multi-id cleanup is needed.
 */
[[nodiscard]] MAYAFLUX_API TextField create_text_field(
    Surface& surface,
    Kinesis::AABB2D bounds,
    std::shared_ptr<Portal::Text::PressParams> params = nullptr,
    std::string initial_text = {},
    bool scrollable = false);

/**
 * @struct Tend
 * @brief The fourth member of the Collapsible/Scrollable/TextField family,
 *        for a bare Form<T> control.
 *
 * Configure with form/initial, then place(Surface&) - same as all three
 * siblings, Tend never builds a Surface itself, it only ever wraps one the
 * caller already has. place() can only call Portal::Forma::create(), never
 * create_element(), so the wire-dropping mistake create_element<T>'s
 * bare-GeometryFn overload allows is structurally unreachable through Tend.
 *
 * place() retains a copy of the Surface it was given in the surface member
 * (cheap - Surface's own three fields are shared_ptrs, so the copy shares
 * the same Layer/Context/window as the original), so ctx()/layer()/window()
 * stay reachable afterward without holding a separate reference of your own.
 *
 * Accepting a SurfaceConfig or a window description directly, so a caller
 * with nothing yet does not need to call create_surface() themselves first,
 * is deliberately not this struct's job - that escalation belongs to a
 * higher-level composer (a future Creator-level tend_forma), which can
 * resolve either shape into a Surface via Atelier::create_surface's own two
 * overloads and hand the result to place() same as any other caller would.
 *
 * place() alone stops at construction. bridge() and track() are what keep
 * going without falling out of the chain into a separate statement:
 *
 * @code
 * auto fader = Tend<float>{
 *     .form = Geometry::horizontal_fader(track, 0.04F),
 *     .initial = 0.5F,
 * }.place(surface)
 *  .bridge().write(vega.Constant(0.5) | Audio[0]);
 * @endcode
 *
 * @tparam T MappedState value type.
 */
template <typename T>
struct Tend {
    /// @brief Geometry, topology, capacity, and interaction. Consumed by place().
    Geometry::Form<T> form;

    /// @brief Starting value written into MappedState. Consumed by place().
    T initial {};

    /// @brief Populated by place(). The real, fully wired element.
    std::optional<Mapped<T>> result;

    /// @brief Populated by place() with a copy of the Surface it was given,
    ///        so ctx()/layer()/window() stay reachable afterward.
    std::optional<Surface> surface;

    /**
     * @brief Realize form on @p target.
     * @param target Canvas to register on. A copy is retained in surface.
     * @return *this, with surface and result populated.
     */
    Tend& place(Surface& target)
    {
        surface = target;
        result = Portal::Forma::create<T>(target, std::move(form), std::move(initial));
        return *this;
    }

    /**
     * @brief Bridge::Binding handle for result's element.
     *
     * Thin forwarder to Bridge::at(). It keeps the chain unbroken rather than
     * requiring a separate Portal::Forma::bridge().at(...) statement after
     * place(). Every real bind()/write()/unbind() overload lives on the
     * returned Binding itself, unchanged. Nothing here duplicates them.
     * Calls internal::atelier().bridge() rather than the public
     * Portal::Forma::bridge() wrapper, since that free function is declared
     * in Forma.hpp, which this file cannot include.
     *
     * @pre place() has been called.
     * @return Binding handle, chainable into .bind(...)/.write(...).
     */
    [[nodiscard]] Bridge::Binding bridge()
    {
        return internal::atelier().bridge().at(result->state);
    }

    /**
     * @brief Register result's element for scroll reflow in @p panel.
     *
     * Thin forwarder to Atelier::track<T>(). See its own doc for what the
     * default (built when @p reposition is omitted) does and why it
     * captures only independently-owned pieces of result, never a pointer
     * back to Tend/result itself.
     *
     * @pre place() has been called.
     * @param panel       Scrollable to track into.
     * @param layer       Layer both panel and result's element were
     *                    registered on.
     * @param base_bounds result's current on-screen NDC bounds.
     * @param reposition  Called with the scrolled bounds on every scroll.
     *                    Defaulted when empty; see Atelier::track<T>().
     * @return *this.
     */
    Tend& track(
        Scrollable& panel,
        Layer& layer,
        Kinesis::AABB2D base_bounds,
        std::function<void(Kinesis::AABB2D)> reposition = {})
    {
        internal::atelier().track(panel, layer, *result, base_bounds, std::move(reposition));
        return *this;
    }
};

} // namespace MayaFlux::Portal::Forma
