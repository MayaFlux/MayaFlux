#pragma once

#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct VertexAccess
 * @brief Memory-compatible view of a DataVariant for vertex buffer upload.
 *
 * Mirrors TextureAccess: describes what the data IS in memory terms —
 * pointer, byte count, and a VertexLayout that faithfully represents
 * stride and attribute semantics — with no knowledge of Vulkan, Portal,
 * or concrete vertex structs (PointVertex, LineVertex).
 *
 * Zero-copy cases (data_ptr points directly into variant storage):
 *   vector<glm::vec3>   position-only vertices, stride = 12
 *   vector<glm::vec2>   2D position vertices,   stride = 8
 *   vector<glm::vec4>   position + W attribute, stride = 16
 *
 * Conversion cases (conversion_buffer holds promoted data):
 *   vector<float>            Y-displacement waveform: x = normalised index,
 *                            y = value,  z = 0. Output: vec3, stride = 12.
 *   vector<double>           Same as float, narrowed to float in output.
 *   vector<uint8_t>          Values normalised to [-1, 1] as Y displacement.
 *   vector<uint16_t>         Same normalisation.
 *   vector<uint32_t>         Same normalisation.
 *   vector<complex<float>>   magnitude as Y, phase normalised to [0,1] as Z.
 *                            Output: vec3, stride = 12.
 *
 * Rejected types (as_vertex_access returns std::nullopt, logs error):
 *   vector<complex<double>>  No standard waveform interpretation; caller must
 *                            convert to float complex or extract components.
 *   vector<glm::mat4>        Ambiguous layout; caller must unpack to vec4 first.
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
as_vertex_access(const DataVariant& variant);

} // namespace MayaFlux::Kakshya
