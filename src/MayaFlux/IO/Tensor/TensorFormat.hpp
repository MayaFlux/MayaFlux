#pragma once

#include "TensorReader.hpp"
#include "TensorWriter.hpp"

namespace MayaFlux::IO {

/**
 * @struct TensorFormat
 * @brief Everything the registry needs to know about one tensor file format.
 *
 * probe inspects the leading bytes and the file size to recognise a format by
 * content. Leave it empty when the format has no reliable signature (a zip
 * container, a headerless dump) so it is chosen by extension or by name only.
 */
struct TensorFormat {
    std::string name;
    std::vector<std::string> extensions;
    std::function<bool(std::span<const std::byte> head, uint64_t file_size)> probe;
    std::function<std::unique_ptr<TensorReader>()> make_reader;
    std::function<std::unique_ptr<TensorWriter>()> make_writer;
};

/**
 * @class TensorFormatRegistry
 * @brief Registry that picks a tensor reader or writer by name, content or extension.
 *
 * Reading tries an explicit format name first, then content probes in
 * registration order, then the file extension. Writing, where no file exists
 * yet, tries the name and then the extension. A format is added by calling
 * register_format(); nothing else needs to change.
 *
 * Usage:
 * @code
 * auto& registry = TensorFormatRegistry::instance();
 * auto reader = registry.create_reader("weights.safetensors");
 * auto writer = registry.create_writer("out.npz");
 * @endcode
 */
class MAYAFLUX_API TensorFormatRegistry {
public:
    /** @brief The process-wide registry. */
    static TensorFormatRegistry& instance();

    /** @brief Add a format, replacing any earlier one with the same name. */
    void register_format(TensorFormat format);

    /** @brief Format with this name, or nullptr. */
    [[nodiscard]] const TensorFormat* find(std::string_view name) const;

    /** @brief Format recognised by content, then by extension, or nullptr. */
    [[nodiscard]] const TensorFormat* detect(const std::string& filepath) const;

    /**
     * @brief New reader for a file.
     * @param filepath Path to read.
     * @param format Optional format name that overrides detection.
     * @return Unopened reader, or nullptr when no format applies.
     */
    [[nodiscard]] std::unique_ptr<TensorReader> create_reader(
        const std::string& filepath, std::string_view format = {}) const;

    /**
     * @brief New writer for a destination path.
     * @param filepath Path to write.
     * @param format Optional format name that overrides the extension.
     * @return Writer, or nullptr when no writing format applies.
     */
    [[nodiscard]] std::unique_ptr<TensorWriter> create_writer(
        const std::string& filepath, std::string_view format = {}) const;

    /** @brief Names of all registered formats, in registration order. */
    [[nodiscard]] std::vector<std::string> format_names() const;

private:
    [[nodiscard]] const TensorFormat* by_extension(const std::string& filepath, bool need_writer) const;

    std::vector<TensorFormat> m_formats;
};

}
