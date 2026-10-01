#include "VertexInsertion.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    template <typename T>
    DataVariant gather(
        std::span<const uint8_t> bytes,
        uint32_t stride,
        uint32_t offset,
        size_t count)
    {
        std::vector<T> out(count);
        for (size_t i = 0; i < count; ++i) {
            std::memcpy(&out[i], bytes.data() + i * stride + offset, sizeof(T));
        }
        return out;
    }

    size_t element_size(DataModality modality) noexcept
    {
        switch (modality) {
        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
        case DataModality::VERTEX_COLORS_RGB:
            return sizeof(glm::vec3);
        case DataModality::TEXTURE_COORDS_2D:
            return sizeof(glm::vec2);
        case DataModality::VERTEX_COLORS_RGBA:
            return sizeof(glm::vec4);
        case DataModality::SCALAR_F32:
            return sizeof(float);
        default:
            return 0;
        }
    }

    DataVariant gather_attribute(
        DataModality modality,
        std::span<const uint8_t> bytes,
        uint32_t stride,
        uint32_t offset,
        size_t count)
    {
        switch (modality) {
        case DataModality::TEXTURE_COORDS_2D:
            return gather<glm::vec2>(bytes, stride, offset, count);
        case DataModality::VERTEX_COLORS_RGBA:
            return gather<glm::vec4>(bytes, stride, offset, count);
        case DataModality::SCALAR_F32:
            return gather<float>(bytes, stride, offset, count);
        default:
            return gather<glm::vec3>(bytes, stride, offset, count);
        }
    }

    const char* layout_fault(const VertexLayout& layout) noexcept
    {
        if (layout.stride_bytes == 0) {
            return "layout stride_bytes is zero";
        }
        if (layout.attributes.empty()) {
            return "layout has no attributes";
        }
        for (const auto& attr : layout.attributes) {
            const size_t size = element_size(attr.component_modality);
            if (size == 0) {
                return "layout attribute has a modality with no vertex channel type";
            }
            if (static_cast<size_t>(attr.offset_in_vertex) + size > layout.stride_bytes) {
                return "layout attribute extends past the stride";
            }
        }
        return nullptr;
    }

} // namespace

VertexInsertion::VertexInsertion(std::vector<DataVariant>& channels)
    : m_channels(channels)
{
}

bool VertexInsertion::can_decode(const VertexLayout& layout) noexcept
{
    return layout_fault(layout) == nullptr;
}

bool VertexInsertion::insert_interleaved(
    std::span<const uint8_t> vertex_bytes,
    const VertexLayout& layout)
{
    if (const char* fault = layout_fault(layout)) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "VertexInsertion::insert_interleaved: {}", fault);
        return false;
    }
    if (vertex_bytes.empty() || vertex_bytes.size() % layout.stride_bytes != 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "VertexInsertion::insert_interleaved: {} bytes is not a nonzero multiple of stride {}",
            vertex_bytes.size(), layout.stride_bytes);
        return false;
    }

    const size_t count = vertex_bytes.size() / layout.stride_bytes;

    m_channels.clear();
    m_channels.reserve(layout.attributes.size());
    for (const auto& attr : layout.attributes) {
        m_channels.push_back(gather_attribute(
            attr.component_modality, vertex_bytes,
            layout.stride_bytes, attr.offset_in_vertex, count));
    }

    m_layout = layout;
    m_layout.vertex_count = static_cast<uint32_t>(count);

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::Runtime,
        "VertexInsertion::insert_interleaved: {} vertices into {} channels",
        count, m_channels.size());
    return true;
}

void VertexInsertion::clear()
{
    m_channels.clear();
    m_layout = {};
}

} // namespace MayaFlux::Kakshya
