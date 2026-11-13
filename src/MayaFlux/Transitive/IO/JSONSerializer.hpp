#pragma once

#include "Serializer.hpp"

#include "MayaFlux/IO/FileWriter.hpp"

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Transitive/Reflect/Mirror.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace MayaFlux::IO {

/**
 * @class JSONSerializer
 * @brief Converts arbitrary C++ types to/from JSON strings and disk files.
 *
 * A type with a generated description (MF_DESCRIBE) is handled recursively
 * through its members, keyed by member name. A std::optional member is omitted
 * when empty. A std::any member, or a container of it, is never persisted and
 * is left out of both directions. Any other member without an encoding fails to
 * compile, so a missing Serializer specialization is never skipped silently.
 * Other types fall through to nlohmann's native converters (arithmetic,
 * std::string, bool) or to the built-in dispatchers for std::vector,
 * std::optional and std::unordered_map / std::map
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
     * @tparam T Any described struct, std::vector of one, arithmetic
     *           type, std::string, std::optional<T> or std::unordered_map
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
        const auto resolved = resolve_write_path(path);
        std::ofstream file(resolved, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            m_last_error = "Failed to open for writing: " + resolved;
            return false;
        }
        file << encode(value, indent);
        if (!file.good()) {
            m_last_error = "Write failed: " + resolved;
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
        return read<T>(path, [](nlohmann::json&) { });
    }

    /**
     * @brief Read @p path, let @p transform rewrite the parsed JSON, then deserialize into T.
     *
     * Lets a reader migrate an older layout before decoding. @p transform
     * receives the parsed document and may change it in place.
     */
    template <typename T, typename Transform>
    [[nodiscard]] std::optional<T> read(const std::string& path, Transform&& transform)
    {
        m_last_error.clear();
        const auto resolved = resolve_path(path);
        std::ifstream file(resolved);
        if (!file.is_open()) {
            m_last_error = "Failed to open for reading: " + resolved;
            return std::nullopt;
        }
        try {
            auto j = nlohmann::json::parse(file);
            std::forward<Transform>(transform)(j);
            T out {};
            from_json(j, out);
            return out;
        } catch (const std::exception& e) {
            m_last_error = std::string("read error in ") + resolved + ": " + e.what();
            return std::nullopt;
        }
    }

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    std::string m_last_error;

    [[nodiscard]] static std::string resolve_path(const std::string& filepath)
    {
        namespace fs = std::filesystem;
        auto normalized = std::string(filepath);
        std::ranges::replace(normalized, '\\', '/');
        if (fs::path(normalized).is_absolute())
            return normalized;
        if (fs::exists(normalized))
            return normalized;
        auto from_cwd = fs::current_path() / normalized;
        if (fs::exists(from_cwd))
            return from_cwd.string();
        auto from_root = fs::path(Config::project_root()) / normalized;
        if (fs::exists(from_root))
            return from_root.string();
        return normalized;
    }

    // -----------------------------------------------------------------------
    // Encoding engine
    // -----------------------------------------------------------------------

    template <typename T>
    static constexpr bool encodable()
    {
        if constexpr (Reflect::Described<T>) {
            return true;
        } else if constexpr (Reflect::is_optional_v<T>) {
            return encodable<typename Reflect::is_optional<T>::inner>();
        } else if constexpr (Reflect::is_vector_v<T>) {
            return encodable<typename Reflect::is_vector<T>::element>();
        } else if constexpr (Reflect::is_string_map_v<T>) {
            return encodable<typename Reflect::is_string_map<T>::element>();
        } else {
            return !std::is_same_v<T, std::any>;
        }
    }

    template <typename Member, typename Class>
    static void encode_member(nlohmann::json& j, const Class& obj, const Member& member)
    {
        using Value = typename Member::value_type;
        if constexpr (encodable<Value>()) {
            const auto& value = obj.*Member::pointer;
            if constexpr (Reflect::is_optional_v<Value>) {
                if (value.has_value()) {
                    j[std::string(member.name)] = to_json(*value);
                }
            } else {
                j[std::string(member.name)] = to_json(value);
            }
        }
    }

    template <typename Member, typename Class>
    static void decode_member(const nlohmann::json& j, Class& obj, const Member& member)
    {
        using Value = typename Member::value_type;
        if constexpr (encodable<Value>()) {
            const std::string key(member.name);
            if constexpr (Reflect::is_optional_v<Value>) {
                if (!j.contains(key) || j.at(key).is_null()) {
                    obj.*Member::pointer = std::nullopt;
                } else {
                    typename Reflect::is_optional<Value>::inner inner {};
                    from_json(j.at(key), inner);
                    obj.*Member::pointer = std::move(inner);
                }
            } else if (j.contains(key)) {
                from_json(j.at(key), obj.*Member::pointer);
            }
        }
    }

    template <typename T>
    static nlohmann::json to_json(const T& val)
    {
        if constexpr (Reflect::Described<T>) {
            nlohmann::json j = nlohmann::json::object();
            std::apply(
                [&](const auto&... members) {
                    (encode_member(j, val, members), ...);
                },
                Reflect::Generated::Description<T>::members());
            return j;
        } else if constexpr (Reflect::is_optional_v<T>) {
            if (!val.has_value()) {
                return nullptr;
            }
            return to_json(*val);
        } else if constexpr (Reflect::is_vector_v<T>) {
            auto arr = nlohmann::json::array();
            for (const auto& item : val) {
                arr.push_back(to_json(item));
            }
            return arr;
        } else if constexpr (Reflect::is_string_map_v<T>) {
            nlohmann::json j = nlohmann::json::object();
            for (const auto& [k, v] : val) {
                j[k] = to_json(v);
            }
            return j;
        } else if constexpr (std::is_enum_v<T>) {
            return static_cast<std::underlying_type_t<T>>(val);
        } else if constexpr (std::is_same_v<T, nlohmann::json>) {
            return val;
        } else {
            /// Extension point — specialize Serializer<T> to handle custom types
            return Serializer<T>::to_json(val);
        }
    }

    // -----------------------------------------------------------------------
    // Decoding engine
    // -----------------------------------------------------------------------

    template <typename T>
    static void from_json(const nlohmann::json& j, T& out)
    {
        if constexpr (Reflect::Described<T>) {
            if (!j.is_object()) {
                throw nlohmann::json::type_error::create(302, "expected object for described type", &j);
            }
            std::apply(
                [&](const auto&... members) {
                    (decode_member(j, out, members), ...);
                },
                Reflect::Generated::Description<T>::members());
        } else if constexpr (Reflect::is_optional_v<T>) {
            using Inner = typename Reflect::is_optional<T>::inner;
            if (j.is_null()) {
                out = std::nullopt;
            } else {
                Inner inner {};
                from_json(j, inner);
                out = std::move(inner);
            }
        } else if constexpr (Reflect::is_vector_v<T>) {
            using V = typename Reflect::is_vector<T>::element;
            if (!j.is_array())
                throw nlohmann::json::type_error::create(302, "expected array", &j);
            out.clear();
            out.reserve(j.size());
            for (const auto& item : j) {
                V element {};
                from_json(item, element);
                out.push_back(std::move(element));
            }

        } else if constexpr (Reflect::is_string_map_v<T>) {
            using V = typename Reflect::is_string_map<T>::element;
            if (!j.is_object())
                throw nlohmann::json::type_error::create(302, "expected object for map", &j);
            out.clear();
            for (const auto& [k, v] : j.items()) {
                V val {};
                from_json(v, val);
                out.emplace(k, std::move(val));
            }
        } else if constexpr (std::is_enum_v<T>) {
            out = static_cast<T>(j.get<std::underlying_type_t<T>>());

        } else if constexpr (std::is_same_v<T, nlohmann::json>) {
            out = j;

        } else {
            Serializer<T>::from_json(j, out);
        }
    }
};

} // namespace MayaFlux::IO
