#pragma once

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/IO/WriterRegistry.hpp"

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

using ImageWriterFactory = WriterFactory<ImageWriter>;

/**
 * @brief Singleton registry dispatching image writes by file extension.
 *
 * See WriterRegistry for behavior. Concrete writers register themselves on
 * static initialization.
 */
using ImageWriterRegistry = WriterRegistry<ImageWriter>;

} // namespace MayaFlux::IO
