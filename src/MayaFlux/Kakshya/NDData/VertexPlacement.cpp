#include "VertexPlacement.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kakshya {

namespace {

    template <typename T>
    T load(const uint8_t* at) noexcept
    {
        T value {};
        std::memcpy(&value, at, sizeof(T));
        return value;
    }

    template <typename T>
    void store(uint8_t* at, const T& value) noexcept
    {
        std::memcpy(at, &value, sizeof(T));
    }

    glm::vec3 direction(const glm::mat3& matrix, const glm::vec3& vector) noexcept
    {
        const glm::vec3 moved = matrix * vector;
        const float length = glm::length(moved);
        return length > 0.0F ? moved / length : moved;
    }

} // namespace

bool place_vertices(
    std::span<uint8_t> vertex_bytes,
    const VertexLayout& layout,
    const glm::mat4& transform) noexcept
{
    const size_t stride = layout.stride_bytes;
    if (stride == 0 || vertex_bytes.empty() || vertex_bytes.size() % stride != 0) {
        return false;
    }

    const auto fits = [stride](const VertexAttributeLayout& attribute, size_t size) {
        return attribute.offset_in_vertex + size <= stride;
    };

    const auto has_position = std::ranges::any_of(layout.attributes, [&](const auto& attribute) {
        return attribute.component_modality == DataModality::VERTEX_POSITIONS_3D
            && fits(attribute, sizeof(glm::vec3));
    });
    if (!has_position) {
        return false;
    }

    const glm::mat3 linear(transform);
    const float determinant = glm::determinant(linear);
    const bool invertible = std::isfinite(determinant) && determinant != 0.0F;
    const glm::mat3 normal_matrix = invertible ? glm::transpose(glm::inverse(linear)) : glm::mat3(1.0F);

    const size_t count = vertex_bytes.size() / stride;
    for (const auto& attribute : layout.attributes) {
        const DataModality modality = attribute.component_modality;
        const bool is_position = modality == DataModality::VERTEX_POSITIONS_3D;
        if (!is_position && !invertible) {
            continue;
        }

        const bool is_normal = modality == DataModality::VERTEX_NORMALS_3D;
        const bool is_tangent3 = modality == DataModality::VERTEX_TANGENTS_3D;
        const bool is_tangent4 = modality == DataModality::VERTEX_TANGENTS_4D;
        if (!is_position && !is_normal && !is_tangent3 && !is_tangent4) {
            continue;
        }
        if (!fits(attribute, is_tangent4 ? sizeof(glm::vec4) : sizeof(glm::vec3))) {
            continue;
        }

        for (size_t i = 0; i < count; ++i) {
            uint8_t* at = vertex_bytes.data() + i * stride + attribute.offset_in_vertex;
            if (is_position) {
                store(at, glm::vec3(transform * glm::vec4(load<glm::vec3>(at), 1.0F)));
            } else if (is_normal) {
                store(at, direction(normal_matrix, load<glm::vec3>(at)));
            } else if (is_tangent3) {
                store(at, direction(linear, load<glm::vec3>(at)));
            } else {
                const auto tangent = load<glm::vec4>(at);
                store(at, glm::vec4(direction(linear, glm::vec3(tangent)), tangent.w));
            }
        }
    }
    return true;
}

} // namespace MayaFlux::Kakshya
