#pragma once

#include <nlohmann/json.hpp>

namespace MayaFlux::IO {

/**
 * @struct Serializer
 * @brief Extension point for JSONSerializer.
 *
 * Specialize this struct for any type that JSONSerializer does not
 * handle natively. The specialization must provide:
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

} // namespace MayaFlux::IO
