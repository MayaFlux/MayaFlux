#pragma once

#include "MayaFlux/Transitive/IO/Serializer.hpp"

namespace MayaFlux::Reflect {

template <typename T>
concept GlmSerializable = GlmType<T>;

}

namespace MayaFlux::IO {

/**
 * @brief JSONSerializer extension for GLM vector and matrix types.
 *
 * Encodes GLM types as flat JSON arrays of their scalar components.
 * Include this header in any translation unit that serializes GLM types
 * via JSONSerializer — the specialization is found automatically.
 *
 * Supported types: vec2, vec3, vec4, dvec2, dvec3, dvec4,
 *                  mat2, mat3, mat4, dmat2, dmat3, dmat4.
 */

template <typename T>
    requires Reflect::GlmSerializable<T>
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
