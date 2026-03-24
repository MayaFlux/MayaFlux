#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct TextureAccess
 * @brief Memory-compatible view of a DataVariant for texel upload.
 *
 * Analogous to VertexLayout: describes what the data IS in memory terms —
 * pointer, byte count, and the GpuDataFormat that faithfully represents it —
 * with no knowledge of Vulkan format enumerants or Portal types.
 * The Portal layer maps GpuDataFormat to ImageFormat or vk::Format.
 *
 * For types that map directly (vec2, vec4, float, uint8_t, complex<float>)
 * data_ptr points into the variant's own storage — zero copy.
 *
 * For glm::vec3, which has no universally-supported 3-channel float sampled
 * image format in Vulkan, the variant is promoted to VEC4_F32 with W=0.
 * In this case conversion_buffer holds the promoted data and data_ptr
 * points into it. The promotion is logged as a warning.
 *
 * Rejected types (as_texture_access returns std::nullopt, logs error):
 *   vector<complex<double>>  RG64F is not a sampled image format in Vulkan.
 *   vector<glm::mat4>        Ambiguous layout; caller must unpack to vec4 first.
 *   VEC2_F64 / VEC3_F64 / VEC4_F64 variants do not exist in DataVariant —
 *                            included in GpuDataFormat for buffer use only.
 */
struct TextureAccess {
    const void* data_ptr = nullptr;
    size_t byte_count = 0;
    GpuDataFormat format = GpuDataFormat::VEC4_F32;

    /**
     * @brief Bytes per texel for this format.
     */
    [[nodiscard]] size_t bytes_per_texel() const noexcept;

    /**
     * @brief Number of channels (components per texel).
     */
    [[nodiscard]] uint32_t channel_count() const noexcept;

    /**
     * @brief True when data_ptr points into conversion_buffer rather than
     *        the original variant storage (i.e. a vec3 promotion occurred).
     */
    [[nodiscard]] bool was_promoted() const noexcept
    {
        return !conversion_buffer.empty();
    }

    /**
     * @brief Promotion buffer. Non-empty only for vec3 → vec4 promotion.
     *        Keeps promoted data alive for the lifetime of this struct.
     */
    std::vector<std::byte> conversion_buffer;
};

/**
 * @brief Extract a TextureAccess from a DataVariant.
 * @param variant Source data.
 * @return Populated TextureAccess, or std::nullopt on incompatible type.
 *
 * DataVariant active type → GpuDataFormat mapping:
 *   vector<uint8_t>          → UINT8
 *   vector<uint16_t>         → UINT16
 *   vector<uint32_t>         → UINT32
 *   vector<float>            → FLOAT32
 *   vector<double>           → FLOAT32  (narrowed, warned)
 *   vector<complex<float>>   → VEC2_F32 (identical layout to glm::vec2)
 *   vector<glm::vec2>        → VEC2_F32
 *   vector<glm::vec3>        → VEC4_F32 (W=0 promotion, warned)
 *   vector<glm::vec4>        → VEC4_F32
 *   vector<complex<double>>  → std::nullopt (logged error)
 *   vector<glm::mat4>        → std::nullopt (logged error)
 */
[[nodiscard]] MAYAFLUX_API std::optional<TextureAccess>
as_texture_access(const DataVariant& variant);

} // namespace MayaFlux::Kakshya
