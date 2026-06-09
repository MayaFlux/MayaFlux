#pragma once
#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct VertexAccessConfig
 * @brief Default attribute values for shader-compatible vertex conversion.
 *
 * Applied when DataVariant contains only positional data and the target
 * layout requires additional attributes (color, size, thickness, uv).
 */
struct MAYAFLUX_API VertexAccessConfig {
    glm::vec3 default_color { 1.0F, 1.0F, 1.0F };
    float default_size { 3.0F };
    float default_thickness { 1.0F };
    float default_weight { 0.0F };
    glm::vec2 default_uv { 0.0F, 0.0F };
    glm::vec3 default_normal { 0.0F, 0.0F, 1.0F };
    glm::vec3 default_tangent { 1.0F, 0.0F, 0.0F };
};
/**
 * @struct VertexAccess
 * @brief Memory-compatible view of channel data assembled into full 60-byte vertices.
 *
 * Mirrors TextureAccess: describes what the data IS in memory terms —
 * pointer, byte count, and a VertexLayout — with no knowledge of Vulkan,
 * Portal, or concrete vertex structs (PointVertex, LineVertex, MeshVertex).
 *
 * A vertex is always the canonical 60-byte record:
 *   position (vec3, 12) | color (vec3, 12) | scalar (float, 4) |
 *   uv (vec2, 8) | normal (vec3, 12) | tangent (vec3, 12)
 *
 * Zero-copy path: single vector<uint8_t> channel of N*60 bytes. data_ptr
 * points directly into variant storage; conversion_buffer is empty.
 *
 * Assembly path: one or more typed channels supplied in canonical field order.
 * Slot 0 = position (vec3), slot 1 = color (vec3), slot 2 = scalar (float),
 * slot 3 = uv (vec2), slot 4 = normal (vec3), slot 5 = tangent (vec3).
 * Missing trailing slots are filled from VertexAccessConfig defaults.
 * Assembled bytes are held in conversion_buffer; data_ptr points into it.
 *
 * Source element size is taken from the supplied variant type. A vec4 color
 * channel (16 bytes) copies 12 bytes into the color field; the alpha is
 * discarded. A float scalar channel (4 bytes) maps exactly. Oversized
 * elements are truncated; undersized elements are zero-padded within the field.
 */
struct MAYAFLUX_API VertexAccess {
    const void* data_ptr = nullptr;
    size_t byte_count = 0;
    VertexLayout layout;

    /**
     * @brief True when data_ptr points into conversion_buffer rather than
     *        the original variant storage.
     */
    [[nodiscard]] bool was_converted() const noexcept
    {
        return !conversion_buffer.empty();
    }

    /**
     * @brief Conversion buffer. Non-empty only when a type conversion was
     *        performed (scalar waveform → vec3, complex → vec3, etc.).
     *        Keeps converted data alive for the lifetime of this struct.
     */
    std::vector<std::byte> conversion_buffer;
};

/**
 * @brief Extract a VertexAccess from a DataVariant.
 *
 * Inspects the active variant type and produces a VertexAccess whose
 * layout describes the resulting vertex data. Scalar and complex types
 * are converted to a positional vec3 waveform representation. GLM vector
 * types are passed through without copying.
 *
 * @param variant Source data.
 * @return Populated VertexAccess, or std::nullopt on incompatible type.
 */
[[nodiscard]] MAYAFLUX_API std::optional<VertexAccess>
as_vertex_access(std::span<const DataVariant> channels,
    const VertexAccessConfig& config = {});

/**
 * @brief Assemble point-vertex-compatible bytes from one or more data channels.
 *
 * channels[0] must supply position data (vec3 or uint8_t interleaved).
 * channels[1..5] optionally supply color, scalar, uv, normal, tangent in
 * canonical field order. Missing trailing channels are filled from config.
 * A single uint8_t channel of N*60 bytes is passed through zero-copy.
 *
 * @param channels Source channels in canonical field order.
 * @param config   Default attribute values for absent channels.
 * @return Populated VertexAccess, or std::nullopt on empty or incompatible input.
 */
[[nodiscard]] MAYAFLUX_API std::optional<VertexAccess>
as_point_vertex_access(std::span<const DataVariant> channels,
    const VertexAccessConfig& config = {});

/**
 * @brief Assemble line-vertex-compatible bytes from one or more data channels.
 *
 * channels[0] must supply position data (vec3 or uint8_t interleaved).
 * channels[1..5] optionally supply color, thickness, uv, normal, tangent in
 * canonical field order. Missing trailing channels are filled from config;
 * slot 2 defaults to config.default_thickness.
 * A single uint8_t channel of N*60 bytes is passed through zero-copy.
 *
 * @param channels Source channels in canonical field order.
 * @param config   Default attribute values for absent channels.
 * @return Populated VertexAccess, or std::nullopt on empty or incompatible input.
 */
[[nodiscard]] MAYAFLUX_API std::optional<VertexAccess>
as_line_vertex_access(std::span<const DataVariant> channels,
    const VertexAccessConfig& config = {});

/**
 * @brief Assemble mesh-vertex-compatible bytes from one or more data channels.
 *
 * channels[0] must supply position data (vec3 or uint8_t interleaved).
 * channels[1..5] optionally supply color, weight, uv, normal, tangent in
 * canonical field order. Missing trailing channels are filled from config;
 * slot 2 defaults to config.default_weight.
 * A single uint8_t channel of N*60 bytes is passed through zero-copy.
 *
 * @param channels Source channels in canonical field order.
 * @param config   Default attribute values for absent channels.
 * @return Populated VertexAccess, or std::nullopt on empty or incompatible input.
 */
[[nodiscard]] MAYAFLUX_API std::optional<VertexAccess>
as_mesh_vertex_access(std::span<const DataVariant> channels,
    const VertexAccessConfig& config = {});

} // namespace MayaFlux::Kakshya
