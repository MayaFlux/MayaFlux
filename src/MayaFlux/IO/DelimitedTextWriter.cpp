#include "DelimitedTextWriter.hpp"

#include "FileWriter.hpp"

#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <limits>

namespace MayaFlux::IO {

namespace {

std::string extension_of(const std::string& filepath)
{
    auto extension = std::filesystem::path(filepath).extension().string();

    std::ranges::transform(extension, extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return extension;
}

bool same_layout(
    const Kakshya::CompositeLayout& left,
    const Kakshya::CompositeLayout& right)
{
    if (left.stride_bytes() != right.stride_bytes()
        || left.presence_bytes() != right.presence_bytes()
        || left.fields().size() != right.fields().size())
        return false;

    for (size_t i = 0; i < left.fields().size(); ++i) {
        const auto& a = left.fields()[i];
        const auto& b = right.fields()[i];

        if (a.name != b.name || a.type != b.type
            || a.offset_bytes != b.offset_bytes
            || a.size_bytes != b.size_bytes)
            return false;
    }

    return true;
}

template <typename T>
std::optional<std::string> format_number(
    const Kakshya::Composite& element,
    std::string_view name)
{
    const auto value = element.get<T>(name);
    if (!value)
        return std::nullopt;

    if constexpr (std::same_as<T, bool>) {
        return *value ? "true" : "false";
    } else {
        std::array<char, 128> buffer {};

        if constexpr (std::integral<T>) {
            using Formatted = std::conditional_t<
                std::is_signed_v<T>, long long, unsigned long long>;

            const auto [end, error] = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(),
                static_cast<Formatted>(*value));

            if (error != std::errc {})
                return std::nullopt;

            return std::string(buffer.data(), end);
        } else {
            if (!std::isfinite(*value))
                return std::nullopt;

            const auto [end, error] = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), *value);

            if (error != std::errc {})
                return std::nullopt;

            return std::string(buffer.data(), end);
        }
    }
}

std::optional<std::string> format_field(
    const Kakshya::Composite& element,
    const Kakshya::CompositeField& field)
{
    const auto name = std::string_view(field.name);
    const auto type = field.type;

    if (type == typeid(bool))
        return format_number<bool>(element, name);

    if (type == typeid(char))
        return format_number<char>(element, name);
    if (type == typeid(signed char))
        return format_number<signed char>(element, name);
    if (type == typeid(unsigned char))
        return format_number<unsigned char>(element, name);

    if (type == typeid(short))
        return format_number<short>(element, name);
    if (type == typeid(unsigned short))
        return format_number<unsigned short>(element, name);

    if (type == typeid(int))
        return format_number<int>(element, name);
    if (type == typeid(unsigned int))
        return format_number<unsigned int>(element, name);

    if (type == typeid(long))
        return format_number<long>(element, name);
    if (type == typeid(unsigned long))
        return format_number<unsigned long>(element, name);

    if (type == typeid(long long))
        return format_number<long long>(element, name);
    if (type == typeid(unsigned long long))
        return format_number<unsigned long long>(element, name);

    if (type == typeid(float))
        return format_number<float>(element, name);
    if (type == typeid(double))
        return format_number<double>(element, name);
    if (type == typeid(long double))
        return format_number<long double>(element, name);

    return std::nullopt;
}

bool supported_type(std::type_index type)
{
    return type == typeid(std::string)
        || type == typeid(bool)
        || type == typeid(char)
        || type == typeid(signed char)
        || type == typeid(unsigned char)
        || type == typeid(short)
        || type == typeid(unsigned short)
        || type == typeid(int)
        || type == typeid(unsigned int)
        || type == typeid(long)
        || type == typeid(unsigned long)
        || type == typeid(long long)
        || type == typeid(unsigned long long)
        || type == typeid(float)
        || type == typeid(double)
        || type == typeid(long double);
}

}

DelimitedTextWriter::~DelimitedTextWriter()
{
    if (m_file.is_open())
        close();
}

bool DelimitedTextWriter::set_write_header(bool enabled) noexcept
{
    if (is_open())
        return false;

    m_write_header = enabled;
    return true;
}

bool DelimitedTextWriter::set_delimiter(char delimiter) noexcept
{
    if (is_open() || delimiter == '\0' || delimiter == '"'
        || delimiter == '\r' || delimiter == '\n')
        return false;

    m_delimiter = delimiter;
    return true;
}

bool DelimitedTextWriter::can_write(const std::string& filepath) const
{
    const auto extension = extension_of(filepath);

    return extension == ".csv" || extension == ".tsv";
}

bool DelimitedTextWriter::open(
    const std::string& filepath,
    const Kakshya::CompositeLayout& layout)
{
    if (is_open()) {
        m_last_error = "Close the current file before opening another";
        return false;
    }

    m_last_error.clear();

    if (!can_write(filepath)) {
        m_last_error = "Expected a .csv or .tsv output path";
        return false;
    }

    if (layout.fields().empty()) {
        m_last_error = "Composite output requires at least one field";
        return false;
    }

    for (const auto& field : layout.fields()) {
        if (!supported_type(field.type)) {
            m_last_error = "Unsupported Composite field: " + field.name;
            return false;
        }
    }

    Kakshya::CompositeArray empty(layout);
    m_layout = empty.layout();
    m_active_delimiter = m_delimiter != '\0'
        ? m_delimiter
        : (extension_of(filepath) == ".tsv" ? '\t' : ',');

    m_file.open(resolve_write_path(filepath),
        std::ios::binary | std::ios::out | std::ios::trunc);

    if (!m_file) {
        m_last_error = "Cannot open delimited text output";
        m_layout.reset();
        return false;
    }

    m_rows_written = 0;

    if (m_write_header) {
        const auto& fields = m_layout->fields();

        for (size_t i = 0; i < fields.size(); ++i) {
            if (i != 0)
                m_file.put(m_active_delimiter);

            if (!write_cell(fields[i].name, false)) {
                close();
                return false;
            }
        }

        m_file.put('\n');

        if (!m_file) {
            m_last_error = "Failed to write delimited text header";
            close();
            return false;
        }
    }

    return true;
}

bool DelimitedTextWriter::write_rows(
    const Kakshya::CompositeSlice& rows)
{
    if (!is_open() || !m_layout) {
        m_last_error = "No delimited text output is open";
        return false;
    }

    if (!same_layout(*m_layout, rows.layout())) {
        m_last_error = "Composite slice layout differs from the open file";
        return false;
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto element = rows.at(i);

        if (!element || !write_element(*element))
            return false;

        ++m_rows_written;
    }

    return true;
}

bool DelimitedTextWriter::flush()
{
    if (!is_open()) {
        m_last_error = "No delimited text output is open";
        return false;
    }

    m_file.flush();

    if (!m_file) {
        m_last_error = "Failed to flush delimited text output";
        return false;
    }

    return true;
}

bool DelimitedTextWriter::close()
{
    if (!is_open())
        return true;

    m_file.flush();
    m_file.close();

    const bool finished = !m_file.fail();
    if (!finished && m_last_error.empty())
        m_last_error = "Failed to finalize delimited text output";

    m_layout.reset();
    return finished;
}

bool DelimitedTextWriter::write_cell(
    std::string_view text, bool force_quote)
{
    const bool needs_quote = force_quote
        || text.find(m_active_delimiter) != std::string_view::npos
        || text.find('"') != std::string_view::npos
        || text.find('\r') != std::string_view::npos
        || text.find('\n') != std::string_view::npos;

    if (needs_quote)
        m_file.put('"');

    if (needs_quote) {
        for (const char c : text) {
            if (c == '"')
                m_file.put('"');

            m_file.put(c);
        }
    } else {
        if (text.size() > static_cast<size_t>(
                std::numeric_limits<std::streamsize>::max())) {
            m_last_error = "Delimited text cell exceeds stream size";
            return false;
        }

        m_file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    if (needs_quote)
        m_file.put('"');

    if (!m_file) {
        m_last_error = "Failed to write delimited text cell";
        return false;
    }

    return true;
}

bool DelimitedTextWriter::write_element(
    const Kakshya::Composite& element)
{
    const auto& fields = m_layout->fields();

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i != 0)
            m_file.put(m_active_delimiter);

        const auto& field = fields[i];
        if (!element.has(field.name))
            continue;

        if (field.type == typeid(std::string)) {
            const auto text = element.text(field.name);

            if (!text || !write_cell(*text, text->empty())) {
                if (m_last_error.empty())
                    m_last_error = "Invalid text field: " + field.name;
                return false;
            }
        } else {
            const auto text = format_field(element, field);

            if (!text || !write_cell(*text, false)) {
                if (m_last_error.empty())
                    m_last_error = "Invalid numeric field: " + field.name;
                return false;
            }
        }
    }

    m_file.put('\n');

    if (!m_file) {
        m_last_error = "Failed to write delimited text element";
        return false;
    }

    return true;
}

}
