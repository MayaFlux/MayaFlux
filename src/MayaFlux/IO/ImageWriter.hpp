#pragma once

#include "MayaFlux/IO/ImageReader.hpp"

namespace MayaFlux::IO {

/**
 * @struct ImageWriteOptions
 * @brief Configuration for image writing.
 *
 * Format-specific knobs are interpreted by the concrete writer; unsupported
 * options are silently ignored. Compression maps differently per format:
 *   PNG   : deflate level [0..9], default 6
 *   JPG   : quality       [1..100], default 90
 *   EXR   : compression code (zip/piz/rle), default zip
 *   TGA   : RLE flag, default false
 */
struct ImageWriteOptions {
    int compression { -1 };
    int quality { 90 };
    bool flip_vertically { false };

    std::vector<std::string> channel_names;
};

/**
 * @class ImageWriter
 * @brief Abstract base for image format writers.
 *
 * Parallels ImageReader. Each concrete writer handles one or more file
 * extensions and validates that the supplied ImageData matches the format's
 * expected pixel variant (uint8 for PNG/JPG/BMP/TGA, float for EXR/HDR,
 * uint16 for 16-bit PNG).
 *
 * Writers are single-shot: one call to write() produces one file. No open/
 * close lifecycle because image formats are whole-file atomic.
 */
class MAYAFLUX_API ImageWriter {
public:
    virtual ~ImageWriter() = default;

    /**
     * @brief Check whether this writer handles the given filepath.
     */
    [[nodiscard]] virtual bool can_write(const std::string& filepath) const = 0;

    /**
     * @brief Write image data to disk.
     * @param filepath Destination path.
     * @param data     Image data. Must satisfy ImageData::is_consistent().
     * @param options  Format-specific options.
     * @return true on success. On failure call get_last_error().
     */
    virtual bool write(
        const std::string& filepath,
        const ImageData& data,
        const ImageWriteOptions& options = {}) = 0;

    /**
     * @brief File extensions handled by this writer (without dot).
     */
    [[nodiscard]] virtual std::vector<std::string> get_supported_extensions() const = 0;

    /**
     * @brief Last error message or empty string.
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;
};

using ImageWriterFactory = std::function<std::unique_ptr<ImageWriter>()>;

/**
 * @class ImageWriterRegistry
 * @brief Singleton registry dispatching image writes by file extension.
 *
 * Mirrors FileReaderRegistry. Concrete writers register themselves on static
 * initialization. create_writer(path) looks up the extension and returns a
 * fresh instance of the appropriate writer, or nullptr if none is registered.
 */
class MAYAFLUX_API ImageWriterRegistry {
public:
    static ImageWriterRegistry& instance()
    {
        static ImageWriterRegistry registry;
        return registry;
    }

    void register_writer(
        const std::vector<std::string>& extensions,
        const ImageWriterFactory& factory)
    {
        for (const auto& ext : extensions) {
            m_factories[ext] = factory;
        }
    }

    [[nodiscard]] std::unique_ptr<ImageWriter> create_writer(const std::string& filepath) const
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }

        auto it = m_factories.find(ext);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> get_registered_extensions() const
    {
        std::vector<std::string> exts;
        exts.reserve(m_factories.size());
        for (const auto& [ext, _] : m_factories) {
            exts.push_back(ext);
        }
        return exts;
    }

private:
    std::unordered_map<std::string, ImageWriterFactory> m_factories;
};

} // namespace MayaFlux::IO
