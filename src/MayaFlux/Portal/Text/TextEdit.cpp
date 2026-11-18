#include "TextEdit.hpp"

namespace MayaFlux::Portal::Text {

std::optional<std::pair<size_t, size_t>> selection(const EditableText& s)
{
    if (!s.anchor)
        return std::nullopt;

    const size_t a = std::min(*s.anchor, s.text.size());
    const size_t c = std::min(s.cursor, s.text.size());
    if (a == c)
        return std::nullopt;

    return std::pair { std::min(a, c), std::max(a, c) };
}

std::string selected_text(const EditableText& s)
{
    const auto range = selection(s);
    return range ? s.text.substr(range->first, range->second - range->first) : std::string {};
}

void clear_selection(EditableText& s)
{
    s.anchor.reset();
}

void select_all(EditableText& s)
{
    s.anchor = 0;
    s.cursor = s.text.size();
}

bool erase_selection(EditableText& s)
{
    const auto range = selection(s);
    s.anchor.reset();
    if (!range)
        return false;

    s.text.erase(range->first, range->second - range->first);
    s.cursor = range->first;
    return true;
}

void move_to(EditableText& s, size_t offset, bool extend)
{
    if (extend) {
        if (!s.anchor)
            s.anchor = s.cursor;
    } else {
        s.anchor.reset();
    }
    s.cursor = std::min(offset, s.text.size());
}

void insert_text(EditableText& s, std::string_view text)
{
    std::string clean;
    clean.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '\r') {
            clean.push_back('\n');
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        } else if (c == '\n' || c == '\t' || (c >= 0x20 && c != 0x7F)) {
            clean.push_back(static_cast<char>(c));
        }
    }

    if (clean.empty())
        return;

    erase_selection(s);
    s.text.insert(s.cursor, clean);
    s.cursor += clean.size();
}

void insert_codepoint(EditableText& s, uint32_t codepoint)
{
    if (is_control(codepoint))
        return;
    const std::string encoded = encode_utf8(codepoint);
    erase_selection(s);
    s.text.insert(s.cursor, encoded);
    s.cursor += encoded.size();
}

void insert_literal(EditableText& s, char c)
{
    erase_selection(s);
    s.text.insert(s.cursor, 1, c);
    s.cursor += 1;
}

namespace {

    void step(EditableText& s, int n)
    {
        if (n > 0) {
            for (int i = 0; i < n && s.cursor < s.text.size(); ++i)
                s.cursor = next_codepoint_offset(s.text, s.cursor);
        } else {
            for (int i = 0; i > n && s.cursor > 0; --i)
                s.cursor = previous_codepoint_offset(s.text, s.cursor);
        }
    }

} // namespace

void move(EditableText& s, int n)
{
    if (const auto range = selection(s); range && n != 0) {
        s.cursor = n < 0 ? range->first : range->second;
        s.anchor.reset();
        return;
    }

    s.anchor.reset();
    step(s, n);
}

void extend(EditableText& s, int n)
{
    if (!s.anchor)
        s.anchor = s.cursor;

    step(s, n);

    if (s.anchor == s.cursor)
        s.anchor.reset();
}

void erase(EditableText& s, int n)
{
    if (erase_selection(s))
        return;

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
