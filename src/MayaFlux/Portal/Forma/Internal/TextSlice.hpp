#pragma once

#include "MayaFlux/Portal/Forma/Target.hpp"
#include "MayaFlux/Portal/Text/TextRun.hpp"

namespace MayaFlux::Portal::Forma::internal {

/**
 * @class TextSlice
 * @brief Text drawn as glyph quads into one FormaTarget.
 *
 * Owns a Text::TextRun and writes its vertices, an optional tail of extra
 * vertices after them, and the atlas image binding to the target. The
 * target's buffer must be created with a texture slot at index 0 (the atlas
 * is bound there). Layout pixels map onto the bounds with
 * Text::GlyphMap::fit against the run's extent, so a caller that sets
 * render_bounds to the pixel size of the bounds, at least one line high, gets
 * one layout pixel per screen pixel; shorter bounds squeeze the text to fit.
 *
 * Every change rewrites the target's whole geometry. When an atlas image is
 * replaced, because the atlas grew or was swapped, every live slice drawing
 * from that atlas is written again, so no text is left on stale UVs. A slice
 * whose target has been removed is left alone.
 *
 * Always held through shared_ptr, made with make().
 */
class MAYAFLUX_API TextSlice {
public:
    /**
     * @brief Draw @p text in @p bounds on @p target.
     *
     * @param target Where the vertices go.
     * @param bounds NDC region the run's render_bounds are fitted onto.
     * @param params Colour, spans, background, render_bounds and atlas.
     * @param text   UTF-8 text.
     */
    [[nodiscard]] static std::shared_ptr<TextSlice> make(
        FormaTarget target,
        Kinesis::AABB2D bounds,
        Text::PressParams params,
        std::string_view text);

    /// @brief Replace the text.
    void set(std::string_view text);

    /// @brief Replace text and parameters together.
    void assign(std::string_view text, Text::PressParams params);

    /// @brief Move the text to a new region.
    void set_bounds(Kinesis::AABB2D bounds);

    /// @brief Replace the extra vertices drawn after the glyphs, such as a caret.
    void set_tail(std::vector<Kakshya::MeshVertex> tail);

    [[nodiscard]] Text::TextRun& run() noexcept { return m_run; }
    [[nodiscard]] const Text::TextRun& run() const noexcept { return m_run; }
    [[nodiscard]] Kinesis::AABB2D bounds() const noexcept { return m_bounds; }

    /// @brief Layout pixels to NDC for the current bounds.
    [[nodiscard]] Text::GlyphMap map() const noexcept;

    /// @brief Write the current state to the target. No-op once the target is gone.
    void refresh();

private:
    [[nodiscard]] bool alive() const;

    TextSlice(FormaTarget target, Kinesis::AABB2D bounds, Text::PressParams params);

    FormaTarget m_target;
    Kinesis::AABB2D m_bounds;
    Text::TextRun m_run;
    std::vector<Kakshya::MeshVertex> m_tail;
    std::shared_ptr<Core::VKImage> m_bound_image;
};

} // namespace MayaFlux::Portal::Forma::internal
