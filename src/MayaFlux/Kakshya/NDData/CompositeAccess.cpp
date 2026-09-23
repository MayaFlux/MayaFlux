#include "CompositeAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

std::optional<CompositeAccess> CompositeArray::access() const
{
    return as_composite_access(m_rows, m_text, m_layout);
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
    const DataVariant& elements, const DataVariant& text, const CompositeLayout& layout)
{
    const auto* element_bytes = std::get_if<std::vector<uint8_t>>(&elements);
    const auto* text_bytes = std::get_if<std::vector<uint8_t>>(&text);
    const size_t stride = layout.stride_bytes();
    if (!element_bytes || !text_bytes || stride == 0 || element_bytes->size() % stride != 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_composite_access: invalid storage or element stride");
        return std::nullopt;
    }

    for (const auto& field : layout.fields()) {
        if (field.offset_bytes > stride || field.size_bytes > stride - field.offset_bytes
            || (field.type == typeid(std::string) && field.size_bytes != 2 * sizeof(uint64_t))) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_composite_access: invalid layout for field '{}'", field.name);
            return std::nullopt;
        }
    }

    return CompositeAccess(elements, text, layout);
}

std::optional<Composite> CompositeAccess::at(size_t index) const noexcept
{
    if (index >= size())
        return std::nullopt;
    return Composite(*m_layout, *m_elements, *m_text, index);
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
