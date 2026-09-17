#include "TextEdit.hpp"

namespace MayaFlux::Portal::Text {

void insert_codepoint(EditableText& s, uint32_t codepoint)
{
    if (is_control(codepoint))
        return;
    const std::string encoded = encode_utf8(codepoint);
    s.text.insert(s.cursor, encoded);
    s.cursor += encoded.size();
}

void insert_literal(EditableText& s, char c)
{
    s.text.insert(s.cursor, 1, c);
    s.cursor += 1;
}

void move(EditableText& s, int n)
{
    if (n > 0) {
        for (int i = 0; i < n && s.cursor < s.text.size(); ++i)
            s.cursor = next_codepoint_offset(s.text, s.cursor);
    } else {
        for (int i = 0; i > n && s.cursor > 0; --i)
            s.cursor = previous_codepoint_offset(s.text, s.cursor);
    }
}

void erase(EditableText& s, int n)
{
    if (n > 0) {
        for (int i = 0; i < n && s.cursor < s.text.size(); ++i) {
            const size_t next = next_codepoint_offset(s.text, s.cursor);
            s.text.erase(s.cursor, next - s.cursor);
        }
    } else {
        for (int i = 0; i > n && s.cursor > 0; --i) {
            const size_t prev = previous_codepoint_offset(s.text, s.cursor);
            s.text.erase(prev, s.cursor - prev);
            s.cursor = prev;
        }
    }
}

} // namespace MayaFlux::Portal::Text
