#include "Composite.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    struct TextSlice {
        uint64_t offset {};
        uint64_t length {};
    };

    static_assert(sizeof(TextSlice) == 2 * sizeof(uint64_t));

}

bool CompositeLayout::add_field_impl(std::string name, std::type_index type, size_t size_bytes)
{
    if (m_finalized) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeLayout: cannot add field '{}' after finalization", name);
        return false;
    }
    if (name.empty()) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeLayout: field name cannot be empty");
        return false;
    }
    if (find_field(name)) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeLayout: duplicate field '{}'", name);
        return false;
    }

    m_fields.push_back({ .name = std::move(name), .type = type, .offset_bytes = 0, .size_bytes = size_bytes });
    return true;
}

std::optional<size_t> CompositeLayout::find_field(std::string_view name) const noexcept
{
    for (size_t i = 0; i < m_fields.size(); ++i) {
        if (m_fields[i].name == name)
            return i;
    }
    return std::nullopt;
}

void CompositeLayout::finalize()
{
    if (m_finalized)
        return;

    if (m_fields.empty()) {
        Journal::error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "CompositeLayout requires at least one field");
    }

    m_presence_bytes = m_fields.size() / 8 + (m_fields.size() % 8 != 0);
    size_t offset = m_presence_bytes;

    for (auto& field : m_fields) {
        if (field.size_bytes > std::numeric_limits<size_t>::max() - offset) {
            Journal::error<std::length_error>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "CompositeLayout exceeds addressable size at field '{}'", field.name);
        }
        field.offset_bytes = offset;
        offset += field.size_bytes;
    }

    m_stride_bytes = offset;
    m_finalized = true;
}

CompositeArray::CompositeArray(CompositeLayout layout)
    : m_layout(std::move(layout))
{
    m_layout.finalize();
}

size_t CompositeArray::size() const noexcept
{
    const auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    return rows.size() / m_layout.stride_bytes();
}

std::optional<Composite> CompositeArray::at(size_t index) const noexcept
{
    if (index >= size())
        return std::nullopt;

    return Composite(m_layout, m_rows, m_text, index);
}

size_t CompositeArray::append()
{
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    const size_t index = size();

    if (m_layout.stride_bytes() > rows.max_size() - rows.size()) {
        Journal::error<std::length_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "CompositeArray exceeds addressable size at element {}", index);
    }

    rows.resize(rows.size() + m_layout.stride_bytes(), 0);
    return index;
}

bool CompositeArray::is_present(size_t index, size_t field_index) const noexcept
{
    const auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    const size_t byte_index = index * m_layout.stride_bytes() + field_index / 8;

    return (rows[byte_index] & static_cast<uint8_t>(1U << (field_index % 8))) != 0;
}

void CompositeArray::set_present(size_t index, size_t field_index, bool present) noexcept
{
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    const size_t byte_index = index * m_layout.stride_bytes() + field_index / 8;
    const auto mask = static_cast<uint8_t>(1U << (field_index % 8));

    if (present) {
        rows[byte_index] |= mask;
    } else {
        rows[byte_index] &= static_cast<uint8_t>(~mask);
    }
}

std::optional<size_t> CompositeArray::validate_write(
    size_t index, std::string_view field_name, std::type_index type) const
{
    if (index >= size()) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: element {} is out of range for {} elements", index, size());
        return std::nullopt;
    }

    const auto field_index = m_layout.find_field(field_name);
    if (!field_index) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: field '{}' does not exist", field_name);
        return std::nullopt;
    }

    const auto& field = m_layout.fields()[*field_index];
    if (field.type != type) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: field '{}' expects type '{}', received '{}'",
            field_name, field.type.name(), type.name());
        return std::nullopt;
    }

    return field_index;
}

bool CompositeArray::set_text(size_t index, std::string_view field_name, std::string_view value)
{
    const auto field_index = validate_write(index, field_name, typeid(std::string));
    if (!field_index)
        return false;

    const auto& field = m_layout.fields()[*field_index];
    auto& text = std::get<std::vector<uint8_t>>(m_text);
    if (value.size() > text.max_size() - text.size()) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: text value for field '{}' exceeds addressable size", field_name);
        return false;
    }

    const std::string value_copy(value);
    const TextSlice slice {
        .offset = static_cast<uint64_t>(text.size()),
        .length = static_cast<uint64_t>(value_copy.size())
    };
    text.insert(text.end(), value_copy.begin(), value_copy.end());

    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes,
        &slice, sizeof(slice));

    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::clear(size_t index, std::string_view field_name)
{
    if (index >= size()) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: element {} is out of range for {} elements", index, size());
        return false;
    }

    const auto field_index = m_layout.find_field(field_name);

    if (!field_index) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeArray: field '{}' does not exist", field_name);
        return false;
    }

    set_present(index, *field_index, false);
    return true;
}

bool Composite::has(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index)
        return false;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    const size_t byte_index = m_index * m_layout->stride_bytes() + *field_index / 8;

    return (rows[byte_index] & static_cast<uint8_t>(1U << (*field_index % 8))) != 0;
}

const CompositeLayout& Composite::layout() const noexcept
{
    return *m_layout;
}

std::optional<std::string_view> Composite::text(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name))
        return std::nullopt;

    const auto& field = m_layout->fields()[*field_index];
    if (field.type != typeid(std::string))
        return std::nullopt;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    TextSlice slice;
    std::memcpy(&slice,
        rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes,
        sizeof(slice));

    const auto& text = std::get<std::vector<uint8_t>>(*m_text);
    if (slice.offset > text.size() || slice.length > text.size() - slice.offset)
        return std::nullopt;

    if (slice.length == 0)
        return std::string_view {};

    return std::string_view(
        reinterpret_cast<const char*>(text.data() + slice.offset),
        static_cast<size_t>(slice.length));
}

}
