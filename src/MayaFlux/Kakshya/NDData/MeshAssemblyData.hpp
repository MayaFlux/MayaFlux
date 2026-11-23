#pragma once

#include "Composite.hpp"
#include "MeshData.hpp"
#include "SpatialData.hpp"

#include "MayaFlux/Kinesis/Temporal/TimedValues.hpp"
#include "MayaFlux/Transitive/Facet/Facets.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum TopologyVariance
 * @brief How a geometry source changes over time, as the file declared it.
 *
 * Constant never changes. Homogeneous keeps its connectivity and only the
 * positions and attributes change, so indices need be sent once. Heterogeneous
 * may change anything from one time to the next.
 */
enum class TopologyVariance : uint8_t {
    Constant,
    Homogeneous,
    Heterogeneous
};

/**
 * @struct MeshAssemblySource
 * @brief One piece of geometry, stored once however many placements use it.
 *
 * geometry is the first shot of the source, or its only one. time and variance
 * are what the file said about how the source changes; the frames themselves
 * are served by a FrameSource, not held here. facets carries anything else the
 * file attached to the source (user properties, bounds, extras), by typed key or
 * as whole structs. origin is where in the file the geometry came from, the name its
 * reader reads it by, empty for geometry made in code.
 */
struct MeshAssemblySource {
    std::variant<MeshData, SpatialData> geometry;
    std::string name;
    std::string origin;
    std::optional<Kinesis::TimeSampling> time;
    TopologyVariance variance { TopologyVariance::Constant };
    Facets facets;
};

MF_DESCRIBE(MeshAssemblySource)

/**
 * @struct MeshAssemblyMaterial
 * @brief The surface a file names for a placement, kept as the file wrote it.
 *
 * diffuse_embedded says diffuse_path is not a file path but a reference into
 * the model file itself. facets carries the rest of what the file says about the
 * surface (colours, factors, further textures), by typed key or as whole structs.
 */
struct MeshAssemblyMaterial {
    std::string name;
    std::string diffuse_path;
    bool diffuse_embedded {};
    Facets facets;
};

MF_DESCRIBE(MeshAssemblyMaterial)

/**
 * @struct MeshAssemblySurface
 * @brief The colour and factors a material names, kept in its facets by the readers that know them.
 *
 * base_color is the diffuse or base colour with its opacity in the alpha. A reader that finds none of
 * them leaves the facet out, and a consumer reads it with facets.get<MeshAssemblySurface>().
 */
struct MeshAssemblySurface {
    glm::vec4 base_color { 1.0F };
    float metallic { 0.0F };
    float roughness { 1.0F };
};

MF_DESCRIBE(MeshAssemblySurface)

/**
 * @struct MeshAssemblyChannel
 * @brief Keyed motion of one placement row. A missing component leaves that part of the
 *        row's rest transform as it is.
 *
 * rotation is a unit quaternion in x, y, z, w order (Kinesis::ValueKind::Rotation).
 */
struct MeshAssemblyChannel {
    uint64_t row {};
    std::optional<Kinesis::TimedValues<glm::vec3>> translation;
    std::optional<Kinesis::TimedValues<glm::vec4>> rotation;
    std::optional<Kinesis::TimedValues<glm::vec3>> scale;
};

/**
 * @struct MeshAssemblyMotion
 * @brief A named set of channels, each targeting a different placement row.
 */
struct MeshAssemblyMotion {
    std::string name;
    std::vector<MeshAssemblyChannel> channels;
};

MF_DESCRIBE(MeshAssemblyMotion)

/**
 * @struct MeshAssemblyGroup
 * @brief Placement rows that show the same source with the same material.
 */
struct MeshAssemblyGroup {
    uint64_t source {};
    std::optional<uint64_t> material;
    std::vector<uint64_t> rows;
};

MF_DESCRIBE(MeshAssemblyGroup)

/**
 * @struct MeshAssemblyPlacement
 * @brief One row of the placement table: a place something is shown, or a joint.
 *
 * The table's layout is derived from this description, so a column is named by the
 * member that declares it. parent is the row of the parent and is always an earlier
 * row, absent for a root. source indexes MeshAssemblyData::sources and is absent for a
 * joint. material indexes MeshAssemblyData::materials and is absent for none. local is
 * the placement relative to the parent. Two rows may name the same source, which is
 * how a shared mesh is kept shared.
 */
struct MeshAssemblyPlacement {
    std::string name;
    std::optional<uint64_t> parent;
    std::optional<uint64_t> source;
    std::optional<uint64_t> material;
    glm::mat4 local { 1.0F };
};

MF_DESCRIBE(MeshAssemblyPlacement)

/**
 * @struct MeshAssemblyData
 * @brief Geometry sources placed in a hierarchy: what a model file or an
 *        Alembic archive holds, as one passive value.
 *
 * Sources and materials are stored once. The placement table has one row per
 * MeshAssemblyPlacement. Motions move rows by keys over time. facets holds what
 * describes the whole file (its path, application, units, up axis), by typed key
 * or as whole structs, so no such fact is a member.
 *
 * MeshAssemblyContainer holds one and keeps these references valid.
 */
struct MAYAFLUX_API MeshAssemblyData {
    /**
     * @brief The layout every placement table uses, derived from MeshAssemblyPlacement.
     */
    [[nodiscard]] static CompositeLayout placement_layout();

    /**
     * @brief Name of the placement table's parent column, taken from MeshAssemblyPlacement.
     */
    [[nodiscard]] static std::string_view parent_column();

    std::vector<MeshAssemblySource> sources;
    std::vector<MeshAssemblyMaterial> materials;
    CompositeArray placements { placement_layout() };
    std::vector<MeshAssemblyMotion> motions;
    Facets facets;
};

}
#include "MeshAssemblyData.describe.hpp"
