#pragma once

#include "MayaFlux/IO/ImageWriter.hpp"

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

private:
    mutable std::string m_last_error;

    bool write_png(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_jpg(const std::string& filepath, const ImageData& data, const ImageWriteOptions& options);
    bool write_bmp(const std::string& filepath, const ImageData& data);
    bool write_tga(const std::string& filepath, const ImageData& data);
    bool write_hdr(const std::string& filepath, const ImageData& data);
};

} // namespace MayaFlux::IO
