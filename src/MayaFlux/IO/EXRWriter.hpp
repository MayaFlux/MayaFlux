#pragma once

#include "MayaFlux/IO/ImageWriter.hpp"

namespace MayaFlux::IO {

/**
 * @class EXRWriter
 * @brief ImageWriter implementation backed by tinyexr.
 *
 * Writes OpenEXR single-part scanline images with 32-bit float pixel data.
 * Handles arbitrary channel counts with named channels. State textures use
 * named channels like "position.x", "entity_id", "trigger_kind" directly,
 * so the file is partly self-documenting.
 *
 * Accepts float ImageData only. Integer variants (uint8, uint16) are
 * rejected: EXR is a floating-point format and storing integer data via
 * reinterpret would produce files no viewer reads sensibly.
 *
 * Channel naming:
 *   - ImageWriteOptions::channel_names, if non-empty and sized to match
 *     data.channels, is used verbatim.
 *   - Otherwise defaults are applied by channel count:
 *       1 : "Y"
 *       2 : "Y", "A"
 *       3 : "B", "G", "R"            (EXR convention)
 *       4 : "A", "B", "G", "R"       (EXR convention)
 *       N (>4) : "C0", "C1", ... "C(N-1)"
 *
 * Compression:
 *   - ImageWriteOptions::compression is the tinyexr TINYEXR_COMPRESSIONTYPE_*
 *     value. -1 (default) maps to TINYEXR_COMPRESSIONTYPE_ZIP (scanline block
 *     zip, widest viewer support).
 *
 * Vertical flip:
 *   - Honored via ImageWriteOptions::flip_vertically.
 *
 * Limitations:
 *   - Single-part only. Multi-part EXR is deferred; for layered state
 *     textures, write multiple EXR files with a companion schema/manifest.
 *   - No tiled output, no deep images.
 *   - Half precision output not yet exposed (requires uint16 ImageData and
 *     TINYEXR_PIXELTYPE_HALF plumbing; deferred).
 */
class MAYAFLUX_API EXRWriter : public ImageWriter {
public:
    EXRWriter() = default;
    ~EXRWriter() override = default;

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

    /**
     * @brief Resolve channel name list based on options and channel count.
     */
    [[nodiscard]] std::vector<std::string> resolve_channel_names(
        const ImageData& data, const ImageWriteOptions& options) const;

    /**
     * @brief Deinterleave source float buffer into N planar channel buffers.
     *
     * Source layout: pixel-interleaved (P0C0 P0C1 P0C2 P0C3 P1C0 P1C1 ...).
     * Output layout: one vector<float> per channel, scanline order.
     * If flip_vertically is true, rows are emitted bottom-up.
     */
    static std::vector<std::vector<float>> deinterleave(
        const std::vector<float>& src,
        uint32_t width, uint32_t height, uint32_t channels,
        bool flip_vertically);
};

} // namespace MayaFlux::IO
