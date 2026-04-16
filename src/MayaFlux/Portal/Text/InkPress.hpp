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
 * @brief Composite a UTF-8 string into a new TextBuffer.
 *
 * Uses the default GlyphAtlas. Fails if set_default_font() has not been called.
 *
 * @param text   UTF-8 string to composite.
 * @param color  RGBA glyph color.
 * @return       Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

/**
 * @brief Composite a UTF-8 string into a new TextBuffer using an explicit atlas.
 *
 * @param text   UTF-8 string to composite.
 * @param atlas  GlyphAtlas to source glyph bitmaps from.
 * @param color  RGBA glyph color.
 * @return       Initialized TextBuffer, or nullptr on failure.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F });

/**
 * @brief Re-composite a UTF-8 string into an existing TextBuffer.
 *
 * Uses the default GlyphAtlas. Always clears the buffer before compositing.
 * Behaviour on size mismatch is controlled by @p policy.
 *
 * @param target  Existing TextBuffer to write into.
 * @param text    UTF-8 string to composite.
 * @param color   RGBA glyph color.
 * @param policy  Controls clearing and reallocation behaviour.
 * @return        True on success. False if Strict and text exceeds bounds.
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
 * @param policy  Controls clearing and reallocation behaviour.
 * @return        True on success. False if Strict and text exceeds bounds.
 */
MAYAFLUX_API bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    GlyphAtlas& atlas,
    std::string_view text,
    glm::vec4 color = { 1.F, 1.F, 1.F, 1.F },
    RedrawPolicy policy = RedrawPolicy::Clip);

} // namespace MayaFlux::Portal::Text
