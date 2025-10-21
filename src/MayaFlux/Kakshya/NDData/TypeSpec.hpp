#pragma once

#include <glm/glm.hpp>

namespace MayaFlux {

template <typename T>
concept GlmVec2Type = std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::dvec2>;

template <typename T>
concept GlmVec3Type = std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>;

template <typename T>
concept GlmVec4Type = std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::dvec4>;

template <typename T>
concept GlmVectorType = GlmVec2Type<T> || GlmVec3Type<T> || GlmVec4Type<T>;

template <typename T>
concept GlmMatrixType = std::is_same_v<T, glm::mat2> || std::is_same_v<T, glm::mat3> || std::is_same_v<T, glm::mat4> || std::is_same_v<T, glm::dmat2> || std::is_same_v<T, glm::dmat3> || std::is_same_v<T, glm::dmat4>;

template <typename T>
concept GlmType = GlmVectorType<T> || GlmMatrixType<T>;

template <typename T>
constexpr size_t glm_component_count()
{
    if constexpr (GlmVec2Type<T>) {
        return 2;
    } else if constexpr (GlmVec3Type<T>) {
        return 3;
    } else if constexpr (GlmVec4Type<T>) {
        return 4;
    } else if constexpr (std::is_same_v<T, glm::mat2> || std::is_same_v<T, glm::dmat2>) {
        return 4;
    } else if constexpr (std::is_same_v<T, glm::mat3> || std::is_same_v<T, glm::dmat3>) {
        return 9;
    } else if constexpr (std::is_same_v<T, glm::mat4> || std::is_same_v<T, glm::dmat4>) {
        return 16;
    } else {
        return 0;
    }
}

template <typename T>
using glm_component_type = typename T::value_type;

} // namespace MayaFlux
