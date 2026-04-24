#pragma once

#include <nlohmann/json.hpp>

namespace MayaFlux::IO {

/**
 * @class JSONSerializer
 * @brief Converts nlohmann::json documents to/from strings and disk files.
 *
 * Thin IO layer — owns no knowledge of what the JSON means.
 * Callers (StateEncoder, StateDecoder) own that interpretation.
 *
 * encode/decode operate in memory; write/read are built on top of them.
 * last_error() is set by decode, write, and read on failure.
 */
class MAYAFLUX_API JSONSerializer {
public:
    JSONSerializer() = default;

    JSONSerializer(const JSONSerializer&) = delete;
    JSONSerializer& operator=(const JSONSerializer&) = delete;
    JSONSerializer(JSONSerializer&&) = default;
    JSONSerializer& operator=(JSONSerializer&&) = default;

    /**
     * @brief Serialize @p doc to a JSON string.
     * @param indent Spaces per indentation level (default 2, -1 for compact).
     */
    [[nodiscard]] std::string encode(const nlohmann::json& doc, int indent = 2);

    /**
     * @brief Parse a JSON string into a document.
     * @return Parsed document, or nullopt if @p str is malformed.
     *         On failure call last_error().
     */
    [[nodiscard]] std::optional<nlohmann::json> decode(const std::string& str);

    /**
     * @brief Encode @p doc and write it to @p path (created or truncated).
     * @return True on success. On failure call last_error().
     */
    [[nodiscard]] bool write(const std::string& path, const nlohmann::json& doc, int indent = 2);

    /**
     * @brief Read @p path and decode it into a document.
     * @return Parsed document, or nullopt if the file is missing or malformed.
     *         On failure call last_error().
     */
    [[nodiscard]] std::optional<nlohmann::json> read(const std::string& path);

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    std::string m_last_error;
};

} // namespace MayaFlux::IO
