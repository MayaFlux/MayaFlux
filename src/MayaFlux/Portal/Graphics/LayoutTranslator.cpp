#include "LayoutTranslator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

vk::Format VertexLayoutTranslator::modality_to_vk_format(Kakshya::DataModality modality)
{
    using namespace Kakshya;

    switch (modality) {
    case DataModality::VERTEX_POSITIONS_3D:
    case DataModality::VERTEX_NORMALS_3D:
    case DataModality::VERTEX_TANGENTS_3D:
    case DataModality::VERTEX_COLORS_RGB:
        return vk::Format::eR32G32B32Sfloat;

    case DataModality::TEXTURE_COORDS_2D:
        return vk::Format::eR32G32Sfloat;

    case DataModality::VERTEX_COLORS_RGBA:
        return vk::Format::eR32G32B32A32Sfloat;

    case DataModality::AUDIO_1D:
    case DataModality::AUDIO_MULTICHANNEL:
        return vk::Format::eR64Sfloat;

    case DataModality::SPECTRAL_2D:
        return vk::Format::eR32G32Sfloat;

    case DataModality::TRANSFORMATION_MATRIX:
        return vk::Format::eR32G32B32A32Sfloat;

    default:
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Unknown modality for vertex format conversion, defaulting to R32G32B32Sfloat");
        return vk::Format::eR32G32B32Sfloat;
    }
}

std::pair<
    std::vector<Core::VertexBinding>,
    std::vector<Core::VertexAttribute>>
VertexLayoutTranslator::translate_layout(
    const Kakshya::VertexLayout& layout,
    uint32_t binding_index)
{
    if (layout.attributes.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot translate empty vertex layout");
        return {};
    }

    if (layout.vertex_count == 0) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Vertex layout has zero vertices");
    }

    if (layout.stride_bytes == 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Vertex layout has zero stride");
        return {};
    }

    std::vector<Core::VertexBinding> bindings;
    bindings.emplace_back(
        binding_index,
        layout.stride_bytes,
        false,
        vk::VertexInputRate::eVertex);

    std::vector<Core::VertexAttribute> attributes;
    uint32_t location = 0;

    for (const auto& attr : layout.attributes) {
        attributes.emplace_back(
            location,
            binding_index,
            modality_to_vk_format(attr.component_modality),
            attr.offset_in_vertex);

        MF_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
            "Vertex attribute: location={}, format={}, offset={}",
            location,
            vk::to_string(attributes.back().format),
            attr.offset_in_vertex);

        location++;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "Translated vertex layout: {} vertices, {} bytes stride, {} attributes",
        layout.vertex_count, layout.stride_bytes, layout.attributes.size());

    return { bindings, attributes };
}

uint32_t VertexLayoutTranslator::get_modality_size_bytes(Kakshya::DataModality modality)
{
    using namespace Kakshya;

    switch (modality) {
    case DataModality::VERTEX_POSITIONS_3D:
    case DataModality::VERTEX_NORMALS_3D:
    case DataModality::VERTEX_TANGENTS_3D:
    case DataModality::VERTEX_COLORS_RGB:
        return 12; // 3 * float32

    case DataModality::TEXTURE_COORDS_2D:
        return 8; // 2 * float32

    case DataModality::VERTEX_COLORS_RGBA:
        return 16; // 4 * float32

    case DataModality::AUDIO_1D:
    case DataModality::AUDIO_MULTICHANNEL:
        return 8; // float64

    case DataModality::SPECTRAL_2D:
        return 8; // 2 * float32

    case DataModality::TRANSFORMATION_MATRIX:
        return 64; // 4x4 * float32

    default:
        return 4;
    }
}

std::string_view VertexLayoutTranslator::describe_modality(Kakshya::DataModality modality)
{
    using namespace Kakshya;

    switch (modality) {
    case DataModality::VERTEX_POSITIONS_3D:
        return "vec3 (positions)";
    case DataModality::VERTEX_NORMALS_3D:
        return "vec3 (normals)";
    case DataModality::VERTEX_TANGENTS_3D:
        return "vec3 (tangents)";
    case DataModality::VERTEX_COLORS_RGB:
        return "vec3 (color RGB)";
    case DataModality::VERTEX_COLORS_RGBA:
        return "vec4 (color RGBA)";
    case DataModality::TEXTURE_COORDS_2D:
        return "vec2 (UV)";
    case DataModality::AUDIO_1D:
        return "double (audio sample)";
    case DataModality::AUDIO_MULTICHANNEL:
        return "double (audio multichannel)";
    case DataModality::SPECTRAL_2D:
        return "vec2 (frequency, magnitude)";
    case DataModality::TRANSFORMATION_MATRIX:
        return "mat4 (transformation)";
    default:
        return "unknown";
    }
}

} // namespace MayaFlux::Portal::Graphics
