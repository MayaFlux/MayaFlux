#include "CompositeAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

std::optional<CompositeAccess> CompositeArray::access() const
{
    return CompositeAccess(m_rows, m_text, m_blob, m_nested, m_layout);
}

std::optional<CompositeSlice> CompositeArray::slice(size_t start, size_t count) const
{
    const auto view = access();
    return view ? view->slice(start, count) : std::nullopt;
}

std::optional<CompositeSlice> CompositeArray::slice(const Region& region) const
{
    const auto view = access();
    return view ? view->slice(region) : std::nullopt;
}

std::optional<CompositeAccess> as_composite_access(
    const DataVariant& elements, const DataVariant& text, const DataVariant& blob,
    const CompositeLayout& layout)
{
    for (const auto& field : layout.fields()) {
        if (field.nested_layout)
            return std::nullopt;
    }

    static const std::vector<std::shared_ptr<CompositeArray>> empty;
    return as_composite_access(elements, text, blob, empty, layout);
}

std::optional<CompositeAccess> as_composite_access(
    const DataVariant& elements, const DataVariant& text, const DataVariant& blob,
    const std::vector<std::shared_ptr<CompositeArray>>& nested,
    const CompositeLayout& layout)
{
    const auto* element_bytes = std::get_if<std::vector<uint8_t>>(&elements);
    const auto* text_bytes = std::get_if<std::vector<uint8_t>>(&text);
    const auto* blob_bytes = std::get_if<std::vector<uint8_t>>(&blob);
    const size_t stride = layout.stride_bytes();
    if (!element_bytes || !text_bytes || !blob_bytes || stride == 0
        || element_bytes->size() % stride != 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_composite_access: invalid storage or element stride");
        return std::nullopt;
    }

    for (const auto& field : layout.fields()) {
        if (field.offset_bytes > stride || field.size_bytes > stride - field.offset_bytes
            || ((field.type == typeid(std::string)
                    || field.type == typeid(std::vector<uint8_t>))
                && field.size_bytes != 2 * sizeof(uint64_t))
            || (field.type == typeid(detail::CompositeDynamic)
                && field.size_bytes != 3 * sizeof(uint64_t))
            || ((field.type == typeid(detail::CompositeCompound)
                    || field.type == typeid(detail::CompositeList))
                && (field.size_bytes != sizeof(uint64_t) || !field.nested_layout))) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_composite_access: invalid layout for field '{}'", field.name);
            return std::nullopt;
        }
    }

    const size_t count = element_bytes->size() / stride;
    for (size_t row = 0; row < count; ++row) {
        for (size_t field_index = 0; field_index < layout.fields().size(); ++field_index) {
            const auto& field = layout.fields()[field_index];
            if (!field.nested_layout)
                continue;

            const auto presence = (*element_bytes)[row * stride + field_index / 8];
            if ((presence & static_cast<uint8_t>(1U << (field_index % 8))) == 0)
                continue;

            uint64_t payload_index {};
            std::memcpy(&payload_index,
                element_bytes->data() + row * stride + field.offset_bytes,
                sizeof(payload_index));
            if (payload_index >= nested.size() || !nested[payload_index]
                || !nested[payload_index]->layout().same_structure(*field.nested_layout)
                || (field.type == typeid(detail::CompositeCompound)
                    && nested[payload_index]->size() != 1))
                return std::nullopt;
        }
    }

    return CompositeAccess(elements, text, blob, nested, layout);
}

std::optional<Composite> CompositeAccess::at(size_t index) const noexcept
{
    if (index >= size())
        return std::nullopt;
    return Composite(*m_layout, *m_elements, *m_text, *m_blob, m_nested, index);
}

std::span<const uint8_t> CompositeAccess::element_bytes() const noexcept
{
    const auto& elements = std::get<std::vector<uint8_t>>(*m_elements);
    return { elements.data(), elements.size() };
}

std::vector<DataDimension> CompositeAccess::byte_dimensions() const
{
    return {
        DataDimension("elements", size(), m_layout->stride_bytes(), DataDimension::Role::CUSTOM),
        DataDimension("bytes", m_layout->stride_bytes(), 1, DataDimension::Role::CUSTOM)
    };
}

std::optional<CompositeSlice> CompositeAccess::slice(size_t start, size_t count) const noexcept
{
    if (start > size() || count > size() - start)
        return std::nullopt;
    return CompositeSlice(*this, start, count);
}

std::optional<CompositeSlice> CompositeAccess::slice(const Region& region) const
{
    if (region.start_coordinates.size() != 1 || region.end_coordinates.size() != 1)
        return std::nullopt;

    const auto start = region.start_coordinates[0];
    const auto end = region.end_coordinates[0];

    if (start > end || end >= size())
        return std::nullopt;

    return CompositeSlice(*this, static_cast<size_t>(start),
        static_cast<size_t>(end - start + 1), region);
}

std::optional<Composite> CompositeSlice::at(size_t index) const noexcept
{
    if (index >= m_count)
        return std::nullopt;
    return m_access.at(m_start + index);
}

std::span<const uint8_t> CompositeSlice::element_bytes() const noexcept
{
    const auto bytes = m_access.element_bytes();
    return bytes.subspan(m_start * layout().stride_bytes(), m_count * layout().stride_bytes());
}

}
