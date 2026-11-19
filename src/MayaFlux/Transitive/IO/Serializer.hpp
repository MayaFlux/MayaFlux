#pragma once

#include <nlohmann/json.hpp>

namespace MayaFlux::IO {

/**
 * @struct Serializer
 * @brief Extension point for JSONSerializer.
 *
 * Specialize this struct for any type that JSONSerializer does not
 * handle natively. The primary template serves the types nlohmann converts by
 * itself. A specialization must provide:
 *   static nlohmann::json to_json(const T&);
 *   static void from_json(const nlohmann::json&, T&);
 *
 * Example — adding support for a custom Vec3 type:
 * @code
 * template <>
 * struct Serializer<MyVec3> {
 *     static nlohmann::json to_json(const MyVec3& v) {
 *         return nlohmann::json::array({ v.x, v.y, v.z });
 *     }
 *     static void from_json(const nlohmann::json& j, MyVec3& out) {
 *         out = { j[0], j[1], j[2] };
 *     }
 * };
 * @endcode
 */
template <typename T>
struct Serializer {
    static nlohmann::json to_json(const T& val) { return val; }
    static void from_json(const nlohmann::json& j, T& out) { out = j.get<T>(); }
};

/**
 * @brief GLM vectors and matrices as flat JSON arrays of their scalar components.
 *
 * Covers vec2, vec3, vec4, dvec2, dvec3, dvec4, mat2, mat3, mat4, dmat2, dmat3
 * and dmat4.
 */
template <typename T>
    requires GlmType<T>
struct Serializer<T> {

    static nlohmann::json to_json(const T& v)
    {
        constexpr auto n = glm_component_count<T>();
        using Comp = glm_component_type<T>;
        auto arr = nlohmann::json::array();
        const Comp* ptr = &v[0];
        for (size_t i = 0; i < n; ++i)
            arr.push_back(ptr[i]);
        return arr;
    }

    static void from_json(const nlohmann::json& j, T& out)
    {
        constexpr auto n = glm_component_count<T>();
        if (!j.is_array()) {
            throw nlohmann::json::type_error::create(302, "expected array for glm type", &j);
        }

        if (j.size() != n) {
            throw nlohmann::json::other_error::create(501,
                "glm component count mismatch: expected "
                    + std::to_string(n) + ", got " + std::to_string(j.size()),
                &j);
        }

        using Comp = glm_component_type<T>;
        Comp* ptr = &out[0];
        for (size_t i = 0; i < n; ++i)
            ptr[i] = j[i].get<Comp>();
    }
};

} // namespace MayaFlux::IO
