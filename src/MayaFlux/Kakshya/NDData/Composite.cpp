#include "Composite.hpp"
#include "CompositeInsertion.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    struct TextSlice {
        uint64_t offset {};
        uint64_t length {};
    };

    static_assert(sizeof(TextSlice) == 2 * sizeof(uint64_t));

    struct DynamicCell {
        uint64_t kind {};
        uint64_t payload {};
        uint64_t length {};
    };

    static_assert(sizeof(DynamicCell) == 3 * sizeof(uint64_t));

    constexpr uint64_t null_kind = 0;
    constexpr uint64_t integer_kind = 1;
    constexpr uint64_t real_kind = 2;
    constexpr uint64_t text_kind = 3;
    constexpr uint64_t blob_kind = 4;

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

bool CompositeLayout::add_nested_field(
    std::string name, CompositeLayout layout, std::type_index type)
{
    if (layout.fields().empty())
        return false;

    layout.finalize();
    auto nested_layout = std::make_shared<const CompositeLayout>(std::move(layout));

    if (!add_field_impl(std::move(name), type, sizeof(uint64_t)))
        return false;

    m_fields.back().nested_layout = std::move(nested_layout);
    return true;
}

bool CompositeLayout::same_structure(const CompositeLayout& other) const noexcept
{
    if (m_fields.size() != other.m_fields.size())
        return false;

    for (size_t i = 0; i < m_fields.size(); ++i) {
        const auto& left = m_fields[i];
        const auto& right = other.m_fields[i];

        if (left.name != right.name || left.type != right.type
            || left.size_bytes != right.size_bytes
            || static_cast<bool>(left.nested_layout) != static_cast<bool>(right.nested_layout))
            return false;

        if (left.nested_layout && !left.nested_layout->same_structure(*right.nested_layout))
            return false;
    }
    return true;
}

bool CompositeLayout::add_compound_field(std::string name, CompositeLayout layout)
{
    return add_nested_field(std::move(name), std::move(layout), typeid(detail::CompositeCompound));
}

bool CompositeLayout::add_list_field(std::string name, CompositeLayout item_layout)
{
    return add_nested_field(std::move(name), std::move(item_layout), typeid(detail::CompositeList));
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

CompositeArray::CompositeArray(const CompositeArray& other)
    : m_layout(other.m_layout)
    , m_rows(other.m_rows)
    , m_text(other.m_text)
    , m_blob(other.m_blob)
{
    m_nested.reserve(other.m_nested.size());
    for (const auto& value : other.m_nested) {
        m_nested.push_back(value ? std::make_shared<CompositeArray>(*value) : nullptr);
    }
}

CompositeArray& CompositeArray::operator=(const CompositeArray& other)
{
    if (this != &other) {
        CompositeArray copy(other);
        *this = std::move(copy);
    }
    return *this;
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

    return Composite(m_layout, m_rows, m_text, m_blob, &m_nested, index);
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

std::optional<size_t> CompositeArray::validate_dynamic_write(
    size_t index, std::string_view field_name) const
{
    return validate_write(index, field_name, typeid(detail::CompositeDynamic));
}

bool CompositeArray::set_text(size_t index, std::string_view field_name, std::string_view value)
{
    const auto declared = m_layout.find_field(field_name);
    const std::type_index expected = declared
            && m_layout.fields()[*declared].type == typeid(detail::CompositeDynamic)
        ? typeid(detail::CompositeDynamic) : typeid(std::string);
    const auto field_index = validate_write(index, field_name, expected);
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
    auto* destination = rows.data() + index * m_layout.stride_bytes() + field.offset_bytes;
    if (field.type == typeid(detail::CompositeDynamic)) {
        const DynamicCell cell { text_kind, slice.offset, slice.length };
        std::memcpy(destination, &cell, sizeof(cell));
    } else {
        std::memcpy(destination, &slice, sizeof(slice));
    }

    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_null(size_t index, std::string_view field_name)
{
    const auto field_index = validate_dynamic_write(index, field_name);
    if (!field_index)
        return false;
    const auto& field = m_layout.fields()[*field_index];
    const DynamicCell cell { null_kind, 0, 0 };
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes, &cell, sizeof(cell));
    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_integer(size_t index, std::string_view field_name, int64_t value)
{
    const auto field_index = validate_dynamic_write(index, field_name);
    if (!field_index)
        return false;
    DynamicCell cell { integer_kind, 0, 0 };
    std::memcpy(&cell.payload, &value, sizeof(value));
    const auto& field = m_layout.fields()[*field_index];
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes, &cell, sizeof(cell));
    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_real(size_t index, std::string_view field_name, double value)
{
    const auto field_index = validate_dynamic_write(index, field_name);
    if (!field_index)
        return false;
    DynamicCell cell { real_kind, 0, 0 };
    std::memcpy(&cell.payload, &value, sizeof(value));
    const auto& field = m_layout.fields()[*field_index];
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes, &cell, sizeof(cell));
    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_blob(size_t index, std::string_view field_name, std::span<const uint8_t> value)
{
    const auto declared = m_layout.find_field(field_name);
    const std::type_index expected = declared
            && m_layout.fields()[*declared].type == typeid(detail::CompositeDynamic)
        ? typeid(detail::CompositeDynamic) : typeid(std::vector<uint8_t>);
    const auto field_index = validate_write(index, field_name, expected);
    if (!field_index)
        return false;

    auto& blob = std::get<std::vector<uint8_t>>(m_blob);
    if (value.size() > blob.max_size() - blob.size())
        return false;
    const std::vector<uint8_t> value_copy(value.begin(), value.end());
    const TextSlice slice { static_cast<uint64_t>(blob.size()), static_cast<uint64_t>(value_copy.size()) };
    blob.insert(blob.end(), value_copy.begin(), value_copy.end());

    const auto& field = m_layout.fields()[*field_index];
    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    auto* destination = rows.data() + index * m_layout.stride_bytes() + field.offset_bytes;
    if (field.type == typeid(detail::CompositeDynamic)) {
        const DynamicCell cell { blob_kind, slice.offset, slice.length };
        std::memcpy(destination, &cell, sizeof(cell));
    } else {
        std::memcpy(destination, &slice, sizeof(slice));
    }
    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_nested(size_t index, std::string_view field_name,
    const CompositeArray& value, std::type_index type)
{
    const auto field_index = validate_write(index, field_name, type);
    if (!field_index)
        return false;
    const auto& field = m_layout.fields()[*field_index];
    if (!field.nested_layout || !field.nested_layout->same_structure(value.layout())
        || (type == typeid(detail::CompositeCompound) && value.size() != 1))
        return false;

    if (m_nested.size() == m_nested.max_size())
        return false;

    auto owned = std::make_shared<CompositeArray>(*field.nested_layout);
    CompositeInsertion insertion(*owned);

    for (size_t i = 0; i < value.size(); ++i) {
        const auto row = value.at(i);
        if (!row || !insertion.append(*row))
            return false;
    }

    const uint64_t payload_index = static_cast<uint64_t>(m_nested.size());
    m_nested.push_back(std::move(owned));

    auto& rows = std::get<std::vector<uint8_t>>(m_rows);
    std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes,
        &payload_index, sizeof(payload_index));

    set_present(index, *field_index, true);
    return true;
}

bool CompositeArray::set_compound(size_t index, std::string_view field_name, const Composite& value)
{
    CompositeArray one(value.layout());
    CompositeInsertion insertion(one);

    return insertion.append(value)
        && set_nested(index, field_name, one, typeid(detail::CompositeCompound));
}

bool CompositeArray::set_list(size_t index, std::string_view field_name, const CompositeArray& values)
{
    return set_nested(index, field_name, values, typeid(detail::CompositeList));
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

const CompositeArray* Composite::list(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name) || !m_nested)
        return nullptr;

    const auto& field = m_layout->fields()[*field_index];
    if (field.type != typeid(detail::CompositeList))
        return nullptr;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    uint64_t payload_index {};
    std::memcpy(&payload_index,
        rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes,
        sizeof(payload_index));

    if (payload_index >= m_nested->size() || !(*m_nested)[payload_index])
        return nullptr;

    return (*m_nested)[payload_index].get();
}

const CompositeArray* Composite::compound(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name) || !m_nested)
        return nullptr;

    const auto& field = m_layout->fields()[*field_index];
    if (field.type != typeid(detail::CompositeCompound))
        return nullptr;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    uint64_t payload_index {};
    std::memcpy(&payload_index,
        rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes,
        sizeof(payload_index));

    if (payload_index >= m_nested->size() || !(*m_nested)[payload_index])
        return nullptr;

    return (*m_nested)[payload_index]->size() == 1
        ? (*m_nested)[payload_index].get() : nullptr;
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
    if (field.type != typeid(std::string) && field.type != typeid(detail::CompositeDynamic))
        return std::nullopt;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    const auto* source = rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes;
    TextSlice slice;
    if (field.type == typeid(detail::CompositeDynamic)) {
        DynamicCell cell;
        std::memcpy(&cell, source, sizeof(cell));
        if (cell.kind != text_kind)
            return std::nullopt;
        slice = { cell.payload, cell.length };
    } else {
        std::memcpy(&slice, source, sizeof(slice));
    }

    const auto& text = std::get<std::vector<uint8_t>>(*m_text);
    if (slice.offset > text.size() || slice.length > text.size() - slice.offset)
        return std::nullopt;

    if (slice.length == 0)
        return std::string_view {};

    return std::string_view(
        reinterpret_cast<const char*>(text.data() + slice.offset),
        static_cast<size_t>(slice.length));
}

std::optional<std::span<const uint8_t>> Composite::blob(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name))
        return std::nullopt;
    const auto& field = m_layout->fields()[*field_index];
    if (field.type != typeid(std::vector<uint8_t>) && field.type != typeid(detail::CompositeDynamic))
        return std::nullopt;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    const auto* source = rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes;
    TextSlice slice;
    if (field.type == typeid(detail::CompositeDynamic)) {
        DynamicCell cell;
        std::memcpy(&cell, source, sizeof(cell));
        if (cell.kind != blob_kind)
            return std::nullopt;
        slice = { cell.payload, cell.length };
    } else {
        std::memcpy(&slice, source, sizeof(slice));
    }
    const auto& blob = std::get<std::vector<uint8_t>>(*m_blob);
    if (slice.offset > blob.size() || slice.length > blob.size() - slice.offset)
        return std::nullopt;
    if (slice.length == 0)
        return std::span<const uint8_t> {};
    return std::span<const uint8_t>(blob.data() + slice.offset, static_cast<size_t>(slice.length));
}

std::optional<CompositeValue> Composite::value(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name)
        || m_layout->fields()[*field_index].type != typeid(detail::CompositeDynamic))
        return std::nullopt;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    DynamicCell cell;
    std::memcpy(&cell, rows.data() + m_index * m_layout->stride_bytes()
            + m_layout->fields()[*field_index].offset_bytes, sizeof(cell));
    if (cell.kind == null_kind)
        return CompositeValue { std::monostate {} };
    if (cell.kind == integer_kind) {
        int64_t result;
        std::memcpy(&result, &cell.payload, sizeof(result));
        return CompositeValue { result };
    }
    if (cell.kind == real_kind) {
        double result;
        std::memcpy(&result, &cell.payload, sizeof(result));
        return CompositeValue { result };
    }
    if (cell.kind == text_kind) {
        if (auto result = text(field_name))
            return CompositeValue { *result };
    }
    if (cell.kind == blob_kind) {
        if (auto result = blob(field_name))
            return CompositeValue { *result };
    }
    return std::nullopt;
}

}
