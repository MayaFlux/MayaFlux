#pragma once

#include "Mapped.hpp"

#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TextEdit.hpp"

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

class Context;
class Surface;

/**
 * @struct TextField
 * @brief An editable text region: keyboard/text-input wiring over an
 *        Element the caller already has a buffer for.
 *
 * Before place() is called, the struct carries no state. After place(),
 * element_id, state, params, and field_bounds are populated. This mirrors
 * the Scrollable/Collapsible pattern: place() takes a pre-built buffer
 * (Portal::Forma::create_buffer at the call site) and does not itself
 * depend on Forma.hpp - no file under Portal/Forma/ may, except Forma.cpp
 * itself. Portal::Forma::create_text_field (in Forma.hpp) is the one-call
 * convenience that builds the buffer and delegates here, the same split
 * Portal::Forma::create<T> already has between itself and a Form<T>.
 *
 * Editing state lives in a Portal::Text::EditableText, mutated through
 * Portal::Text::move()/erase()/insert_codepoint()/insert_literal() - this
 * struct only wires Context callbacks to those operations and re-renders
 * via Element::set_text. It does not manage focus visuals; wire
 * on_focus_gained/on_focus_lost separately, same as any other Forma
 * element.
 *
 * params is a shared, mutable Portal::Text::PressParams: every keystroke's
 * re-render reads *params fresh, so a caller can change it once (e.g. a
 * focus-driven background color) and have every subsequent keystroke see
 * it, without re-wiring anything.
 *
 * @code
 * auto buf = Portal::Forma::create_buffer(
 *     window, Graphics::PrimitiveTopology::TRIANGLE_LIST,
 *     std::vector{ std::pair{ std::string("text"), std::shared_ptr<Core::VKImage>{} } });
 *
 * auto params = std::make_shared<Portal::Text::PressParams>(
 *     Portal::Text::PressParams { .color = { 0.9F, 0.9F, 0.9F, 1.F }, .render_bounds = { 800, 64 } });
 *
 * auto field = TextField {}.place(buf, surface, bounds, params, "hello");
 * @endcode
 */
struct TextField {
    // =========================================================================
    // Results - populated by place()
    // =========================================================================

    /// @brief Element id of the text field. Valid after place().
    uint32_t element_id {};

    /// @brief Live editable text and cursor. Valid after place().
    std::shared_ptr<MappedState<Portal::Text::EditableText>> state;

    /// @brief Shared, mutable render params - every re-render reads this
    ///        fresh, so a caller can change it once and have every
    ///        subsequent keystroke see it. Valid after place().
    std::shared_ptr<Portal::Text::PressParams> params;

    /// @brief NDC region occupied by the field. Valid after place().
    Kinesis::AABB2D field_bounds {};

    /// @brief Field region, satisfying Kinesis::HasBounds.
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return field_bounds; }

    // =========================================================================
    // Placement
    // =========================================================================

    /**
     * @brief Register the text field element and wire text editing onto it.
     *
     * @param buf          Pre-created buffer for the text quad - must have
     *                     an additional_textures slot at index 0
     *                     (Element::with_text's requirement).
     * @param surface      Surface to register the element on.
     * @param bounds       NDC region: hit-test bounds and the text quad.
     * @param in_params    Shared render params. A default-constructed one
     *                     is allocated if null.
     * @param initial_text Starting text. Cursor starts at its end.
     * @return *this, with results populated.
     */
    MAYAFLUX_API TextField& place(
        std::shared_ptr<Buffers::FormaBuffer> buf,
        Surface& surface,
        Kinesis::AABB2D bounds,
        std::shared_ptr<Portal::Text::PressParams> in_params,
        std::string initial_text = {});

private:
    Element m_element;

    void wire(Context& ctx);
};

} // namespace MayaFlux::Portal::Forma
