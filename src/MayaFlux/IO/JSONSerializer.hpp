#pragma once

#include "Reflection.hpp"

#include <nlohmann/json.hpp>

#include "fstream"

namespace MayaFlux::IO {

/**
 * @class JSONSerializer
 * @brief Converts arbitrary C++ types to/from JSON strings and disk files.
 *
 * Encoding and decoding are driven by the Reflectable concept: any type that
 * provides a static constexpr describe() returning a tuple of Property /
 * OptionalProperty descriptors is handled recursively.  Types without a
 * describe() fall through to nlohmann's native converters (arithmetic,
 * std::string, bool) or to the built-in dispatchers for std::vector,
 * std::optional, std::unordered_map / std::map, and glm vec/mat types.
 *
 * Callers never touch nlohmann::json directly.  JSONSerializer owns all
 * knowledge of the wire format.
 *
 * File operations are built on top of the in-memory encode / decode pair.
 * last_error() is set on any failure; it is cleared at the start of every
 * fallible call.
 */
class MAYAFLUX_API JSONSerializer {
public:
    JSONSerializer() = default;

    JSONSerializer(const JSONSerializer&) = delete;
    JSONSerializer& operator=(const JSONSerializer&) = delete;
    JSONSerializer(JSONSerializer&&) = default;
    JSONSerializer& operator=(JSONSerializer&&) = default;

    // -----------------------------------------------------------------------
    // Encode
    // -----------------------------------------------------------------------

    /**
     * @brief Serialize @p value to a JSON string.
     * @tparam T Any Reflectable struct, std::vector<Reflectable>, arithmetic
     *           type, std::string, std::optional<T>, std::unordered_map, or
     *           glm vec/mat type.
     * @param indent Spaces per indentation level (-1 for compact).
     */
    template <typename T>
    [[nodiscard]] std::string encode(const T& value, int indent = 2)
    {
        return to_json(value).dump(indent);
    }

    /**
     * @brief Encode @p value and write to @p path (created or truncated).
     * @return True on success. On failure call last_error().
     */
    template <typename T>
    [[nodiscard]] bool write(const std::string& path, const T& value, int indent = 2)
    {
        m_last_error.clear();
        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            m_last_error = "Failed to open for writing: " + path;
            return false;
        }
        file << encode(value, indent);
        if (!file.good()) {
            m_last_error = "Write failed: " + path;
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Decode
    // -----------------------------------------------------------------------

    /**
     * @brief Parse @p str and deserialize into T.
     * @return Populated instance, or nullopt on any parse or structural error.
     *         On failure call last_error().
     */
    template <typename T>
    [[nodiscard]] std::optional<T> decode(const std::string& str)
    {
        m_last_error.clear();
        try {
            auto j = nlohmann::json::parse(str);
            T out {};
            from_json(j, out);
            return out;
        } catch (const std::exception& e) {
            m_last_error = std::string("decode error: ") + e.what();
            return std::nullopt;
        }
    }

    /**
     * @brief Read @p path and deserialize into T.
     * @return Populated instance, or nullopt on file or parse error.
     *         On failure call last_error().
     */
    template <typename T>
    [[nodiscard]] std::optional<T> read(const std::string& path)
    {
        m_last_error.clear();
        std::ifstream file(path);
        if (!file.is_open()) {
            m_last_error = "Failed to open for reading: " + path;
            return std::nullopt;
        }
        try {
            auto j = nlohmann::json::parse(file);
            T out {};
            from_json(j, out);
            return out;
        } catch (const std::exception& e) {
            m_last_error = std::string("read error in ") + path + ": " + e.what();
            return std::nullopt;
        }
    }

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    std::string m_last_error;

    // -----------------------------------------------------------------------
    // Encoding engine
    // -----------------------------------------------------------------------

    template <typename T>
    static nlohmann::json to_json(const T& val)
    {
        if constexpr (Reflectable<T>) {
            nlohmann::json j = nlohmann::json::object();
            std::apply(
                [&](const auto&... props) {
                    (encode_property(j, val, props), ...);
                },
                T::describe());
            return j;
        } else if constexpr (is_optional_v<T>) {
            if (!val.has_value()) {
                return nullptr;
            }
            return to_json(*val);
        } else if constexpr (is_vector_v<T>) {
            auto arr = nlohmann::json::array();
            for (const auto& item : val) {
                arr.push_back(to_json(item));
            }
            return arr;
        } else if constexpr (is_string_map_v<T>) {
            nlohmann::json j = nlohmann::json::object();
            for (const auto& [k, v] : val) {
                j[k] = to_json(v);
            }
            return j;
        } else if constexpr (GlmSerializable<T>) {
            return encode_glm(val);
        } else if constexpr (std::is_enum_v<T>) {
            return static_cast<std::underlying_type_t<T>>(val);
        } else {
            return val;
        }
    }

    template <typename Class, typename T>
    static void encode_property(
        nlohmann::json& j,
        const Class& obj,
        const Property<Class, T>& prop)
    {
        j[prop.key] = to_json(obj.*prop.member);
    }

    template <typename Class, typename T>
    static void encode_property(
        nlohmann::json& j,
        const Class& obj,
        const OptionalProperty<Class, T>& prop)
    {
        const auto& opt = obj.*prop.member;
        if (opt.has_value()) {
            j[prop.key] = to_json(*opt);
        }
    }

    // -----------------------------------------------------------------------
    // Decoding engine
    // -----------------------------------------------------------------------

    template <typename T>
    static void from_json(const nlohmann::json& j, T& out)
    {
        if constexpr (Reflectable<T>) {
            if (!j.is_object()) {
                throw nlohmann::json::type_error::create(
                    302, "expected object for Reflectable type", &j);
            }
            std::apply(
                [&](const auto&... props) {
                    (decode_property(j, out, props), ...);
                },
                T::describe());
        } else if constexpr (is_optional_v<T>) {
            using Inner = typename is_optional<T>::inner;
            if (j.is_null()) {
                out = std::nullopt;
            } else {
                Inner inner {};
                from_json(j, inner);
                out = std::move(inner);
            }
        } else if constexpr (is_vector_v<T>) {
            using V = typename is_vector<T>::element;
            if (!j.is_array()) {
                throw nlohmann::json::type_error::create(302, "expected array", &j);
            }
            out.clear();
            out.reserve(j.size());
            for (const auto& item : j) {
                V element {};
                from_json(item, element);
                out.push_back(std::move(element));
            }
        } else if constexpr (is_string_map_v<T>) {
            using V = typename is_string_map<T>::element;
            if (!j.is_object()) {
                throw nlohmann::json::type_error::create(302, "expected object for map", &j);
            }
            out.clear();
            for (const auto& [k, v] : j.items()) {
                V val {};
                from_json(v, val);
                out.emplace(k, std::move(val));
            }
        } else if constexpr (GlmSerializable<T>) {
            decode_glm(j, out);
        } else if constexpr (std::is_enum_v<T>) {
            out = static_cast<T>(j.get<std::underlying_type_t<T>>());
        } else {
            out = j.get<T>();
        }
    }

    template <typename Class, typename T>
    static void decode_property(
        const nlohmann::json& j,
        Class& obj,
        const Property<Class, T>& prop)
    {
        if (j.contains(prop.key)) {
            from_json(j.at(prop.key), obj.*prop.member);
        }
    }

    template <typename Class, typename T>
    static void decode_property(
        const nlohmann::json& j,
        Class& obj,
        const OptionalProperty<Class, T>& prop)
    {
        if (!j.contains(prop.key) || j.at(prop.key).is_null()) {
            obj.*prop.member = std::nullopt;
        } else {
            T inner {};
            from_json(j.at(prop.key), inner);
            obj.*prop.member = std::move(inner);
        }
    }

    // -----------------------------------------------------------------------
    // GLM encode / decode
    // -----------------------------------------------------------------------

    template <typename T>
        requires GlmSerializable<T>
    static nlohmann::json encode_glm(const T& v)
    {
        constexpr auto n = glm_component_count<T>();
        using Comp = glm_component_type<T>;
        auto arr = nlohmann::json::array();
        const Comp* ptr = &v[0];
        for (size_t i = 0; i < n; ++i) {
            arr.push_back(ptr[i]);
        }
        return arr;
    }

    template <typename T>
        requires GlmSerializable<T>
    static void decode_glm(const nlohmann::json& j, T& out)
    {
        if (!j.is_array()) {
            throw nlohmann::json::type_error::create(302, "expected array for glm type", &j);
        }
        constexpr auto n = glm_component_count<T>();
        if (j.size() != n) {
            throw nlohmann::json::other_error::create(
                501, "glm component count mismatch", &j);
        }
        using Comp = glm_component_type<T>;
        Comp* ptr = &out[0];
        for (size_t i = 0; i < n; ++i) {
            ptr[i] = j[i].get<Comp>();
        }
    }
};

} // namespace MayaFlux::IO
