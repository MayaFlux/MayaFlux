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
 * Ok and Overflow are both success states from the caller's perspective:
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
 * @brief A colored byte range within the text passed to a composite call.
 *
 * start/end are byte offsets into that call's own text argument, matching
 * GlyphQuad::byte_offset as produced by lay_out() for that same text, not
 * offsets into any longer accumulated history. Quads whose byte_offset falls
 * in [min(start,end), max(start,end)) are rasterized in this span's color
 * instead of the call's base color. Spans may overlap; where they do, the
 * later entry in the vector wins for the overlapping quads. Quads matched by
 * no span keep the call's base color.
 */
struct StyleSpan {
    size_t start;
    size_t end;
    glm::vec4 color;
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
 * Width is always allocated to render_bounds.x, since the layout engine wraps
 * at that boundary, so no content can ever exceed it and a narrower
 * horizontal allocation serves no purpose.
 */
struct PressParams {
    /// @brief Glyph atlas to use. Null selects the TypeFaceFoundry default at call time.
    GlyphAtlas* atlas { nullptr };

    /// @brief RGBA color applied to all glyphs not covered by a span in `spans`.
    glm::vec4 color { 1.F, 1.F, 1.F, 1.F };

    /**
     * @brief RGBA fill composited beneath every glyph. Default fully
     *        transparent, matching behavior before this field existed.
     *
     * Stored on the resulting TextBuffer (TextBuffer::set_background) at
     * press() time, the same way atlas and render bounds persist:
     * repress(TextBuffer&, ...) and impress() read it back automatically on
     * every subsequent call, so a caller sets it once. The VKImage-returning
     * overloads (press(text, render_bounds, params), repress(VKImage&, ...))
     * have no buffer to persist it on, so those read background fresh from
     * @p params on every call instead.
     */
    glm::vec4 background { 0.F, 0.F, 0.F, 0.F };

    /// @brief Hard render bounds in pixels.
    ///        impress() wraps at x and returns Overflow when y is exhausted.
    glm::uvec2 render_bounds { 1280, 720 };

    /// @brief Initial vertical budget in pixels. Zero applies the grow heuristic.
    uint32_t budget_h { 0 };

    /// @brief Optional per-byte-range color overrides. Empty applies `color` uniformly.
    std::vector<StyleSpan> spans;
};

/**
 * @brief Write glyph quads into a caller-provided RGBA8 pixel buffer.
 *
 * Applies coverage-multiplied alpha blend per glyph cell, compositing over
 * whatever is already in @p dst rather than overwriting it - a glyph's
 * bounding box includes its own zero-coverage interior (the hole in an
 * 'o' or 'e'), so composited blending is what keeps a pre-filled
 * background intact under that hole instead of a low-coverage pixel
 * punching it transparent. The destination buffer must be row-major RGBA8
 * with stride == buf_w * 4 bytes. Quads that fall outside
 * [0, buf_w) x [0, buf_h) are clipped per pixel.
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
 * Clears the buffer's pixel region to target->get_background(), rasterizes
 * @p quads via rasterize_quads(), and marks the buffer dirty for GPU
 * upload. The scratch pixel buffer is thread-local and reused across
 * calls, so no heap allocation occurs after the first call at a given
 * buffer size.
 *
 * Typical usage:
 * @code
 * auto layout = Portal::Text::create_layout(text, 0.F, 0.F, wrap_w);
 * // ... per-quad mutation ...
 * Portal::Text::ink_quads(text_buf, layout->quads, color);
 * @endcode
 *
 * @p quads' UV coordinates are only meaningful against the atlas they were
 * laid out from. @p atlas null (the default) resolves to whatever the
 * TypeFaceFoundry default atlas is *at this call*, not at layout time: if
 * something else has called set_default_font() since, or the atlas has
 * grown, a held LayoutResult's quads will sample the wrong texture. Pass
 * the same atlas the quads were laid out against explicitly whenever it
 * might not still be the current default by the time this runs.
 *
 * @param target  TextBuffer to write into. Dimensions are read from the buffer.
 * @param quads   Quads produced by create_layout(), optionally mutated by the caller.
 * @param color   RGBA glyph color in [0, 1].
 * @param atlas   Atlas the quads' UVs are relative to. Null selects the
 *                TypeFaceFoundry default at call time.
 */
MAYAFLUX_API void ink_quads(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::span<const GlyphQuad> quads,
    glm::vec4 color,
    GlyphAtlas* atlas = nullptr);

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
 * @brief Composite a UTF-8 string directly into a new GPU texture.
 *
 * No TextBuffer, no quad geometry, no registration required.
 * The returned VKImage is RGBA8, shader-read layout, ready for binding
 * into a FormaBuffer or any other descriptor slot.
 *
 * @param text          UTF-8 string to composite.
 * @param render_bounds Texture dimensions and wrap boundary in pixels.
 * @param params        atlas, color, budget_h. render_bounds field ignored.
 * @param staging       Persistent host-visible staging buffer to upload
 *                       through. Sized by the caller beforehand (e.g. via
 *                       Buffers::ensure_image_staging_capacity()) and passed
 *                       again on every call for the same label/texture to
 *                       avoid allocating a fresh one each time; nullptr
 *                       falls back to an internal one-shot staging
 *                       allocation (the prior behaviour).
 * @return              GPU-resident VKImage, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Core::VKImage> press(
    std::string_view text,
    glm::uvec2 render_bounds,
    const PressParams& params = {},
    const std::shared_ptr<Buffers::VKBuffer>& staging = nullptr);

/**
 * @brief Re-composite a UTF-8 string into an existing TextBuffer.
 *
 * Always clears the buffer to target->get_background() before compositing.
 * Render bounds and atlas are read from the target buffer. When the target
 * has a pre-allocated vertical budget the compositing bounds are the budget
 * dimensions, so no VKImage rebuild occurs as long as content fits.
 *
 * @param target  Existing TextBuffer to write into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color for any byte not covered by @p spans.
 * @param policy  Controls reallocation behaviour when text exceeds budget.
 * @param spans   Optional per-byte-range color overrides, offsets into @p text.
 * @return        True on success.
 */
MAYAFLUX_API bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    RedrawPolicy policy = RedrawPolicy::Clip,
    std::span<const StyleSpan> spans = {});

/**
 * @brief Re-composite a UTF-8 string into an existing GPU texture.
 *
 * Uploads in-place when content fits within the existing image dimensions.
 * Reallocates and updates @p target when content height exceeds image height.
 *
 * @param target  VKImage to update. May be replaced on reallocation.
 * @param text    UTF-8 string to composite.
 * @param params  render_bounds, atlas, color.
 * @param staging Persistent host-visible staging buffer to upload through.
 *                Sized by the caller beforehand (e.g. via
 *                Buffers::ensure_image_staging_capacity()) and passed again
 *                on every call for the same @p target to avoid allocating a
 *                fresh one each time; nullptr falls back to an internal
 *                one-shot staging allocation (the prior behaviour).
 * @return        True on success.
 */
MAYAFLUX_API bool repress(
    std::shared_ptr<Core::VKImage>& target,
    std::string_view text,
    const PressParams& params = {},
    const std::shared_ptr<Buffers::VKBuffer>& staging = nullptr);

/**
 * @brief Append a UTF-8 string into an existing TextBuffer at the current cursor.
 *
 * Does not clear existing content. Composites the new run into the
 * pre-allocated budget region and advances the cursor. No VKImage
 * reallocation occurs while the run fits within the vertical budget.
 *
 * When the run would push the cursor past the vertical budget the texture
 * is grown by k_grow_height_multiplier, the full accumulated text is
 * recomposited against target->get_background(), and ImpressResult::Overflow
 * is returned. The caller is responsible for rebuilding prior content after
 * an overflow if accumulated state is not sufficient.
 *
 * When the cursor would exceed render_bounds_h the run is rejected and
 * ImpressResult::Overflow is returned without reallocation.
 *
 * The atlas is always the TypeFaceFoundry default. If a non-default atlas
 * was used at press() time, impress() will use the default instead. Mixing
 * atlases on the same TextBuffer is undefined behaviour.
 *
 * @p spans is offsets into this call's @p text, matching the byte_offset
 * lay_out() assigns for this run. On the Overflow-and-regrow path the full
 * accumulated text is recomposited using @p color and @p spans from this
 * call alone; prior runs' own colors/spans are not retained, the same
 * pre-existing limitation @p color already has on that path.
 *
 * @param target  Existing TextBuffer to append into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color for any byte not covered by @p spans.
 * @param spans   Optional per-byte-range color overrides, offsets into @p text.
 * @return        ImpressResult::Ok or ImpressResult::Overflow.
 */
MAYAFLUX_API ImpressResult impress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    std::span<const StyleSpan> spans = {});

} // namespace MayaFlux::Portal::Text
