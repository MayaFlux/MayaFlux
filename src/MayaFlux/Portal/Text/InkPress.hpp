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
 * @brief Composite a UTF-8 string into a new TextBuffer using the default GlyphAtlas.
 *
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param growing Whether the content is expected to grow on subsequent updates. If true, the
                  returned TextBuffer will have a budget pre-allocated x2 the width and x8 the height
                  of the initial content dimensions. This is a heuristic based on typical UI text update patterns,
                  and is intended to reduce VKImage reallocations and staging buffer uploads on subsequent updates
                  when the content grows moderately. Set to false to allocate a GPU texture with no additional budget.
 * @return        Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    bool growing = true);

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
 * @return        Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    bool growing = true);

/**
 * @brief Composite a UTF-8 string into a new TextBuffer with explicit atlas and bounds.
 *
 * @param text          UTF-8 string to composite.
 * @param atlas         GlyphAtlas to source glyph bitmaps from.
 * @param budget_width  Pre-allocated texture width.
 * @param budget_height Pre-allocated texture height.
 * @param color         RGBA glyph color.
 * @return              Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    uint32_t budget_width,
    uint32_t budget_height,
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

} // namespace MayaFlux::Portal::Text
