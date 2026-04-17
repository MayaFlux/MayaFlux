#pragma once

#include "GlyphAtlas.hpp"
#include "MayaFlux/Buffers/Textures/TextBuffer.hpp"

namespace MayaFlux::Portal::Text {

struct GlyphQuad;

/**
 * @brief Policy controlling TextBuffer reuse behaviour in repress().
 */
enum class RedrawPolicy : uint8_t {
    Clip, ///< Replace content. Truncate to existing budget bounds if text exceeds them.
    Fit ///< Replace content. Reallocate GPU texture if text exceeds existing budget.
};

/**
 * @brief Result of an impress() call.
 *
 * Ok and Overflow are both success states from the caller's perspective --
 * the text was composited in both cases. Overflow additionally signals that
 * the vertical budget was exceeded, the texture was reallocated, and all
 * previous content has been cleared. The caller is responsible for
 * rebuilding the full accumulated string if continuity is required.
 */
enum class ImpressResult : uint8_t {
    Ok, ///< Run composited at cursor. No GPU state change.
    Overflow ///< Vertical budget exceeded. Texture reallocated. Previous content cleared.
};

/**
 * @brief Construction parameters for press().
 *
 * Atlas and render bounds are fixed at press() time and owned by the
 * resulting TextBuffer. repress() and impress() read them from the buffer
 * and do not accept a PressParams argument.
 *
 * budget_h controls the pre-allocated texture height. When zero the
 * framework applies a k_grow_height_multiplier heuristic over the initial
 * content height to reduce early vertical reallocation under growing text.
 * Set to a concrete value to skip the heuristic and allocate exactly.
 *
 * Width is always allocated to render_bounds.x -- the layout engine wraps
 * at that boundary, so no content can ever exceed it and a narrower
 * horizontal allocation serves no purpose.
 */
struct PressParams {
    /// @brief Glyph atlas to use. Null selects the TypeFaceFoundry default at call time.
    GlyphAtlas* atlas { nullptr };

    /// @brief RGBA color applied to all glyphs.
    glm::vec4 color { 1.F, 1.F, 1.F, 1.F };

    /// @brief Hard render bounds in pixels.
    ///        impress() wraps at x and returns Overflow when y is exhausted.
    glm::uvec2 render_bounds { 1280, 720 };

    /// @brief Initial vertical budget in pixels. Zero applies the grow heuristic.
    uint32_t budget_h { 0 };
};

/**
 * @brief Write glyph quads into a caller-provided RGBA8 pixel buffer.
 *
 * Applies coverage-multiplied alpha blend per glyph cell. The destination
 * buffer must be row-major RGBA8 with stride == buf_w * 4 bytes. Quads
 * that fall outside [0, buf_w) x [0, buf_h) are clipped per pixel.
 *
 * The typical usage pattern is:
 * @code
 * auto layout = lay_out(text, atlas, 0.F, 0.F, wrap_w);
 * for (auto& q : layout.quads) { ... } // per-character transforms
 * rasterize_quads(layout.quads, atlas, color, pixels, w, h);
 * @endcode
 *
 * @param quads   Quads produced by lay_out(), optionally mutated by the caller.
 * @param atlas   Source atlas for coverage bitmaps.
 * @param color   RGBA glyph color in [0, 1].
 * @param dst     Destination RGBA8 buffer. Stride is buf_w * 4 bytes.
 * @param buf_w   Buffer width in pixels.
 * @param buf_h   Buffer height in pixels.
 */
MAYAFLUX_API void rasterize_quads(
    std::span<const GlyphQuad> quads,
    GlyphAtlas& atlas,
    glm::vec4 color,
    uint8_t* dst,
    uint32_t buf_w,
    uint32_t buf_h);

/**
 * @brief Rasterize a mutated quad span into an existing TextBuffer.
 *
 * Clears the buffer's pixel region, rasterizes @p quads via rasterize_quads(),
 * and marks the buffer dirty for GPU upload. The scratch pixel buffer is
 * thread-local and reused across calls, so no heap allocation occurs after
 * the first call at a given buffer size.
 *
 * Typical usage:
 * @code
 * auto layout = Portal::Text::create_layout(text, 0.F, 0.F, wrap_w);
 * // ... per-quad mutation ...
 * Portal::Text::ink_quads(text_buf, layout->quads, color);
 * @endcode
 *
 * @param target  TextBuffer to write into. Dimensions are read from the buffer.
 * @param quads   Quads produced by create_layout(), optionally mutated by the caller.
 * @param color   RGBA glyph color in [0, 1].
 */
MAYAFLUX_API void ink_quads(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::span<const GlyphQuad> quads,
    glm::vec4 color);

/**
 * @brief Composite a UTF-8 string into a new TextBuffer.
 *
 * The returned TextBuffer has width == params.render_bounds.x and height
 * equal to either the explicit params.budget_h or the heuristic initial
 * allocation, whichever is larger than the content height.
 *
 * @param text    UTF-8 string to composite.
 * @param params  Construction parameters. Default produces a growing buffer
 *                at 1280x720 render bounds using the default atlas.
 * @return        Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    const PressParams& params = {});

/**
 * @brief Re-composite a UTF-8 string into an existing TextBuffer.
 *
 * Always clears the buffer before compositing. Render bounds and atlas are
 * read from the target buffer. When the target has a pre-allocated vertical
 * budget the compositing bounds are the budget dimensions, so no VKImage
 * rebuild occurs as long as content fits.
 *
 * @param target  Existing TextBuffer to write into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param policy  Controls reallocation behaviour when text exceeds budget.
 * @return        True on success.
 */
MAYAFLUX_API bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    RedrawPolicy policy = RedrawPolicy::Clip);

/**
 * @brief Append a UTF-8 string into an existing TextBuffer at the current cursor.
 *
 * Does not clear existing content. Composites the new run into the
 * pre-allocated budget region and advances the cursor. No VKImage
 * reallocation occurs while the run fits within the vertical budget.
 *
 * When the run would push the cursor past the vertical budget the texture
 * is grown by k_grow_height_multiplier, the full accumulated text is
 * recomposited, and ImpressResult::Overflow is returned. The caller is
 * responsible for rebuilding prior content after an overflow if accumulated
 * state is not sufficient.
 *
 * When the cursor would exceed render_bounds_h the run is rejected and
 * ImpressResult::Overflow is returned without reallocation.
 *
 * The atlas is always the TypeFaceFoundry default. If a non-default atlas
 * was used at press() time, impress() will use the default instead. Mixing
 * atlases on the same TextBuffer is undefined behaviour.
 *
 * @param target  Existing TextBuffer to append into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @return        ImpressResult::Ok or ImpressResult::Overflow.
 */
MAYAFLUX_API ImpressResult impress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

} // namespace MayaFlux::Portal::Text
