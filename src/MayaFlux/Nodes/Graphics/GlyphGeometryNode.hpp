#pragma once

#include "GeometryWriterNode.hpp"

#include "MayaFlux/Portal/Text/GlyphOutline.hpp"

namespace MayaFlux::Portal::Text {
class FontFace;
}

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GlyphGeometryNode
 * @brief GeometryWriterNode that emits glyph vector outlines as LINE_LIST geometry.
 *
 * Decomposes a UTF-8 string into per-glyph polyline outlines via
 * Portal::Text::decompose_glyph(), lays them out using the same pen-advance
 * model as lay_out(), and writes all contours as LINE_LIST vertex pairs into
 * the inherited vertex buffer.
 *
 * Each contour segment becomes two LineVertex entries (one per endpoint),
 * carrying the per-glyph color and thickness set on this node. The caller
 * drives per-character transforms by subclassing and overriding compute_frame(),
 * or by mutating the outline data between calls to rebuild_outlines().
 *
 * The outline geometry is static after set_text() / rebuild_outlines() until
 * either is called again. compute_frame() is a no-op unless m_dirty is set,
 * so this node is RT-safe to tick at VISUAL_RATE once built.
 *
 * Coordinate space: pixel-space with Y downward, pen origin at (pen_x, pen_y).
 * Matches GlyphQuad convention. The caller applies NDC transform via the view
 * transform on the buffer's RenderProcessor.
 *
 * Usage:
 * @code
 * Portal::Text::set_default_font("JetBrains Mono", "Medium", 72);
 *
 * auto node = std::make_shared<GlyphGeometryNode>("hello");
 * node->set_color({ 1.0F, 1.0F, 1.0F });
 * node->set_thickness(1.5F);
 *
 * auto buf = vega.GeometryBuffer(node) | Graphics;
 * buf->setup_rendering({
 *     .target_window = window,
 *     .topology = Portal::Graphics::PrimitiveTopology::LINE_LIST,
 * });
 * @endcode
 */
class MAYAFLUX_API GlyphGeometryNode : public GeometryWriterNode {
public:
    /**
     * @brief Construct from a UTF-8 string using the default font.
     *
     * Calls rebuild_outlines() immediately. Requires set_default_font() to
     * have been called before construction.
     *
     * @param text       UTF-8 string to decompose.
     * @param pen_x      Starting horizontal pen position in pixels.
     * @param pen_y      Starting vertical pen position in pixels (baseline).
     * @param tolerance  Bezier flatness tolerance in pixels.
     */
    explicit GlyphGeometryNode(
        std::string text,
        float pen_x = 0.F,
        float pen_y = 0.F,
        float tolerance = 0.5F);

    /**
     * @brief Construct from a UTF-8 string with an explicit FontFace.
     *
     * @param face       FontFace to decompose from. Must outlive this node.
     * @param pixel_size Glyph size in pixels.
     * @param text       UTF-8 string to decompose.
     * @param pen_x      Starting horizontal pen position in pixels.
     * @param pen_y      Starting vertical pen position in pixels (baseline).
     * @param tolerance  Bezier flatness tolerance in pixels.
     */
    GlyphGeometryNode(
        Portal::Text::FontFace& face,
        uint32_t pixel_size,
        std::string text,
        float pen_x = 0.F,
        float pen_y = 0.F,
        float tolerance = 0.5F);

    ~GlyphGeometryNode() override = default;

    /**
     * @brief Replace the text string and rebuild outlines.
     * @param text UTF-8 string.
     */
    void set_text(std::string text);

    /**
     * @brief Set pen origin for layout.
     *
     * Clears any active center point. Takes effect on the next
     * rebuild_outlines() or set_text() call.
     */
    void set_pen(float pen_x, float pen_y);

    /**
     * @brief Place the text block so its bounding box center falls on @p center.
     *
     * Overrides the pen origin. The bounding box is computed from the total
     * advance width and line height of the laid-out string. Takes effect on
     * the next rebuild_outlines() or set_text() call. Pass std::nullopt to
     * revert to pen-origin placement.
     *
     * @param center  Desired center position in the same coordinate space as
     *                the pen origin (pixels when using an ortho view transform).
     */
    void set_center(std::optional<glm::vec2> center);

    /**
     * @brief Set uniform color for all glyph contours.
     * @param color RGB color in [0, 1].
     */
    void set_color(glm::vec3 color);

    /**
     * @brief Set uniform line thickness for all glyph contours.
     * @param thickness Thickness value passed to LineVertex.
     */
    void set_thickness(float thickness);

    /**
     * @brief Access the per-glyph outline data produced by the last rebuild.
     *
     * The vector is ordered by codepoint occurrence in the source string,
     * matching the layout order. Callers may read (not write) this data to
     * drive per-glyph transforms externally.
     */
    [[nodiscard]] const std::vector<Portal::Text::GlyphOutline>& outlines() const
    {
        return m_outlines;
    }

    /**
     * @brief Pen-advance offsets for each glyph, in pixels.
     *
     * outlines()[i] starts at pen_offsets()[i]. Useful for mapping node graph
     * outputs (e.g. Phasor index) to a specific glyph's screen position.
     */
    [[nodiscard]] const std::vector<glm::vec2>& pen_offsets() const { return m_pen_offsets; }

    /**
     * @brief Override pen offsets for all glyphs.
     *
     * Each element corresponds to outlines()[i]. The caller computes
     * whatever positions it wants (wave, noise, physics) and sets them
     * here. write_vertices() uses these offsets on the next compute_frame().
     *
     * @param offsets Must have the same size as outlines(). Silently ignored
     *                if sizes do not match.
     */
    void set_pen_offsets(std::vector<glm::vec2> offsets);

    /**
     * @brief Rebuild outline geometry from the current text and pen settings.
     *
     * Called automatically by set_text(). Call directly after mutating pen,
     * color, or thickness without changing text.
     */
    void rebuild_outlines();

    /**
     * @brief Upload cached vertex data if dirty. No-op otherwise.
     */
    void compute_frame() override;

    [[nodiscard]] Portal::Graphics::PrimitiveTopology get_primitive_topology() const override
    {
        return Portal::Graphics::PrimitiveTopology::LINE_LIST;
    }

private:
    Portal::Text::FontFace* m_face {};
    uint32_t m_pixel_size {};
    std::string m_text;
    float m_pen_x {};
    float m_pen_y {};
    float m_tolerance { 0.5F };
    std::optional<glm::vec2> m_center;
    float m_total_advance {};
    float m_line_height {};

    glm::vec3 m_color { 1.F, 1.F, 1.F };
    float m_thickness { 1.F };

    std::vector<Portal::Text::GlyphOutline> m_outlines;
    std::vector<glm::vec2> m_pen_offsets;

    bool m_dirty {};

    void write_vertices();
};

} // namespace MayaFlux::Nodes::GpuSync
