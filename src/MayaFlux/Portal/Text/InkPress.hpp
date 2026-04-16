#include "GlyphAtlas.hpp"
#include "MayaFlux/Buffers/Textures/TextBuffer.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @brief Policy controlling TextBuffer reuse behaviour in repress().
 */
enum class RedrawPolicy : uint8_t {
    Clip, ///< Replace content. Truncate to existing buffer bounds if text exceeds them.
    Fit ///< Replace content. Reallocate GPU texture if text exceeds existing bounds.
};

/**
 * @brief Result of an impress() call.
 *
 * Ok and Overflow are both success states from the caller's perspective --
 * the text was composited in both cases. Overflow additionally signals that
 * the budget was exceeded, repress(Fit) was called internally, and all
 * previous content has been cleared. The caller is responsible for
 * rebuilding the full string if continuity is required.
 */
enum class ImpressResult : uint8_t {
    Ok, ///< Run composited at cursor position. No GPU state change.
    Overflow ///< Budget exceeded. repress(Fit) called. Previous content cleared.
             ///< Caller must rebuild full string content if needed.
};

/**
 * @brief Composite a UTF-8 string into a new TextBuffer using the default GlyphAtlas.
 *
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param growing Whether the content is expected to grow on subsequent updates. If true, the
                  returned TextBuffer will have a budget pre-allocated x2 the width and x8 the height
                  of the initial content dimensions. This is a heuristic based on typical UI text update patterns,
                  and is intended to reduce VKImage reallocations and staging buffer uploads on subsequent updates
                  when the content grows moderately. Set to false to allocate a GPU texture with no additional budget.
 * @param render_bounds_h Hard vertical render bound in pixels. impress() returns Overflow when exceeded.
 * @param render_bounds_w Hard horizontal render bound in pixels. impress() wraps to next line when exceeded.
 * @return        Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    bool growing = true,
    uint32_t render_bounds_h = 720, uint32_t render_bounds_w = 1280);

/**
 * @brief Composite a UTF-8 string into a new TextBuffer with an explicit atlas.
 *
 * @param text    UTF-8 string to composite.
 * @param atlas   GlyphAtlas to source glyph bitmaps from.
 * @param color   RGBA glyph color.
 * @param growing Whether the content is expected to grow on subsequent updates. If true, the
                  returned TextBuffer will have a budget pre-allocated x2 the width and x8 the height
                  of the initial content dimensions. This is a heuristic based on typical UI text update patterns,
                  and is intended to reduce VKImage reallocations and staging buffer uploads on subsequent updates
                  when the content grows moderately. Set to false to allocate a GPU texture with no additional budget.
 * @param render_bounds_h Hard vertical render bound in pixels. impress() returns Overflow when exceeded.
 * @param render_bounds_w Hard horizontal render bound in pixels. impress() wraps to next line when exceeded.
 * @return        Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    bool growing = true,
    uint32_t render_bounds_h = 720, uint32_t render_bounds_w = 1280);

/**
 * @brief Composite a UTF-8 string into a new TextBuffer with explicit atlas and bounds.
 *
 * @param text          UTF-8 string to composite.
 * @param atlas         GlyphAtlas to source glyph bitmaps from.
 * @param budget_width  Pre-allocated texture width.
 * @param budget_height Pre-allocated texture height.
 * @param render_bounds_h Hard vertical render bound in pixels.
 * @param render_bounds_w Hard horizontal render bound in pixels.
 * @param color         RGBA glyph color.
 * @return              Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    uint32_t budget_width,
    uint32_t budget_height,
    uint32_t render_bounds_h, uint32_t render_bounds_w,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

/**
 * @brief Re-composite a UTF-8 string into an existing TextBuffer.
 *
 * Uses the default GlyphAtlas. Always clears the buffer before compositing.
 * When the target has a pre-allocated budget the compositing bounds are the
 * budget dimensions, not the content dimensions, so no VKImage rebuild occurs
 * as long as content fits within the budget.
 *
 * @param target  Existing TextBuffer to write into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param policy  Controls reallocation behaviour on overflow.
 * @return        True on success.
 */
MAYAFLUX_API bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    RedrawPolicy policy = RedrawPolicy::Clip);

/**
 * @brief Re-composite a UTF-8 string into an existing TextBuffer using an explicit atlas.
 *
 * @param target  Existing TextBuffer to write into.
 * @param atlas   GlyphAtlas to source glyph bitmaps from.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param policy  Controls reallocation behaviour on overflow.
 * @return        True on success.
 */
MAYAFLUX_API bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    GlyphAtlas& atlas,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    RedrawPolicy policy = RedrawPolicy::Clip);

/**
 * @brief Append a UTF-8 string into an existing TextBuffer at the current cursor position.
 *
 * Uses the default GlyphAtlas. Composites the new run into the pre-allocated
 * budget region without clearing existing content. The cursor advances past
 * the new run. No VKImage reallocation occurs as long as the run fits within
 * the budget.
 *
 * If the run would exceed the budget, repress(Fit) is called internally,
 * the buffer is reallocated to fit the new content only, the cursor resets,
 * and ImpressResult::Overflow is returned. The caller is responsible for
 * reassembling prior content after an overflow.
 *
 * Requires the target to have been created via press() with growing=true or
 * explicit budget dimensions. Calling impress() on a no-budget buffer will
 * always trigger Overflow on the first call that exceeds content size.
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

/**
 * @brief Append a UTF-8 string into an existing TextBuffer using an explicit atlas.
 *
 * @param target  Existing TextBuffer to append into.
 * @param atlas   GlyphAtlas to source glyph bitmaps from.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @return        ImpressResult::Ok or ImpressResult::Overflow.
 */
MAYAFLUX_API ImpressResult impress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    GlyphAtlas& atlas,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

} // namespace MayaFlux::Portal::Text
