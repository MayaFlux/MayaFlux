#pragma once

#include "ImageWriter.hpp"

namespace MayaFlux::IO {

/**
 * @class STBImageWriter
 * @brief ImageWriter implementation backed by stb_image_write.
 *
 * Handles PNG, JPG, BMP, TGA, and HDR. Accepts:
 *   - uint8 ImageData for PNG/JPG/BMP/TGA (RGBA8/RGB8/etc.)
 *   - float ImageData for HDR (RGB32F/RGBA32F, scanline radiance encoding)
 *
 * EXR is not handled here. Float data routed to PNG/JPG/BMP/TGA is rejected
 * with a clear error; use EXRWriter for float-precision outputs.
 *
 * Options honored:
 *   - ImageWriteOptions::compression : PNG deflate level [0..9]
 *   - ImageWriteOptions::quality     : JPG quality [1..100]
 *   - ImageWriteOptions::flip_vertically : vertical flip before encode
 *
 * Separate writer instances can encode concurrently.
 */
class MAYAFLUX_API STBImageWriter : public ImageWriter {
public:
    STBImageWriter() = default;
    ~STBImageWriter() override = default;

    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    bool write(
        const std::string& filepath,
        const ImageData& data,
        const ImageWriteOptions& options = {}) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    /**
     * @brief Register this writer with the ImageWriterRegistry.
     *
     * Called from engine/subsystem init. Idempotent.
     */
    static void register_with_registry();

    /**
     * @brief Encode 8-bit image data as a PNG in memory.
     * @param data    Image whose pixels are uint8.
     * @param options compression and flip_vertically are honoured.
     * @return The PNG's bytes, empty when the data is not uint8 or cannot be encoded.
     */
    [[nodiscard]] static std::vector<uint8_t> encode_png(
        const ImageData& data, const ImageWriteOptions& options = {});

private:
    mutable std::string m_last_error;

    bool write_png(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_jpg(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_bmp(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_tga(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_hdr(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
};

} // namespace MayaFlux::IO
