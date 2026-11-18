#pragma once

#include "TypeSetter.hpp"

namespace MayaFlux::Portal::Text {

/**
 * @file TextEdit.hpp
 * @brief Text editing operations: a mutable buffer plus a byte-offset
 *        cursor, and the operations that keep it correct.
 *
 * A mutable counterpart to copy()/x_at()/index_at() in TypeSetter.hpp:
 * those query a fixed string at a byte position or range, these mutate one
 * in place at a single byte-offset cursor. Built entirely from
 * is_control(), encode_utf8(), and next_codepoint_offset()/
 * previous_codepoint_offset() - no atlas, no layout, no rendering
 * dependency of any kind. Scoped to text editing generally, not tied to
 * any one caller - room for word-boundary movement, Home/End, and similar
 * to land here later.
 */

/**
 * @struct EditableText
 * @brief Mutable UTF-8 text plus a byte-offset cursor.
 *
 * cursor is always a codepoint boundary into text: every function below
 * that moves or writes it goes through next_codepoint_offset(),
 * previous_codepoint_offset(), or encode_utf8(), so nothing here ever sets
 * it to an arbitrary byte offset.
 */
struct EditableText {
    std::string text;
    size_t cursor { 0 };

    /**
     * @brief The fixed end of a selection; the cursor is the moving end.
     *
     * Unset means nothing is selected. Equal to the cursor also means nothing
     * is selected. Like cursor it is always a codepoint boundary.
     */
    std::optional<size_t> anchor;
};

/**
 * @brief The selected byte range as (begin, end), begin first whichever end the
 *        cursor is, or nullopt when nothing is selected.
 *
 * Offsets past the end of the text are clamped to it.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::pair<size_t, size_t>> selection(const EditableText& s);

/// @brief The selected text, empty when nothing is selected.
[[nodiscard]] MAYAFLUX_API std::string selected_text(const EditableText& s);

/// @brief Drop the selection and leave the cursor where it is.
MAYAFLUX_API void clear_selection(EditableText& s);

/// @brief Select the whole text, with the cursor at its end.
MAYAFLUX_API void select_all(EditableText& s);

/**
 * @brief Erase the selected text and put the cursor where it was.
 * @return True if there was a selection to erase.
 */
MAYAFLUX_API bool erase_selection(EditableText& s);

/**
 * @brief Put the cursor at a byte offset, optionally extending the selection to it.
 *
 * @param offset Codepoint boundary; clamped to the text length.
 * @param extend When true the selection grows from where the cursor was (or from
 *               its existing anchor) to @p offset. When false any selection is dropped.
 */
MAYAFLUX_API void move_to(EditableText& s, size_t offset, bool extend = false);

/**
 * @brief Insert UTF-8 text at the cursor, replacing the selection.
 *
 * Line endings are normalised: CRLF and a lone CR become LF. Tab and LF are
 * kept; every other ASCII control byte is dropped. Bytes of multi-byte
 * sequences pass through unchanged.
 */
MAYAFLUX_API void insert_text(EditableText& s, std::string_view text);

/**
 * @brief Insert one codepoint at the cursor, advancing it past the insert.
 *
 * No-op if is_control(codepoint) is true. Replaces the selection, if any.
 */
MAYAFLUX_API void insert_codepoint(EditableText& s, uint32_t codepoint);

/**
 * @brief Insert one raw ASCII byte at the cursor, advancing it past the insert.
 *
 * For control bytes ('\\t', '\\n') that structurally never reach
 * WindowEventType::TEXT_INPUT and so cannot go through insert_codepoint().
 * Replaces the selection, if any.
 */
MAYAFLUX_API void insert_literal(EditableText& s, char c);

/**
 * @brief Move the cursor by @p n codepoints.
 *
 * Positive @p n moves toward the end of the text, negative toward the
 * start. Clamps at whichever text boundary is reached first rather than
 * wrapping or erroring on an out-of-range count.
 *
 * With a selection, a plain move collapses it: the cursor goes to the start of
 * the selection for a negative @p n and to its end for a positive one, and
 * moves no further.
 */
MAYAFLUX_API void move(EditableText& s, int n);

/**
 * @brief Move the cursor by @p n codepoints, extending the selection.
 *
 * The anchor is set to the cursor's old position if nothing was selected, then
 * the cursor moves as in move(), without collapsing. A selection that shrinks
 * back to zero width is dropped.
 */
MAYAFLUX_API void extend(EditableText& s, int n);

/**
 * @brief Erase @p n codepoints from the cursor.
 *
 * Positive @p n erases forward: the codepoints at and after the cursor,
 * which does not move. Negative @p n erases backward: the codepoints
 * before the cursor, which moves back by the same count. Clamps at
 * whichever text boundary is reached first.
 *
 * With a selection, erases the selection instead, whatever @p n is.
 */
MAYAFLUX_API void erase(EditableText& s, int n);

} // namespace MayaFlux::Portal::Text
