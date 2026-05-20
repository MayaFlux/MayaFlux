#pragma once

#include "LayoutCursor.hpp"

namespace MayaFlux::Core {
class Window;
class VKImage;
}

namespace MayaFlux::Buffers {
class FormaBuffer;
}

namespace MayaFlux::Portal::Forma {

class Layer;
class Context;

/**
 * @struct Collapsible
 * @brief A collapsible header strip.
 *
 * Before place() is called, the struct carries only configuration.
 * After place(), header_id, open, buf, and cursor_out are populated.
 * This mirrors the Element pattern: configure with setters, register
 * with a single explicit call.
 *
 * @code
 * auto col = Collapsible{}
 *     .initially_open(false)
 *     .closed_color({ 0.2F, 0.2F, 0.2F })
 *     .label(header_img)
 *     .place(window, layer, ctx, cursor, x_min, x_max, row_h);
 *
 * col.attach(layer, body_id);
 * @endcode
 */
struct Collapsible {
    // =========================================================================
    // Results - populated by place()
    // =========================================================================

    /// @brief Element id of the header strip. Valid after place().
    uint32_t header_id {};

    /// @brief Open/closed state. Valid after place(). Write to toggle.
    std::shared_ptr<MappedState<bool>> open;

    /// @brief FormaBuffer backing the header geometry. Valid after place().
    std::shared_ptr<Buffers::FormaBuffer> buf;

    /// @brief Cursor positioned below the header, ready for body elements. Valid after place().
    LayoutCursor cursor_out;

    // =========================================================================
    // Configuration - set before place()
    // =========================================================================

    /// @brief Starting toggle state. Default: true (open).
    bool m_initially_open { true };

    /// @brief Background color when closed. Default: glm::vec3(0.25F).
    glm::vec3 m_color_closed { 0.25F };

    /// @brief Background color when open. Default: glm::vec3(0.35F).
    glm::vec3 m_color_open { 0.35F };

    /// @brief Optional GPU image overlaid as a text label. nullptr = color only.
    std::shared_ptr<Core::VKImage> m_label;

    // =========================================================================
    // Configuration setters
    // =========================================================================

    Collapsible& initially_open(bool v)
    {
        m_initially_open = v;
        return *this;
    }

    Collapsible& closed_color(glm::vec3 c)
    {
        m_color_closed = c;
        return *this;
    }

    Collapsible& open_color(glm::vec3 c)
    {
        m_color_open = c;
        return *this;
    }

    /**
     * @brief Attach a GPU image overlaid on the header as a text label.
     *
     * When set, the FormaBuffer is created with a "text" texture binding
     * and the geometry emits 12 vertices (background + overlay quad).
     * When null (default), a plain color-only buffer is used with 6 vertices.
     */
    Collapsible& label(std::shared_ptr<Core::VKImage> img)
    {
        m_label = std::move(img);
        return *this;
    }

    // =========================================================================
    // Placement
    // =========================================================================

    /**
     * @brief Register the header element and wire the toggle callback.
     *
     * Advances @p cursor by @p row_h. Populates header_id, open, buf,
     * and cursor_out on this struct.
     *
     * @param buf     Pre-created FormaBuffer sized for this header.
     * @param layer   Layer to register the header element on.
     * @param ctx     Context to wire the left-click toggle handler.
     * @param cursor  Layout cursor. Advanced by row_h on return.
     * @param x_min   Left edge in NDC.
     * @param x_max   Right edge in NDC.
     * @param row_h   Header strip height in NDC units.
     * @return *this, with results populated.
     */
    MAYAFLUX_API Collapsible& place(
        std::shared_ptr<Buffers::FormaBuffer> buf,
        Layer& layer,
        Context& ctx,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h);

    // =========================================================================
    // Post-placement helpers
    // =========================================================================

    /**
     * @brief Relate a body element to this collapsible and sync its visibility
     *        to the current open state.
     *
     * @param layer   Layer the body element lives on.
     * @param body_id Element id returned from layer.add(...).
     */
    MAYAFLUX_API void attach(Layer& layer, uint32_t body_id) const;
};

// =============================================================================
// Free function escape hatch
// =============================================================================

/**
 * @brief Construct a collapsible header strip.
 *
 * Thin wrapper delegating to Collapsible::place(). Preserved as an escape
 * hatch; existing callsites require no changes. The @p bridge parameter is
 * accepted but ignored - Forma::bridge() is called internally.
 */
[[nodiscard]] MAYAFLUX_API Collapsible make_collapsible(
    std::shared_ptr<Buffers::FormaBuffer> buf,
    Layer& layer,
    Context& ctx,
    LayoutCursor& cursor,
    float x_min,
    float x_max,
    float row_h,
    bool initially_open = true,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F),
    std::shared_ptr<Core::VKImage> label = nullptr);

} // namespace MayaFlux::Portal::Forma
