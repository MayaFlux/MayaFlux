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
 * any one caller - room for word-boundary movement, selection ranges,
 * Home/End, and similar to land here later.
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
};

/**
 * @brief Insert one codepoint at the cursor, advancing it past the insert.
 *
 * No-op if is_control(codepoint) is true.
 */
MAYAFLUX_API void insert_codepoint(EditableText& s, uint32_t codepoint);

/**
 * @brief Insert one raw ASCII byte at the cursor, advancing it past the insert.
 *
 * For control bytes ('\\t', '\\n') that structurally never reach
 * WindowEventType::TEXT_INPUT and so cannot go through insert_codepoint().
 */
MAYAFLUX_API void insert_literal(EditableText& s, char c);

/**
 * @brief Move the cursor by @p n codepoints.
 *
 * Positive @p n moves toward the end of the text, negative toward the
 * start. Clamps at whichever text boundary is reached first rather than
 * wrapping or erroring on an out-of-range count.
 */
MAYAFLUX_API void move(EditableText& s, int n);

/**
 * @brief Erase @p n codepoints from the cursor.
 *
 * Positive @p n erases forward: the codepoints at and after the cursor,
 * which does not move. Negative @p n erases backward: the codepoints
 * before the cursor, which moves back by the same count. Clamps at
 * whichever text boundary is reached first.
 */
MAYAFLUX_API void erase(EditableText& s, int n);

} // namespace MayaFlux::Portal::Text
