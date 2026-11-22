#pragma once

#include "NDData.hpp"

namespace MayaFlux::Portal::Graphics {
enum class PrimitiveTopology : uint8_t;
}

namespace MayaFlux::Kakshya {

/**
 * @enum SpatialScope
 * @brief How many elements one attribute contributes per sample.
 *
 * Mirrors Alembic's own GeometryScope, narrowed to the three values that
 * apply to points and curves (kVertexScope/kFacevaryingScope are mesh-only
 * concepts with no equivalent here):
 *   Constant: one value for the whole stream.
 *   Uniform: one value per curve. Meaningless for PrimitiveTopology::POINT_LIST
 *            (a point cloud has no sub-grouping smaller than the stream
 *            itself, so it is treated as Constant there).
 *   Varying: one value per point, or per curve vertex for curve topologies.
 */
enum class SpatialScope : uint8_t {
    Constant,
    Uniform,
    Varying,
};

/**
 * @struct SpatialAttribute
 * @brief One named, scoped channel of already-typed values.
 *
 * values holds one of Kakshya::DataVariant's float/glm::vec2/glm::vec3/
 * glm::vec4/uint32_t vectors, sized per SpatialScope against the sample it
 * is attached to (1 for Constant, curve count for Uniform, point/vertex
 * count for Varying). Any other DataVariant alternative is rejected.
 *
 * A vec3 named exactly "color" or "normal" is declared through Alembic's
 * dedicated color/normal GeomParam types instead of a plain vector one, so
 * an importer recognizes it as such rather than an arbitrary direction. A
 * vec4 "color" is written as an RGBA color. Every other vec4 is a float
 * GeomParam with array extent 4, and every other name stays generic.
 * MayaFlux's own vertex fields already use these names for these roles.
 */
struct SpatialAttribute {
    std::string name;
    SpatialScope scope;
    DataVariant values;
};

/**
 * @struct SpatialData
 * @brief One time shot of a spatial stream: points or curves with their named attributes, owning its data.
 *
 * topology decides which of the remaining fields apply:
 *   POINT_LIST: ids and velocities may be present, vertex_counts_per_curve is empty
 *               (every position is its own point).
 *   LINE_LIST / LINE_STRIP: vertex_counts_per_curve is present and sums to
 *               positions.size(), ids and velocities are empty.
 * Every attribute is sized by its scope against this frame, as SpatialAttribute says.
 *
 * Frames are shared and never changed once a FrameSource publishes one.
 */
struct SpatialData {
    Portal::Graphics::PrimitiveTopology topology {};
    std::vector<glm::vec3> positions;
    std::vector<int32_t> vertex_counts_per_curve;
    std::vector<uint64_t> ids;
    std::vector<glm::vec3> velocities;
    std::vector<SpatialAttribute> attributes;
};

} // namespace MayaFlux::Kakshya
