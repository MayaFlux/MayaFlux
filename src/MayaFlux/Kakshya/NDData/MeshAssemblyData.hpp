#pragma once

#include "Composite.hpp"
#include "ImageData.hpp"
#include "MeshData.hpp"
#include "MeshSkin.hpp"
#include "SpatialData.hpp"

#include "MayaFlux/Kinesis/Temporal/TimedValues.hpp"
#include "MayaFlux/Kinesis/Viewport/Lens.hpp"
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

/** @brief What kind of light source a file describes. */
enum class MeshAssemblyLightKind : uint8_t {
    Directional,
    Point,
    Spot,
    Ambient,
    Area,
    Unknown ///< The importer could not tell, as it does for some FBX area lights.
};

/**
 * @struct MeshAssemblyLight
 * @brief A light source as a file describes it. Its placement row gives position and direction:
 *        the light faces -Z with +Y up.
 *
 * attenuation holds the constant, linear and quadratic terms. The cone angles are radians as the
 * importer gives them (glTF's are measured from the axis) and apply to spot lights, size to area
 * lights. color already includes the file's intensity.
 */
struct MeshAssemblyLight {
    MeshAssemblyLightKind kind { MeshAssemblyLightKind::Point };
    glm::vec3 color { 1.0F };
    glm::vec3 specular { 1.0F };
    glm::vec3 ambient { 0.0F };
    glm::vec3 attenuation { 1.0F, 0.0F, 0.0F };
    float inner_cone {};
    float outer_cone {};
    glm::vec2 size { 0.0F };
};

/**
 * @struct MeshAssemblySource
 * @brief One thing a row can show, stored once however many placements use it: a mesh, points
 *        or curves, or a camera's lens.
 *
 * A camera is shown the way geometry is: a row whose source is a lens places the camera, so its
 * placement comes from the hierarchy and its motion from the row's channels, and the lens
 * itself (focal length, film back, clip planes) is the source.
 *
 * geometry is the first shot of the source, or its only one. time and variance
 * are what the file said about how the source changes; the frames themselves
 * are served by a FrameSource, not held here. facets carries anything else the
 * file attached to the source (user properties, bounds, extras), by typed key or
 * as whole structs. origin is where in the file the geometry came from, the name its
 * reader reads it by, empty for geometry made in code.
 */
struct MeshAssemblySource {
    std::variant<MeshData, SpatialData, Kinesis::Lens, MeshAssemblyLight> geometry;
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
 * @enum MeshAssemblyAlpha
 * @brief How a material means the alpha of its colour to be used.
 */
enum class MeshAssemblyAlpha : uint8_t {
    Opaque,
    Mask,
    Blend
};

/**
 * @struct MeshAssemblySurface
 * @brief The colour and factors a material names, kept in its facets by the readers that know them.
 *
 * base_color is the diffuse or base colour with its opacity in the alpha. alpha_cutoff applies
 * under MeshAssemblyAlpha::Mask. A reader that finds none of them leaves the facet out, and a
 * consumer reads it with facets.get<MeshAssemblySurface>().
 */
struct MeshAssemblySurface {
    glm::vec4 base_color { 1.0F };
    float metallic { 0.0F };
    float roughness { 1.0F };
    glm::vec3 emissive { 0.0F };
    float alpha_cutoff { 0.5F };
    MeshAssemblyAlpha alpha { MeshAssemblyAlpha::Opaque };
    bool double_sided {};
};

MF_DESCRIBE(MeshAssemblySurface)

/** @brief How a texture continues outside the unit square. */
enum class MeshAssemblyWrap : uint8_t {
    Repeat,
    Clamp,
    Mirror,
    Decal
};

/** @brief The offset, scale and rotation in radians a material applies to texture coordinates. */
struct MeshAssemblyUvTransform {
    glm::vec2 offset { 0.0F };
    glm::vec2 scale { 1.0F };
    float rotation {};
};

/**
 * @struct MeshAssemblyTexture
 * @brief One texture a material names.
 *
 * image holds the decoded pixels when the model file carries them, and path is then a reference
 * into that file. Otherwise image is null and path names a file.
 */
struct MeshAssemblyTexture {
    std::string path;
    std::shared_ptr<const ImageData> image;
    MeshAssemblyWrap wrap_u { MeshAssemblyWrap::Repeat };
    MeshAssemblyWrap wrap_v { MeshAssemblyWrap::Repeat };
    std::optional<MeshAssemblyUvTransform> transform;
    uint32_t uv_set {}; ///< 0 is the vertex uv, higher ones are MeshAssemblyVertexSets::uvs[uv_set - 1].
};

/** @brief A texture under a use MeshAssemblyMaps has no member for, or past the first of one it has. */
struct MeshAssemblyNamedTexture {
    std::string use;
    uint32_t index {};
    MeshAssemblyTexture texture;
};

/**
 * @struct MeshAssemblyMaps
 * @brief The textures of a material by use, kept in its facets by the readers that know them.
 *
 * A file that packs metalness and roughness into one texture names it in both.
 */
struct MeshAssemblyMaps {
    std::optional<MeshAssemblyTexture> base_color;
    std::optional<MeshAssemblyTexture> normal;
    std::optional<MeshAssemblyTexture> metalness;
    std::optional<MeshAssemblyTexture> roughness;
    std::optional<MeshAssemblyTexture> emissive;
    std::optional<MeshAssemblyTexture> occlusion;
    std::optional<MeshAssemblyTexture> opacity;
    std::vector<MeshAssemblyNamedTexture> others;
};

/**
 * @struct MeshAssemblyProperties
 * @brief Every property a file gives a material, as the importer named it, kept in the material's facets.
 */
struct MeshAssemblyProperties {
    using Value = std::variant<std::vector<float>, std::vector<double>, std::vector<int32_t>, std::string, std::vector<uint8_t>>;

    struct Entry {
        std::string key;
        uint32_t semantic {};
        uint32_t index {};
        Value value;
    };

    std::vector<Entry> entries;
};

/**
 * @struct MeshAssemblyVertexSets
 * @brief A mesh source's UV and colour sets past the first, one entry per vertex, kept in its facets.
 */
struct MeshAssemblyVertexSets {
    std::vector<std::vector<glm::vec2>> uvs;
    std::vector<std::vector<glm::vec4>> colors;
};

/**
 * @struct MeshAssemblyMorphTarget
 * @brief One shape a mesh can take: replacement values for its vertices, not offsets.
 *
 * Empty normals or tangents mean the target leaves them as they are. weight is the file's rest weight.
 */
struct MeshAssemblyMorphTarget {
    std::string name;
    float weight {};
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
};

/** @brief The morph targets of a mesh source, kept in its facets. */
struct MeshAssemblyMorphs {
    std::vector<MeshAssemblyMorphTarget> targets;
};

/** @brief The keyed weights of one row's morph targets, in the order of its source's targets. */
struct MeshAssemblyWeights {
    uint64_t row {};
    std::vector<Kinesis::TimedValues<float>> targets;
};

/**
 * @struct MeshAssemblyAttributes
 * @brief The named per element values of a geometry source that have no place in its
 *        vertices, kept in its facets.
 *
 * A Varying attribute has one value per vertex, a Uniform one per polygon and a
 * Constant one exactly one.
 */
struct MeshAssemblyAttributes {
    std::vector<SpatialAttribute> values;
};

/**
 * @struct MeshAssemblyPointMap
 * @brief The file point each vertex of a mesh source was split from, kept in its facets
 *        when seams or hard edges made more vertices than points.
 */
struct MeshAssemblyPointMap {
    std::vector<uint32_t> of_vertex;
    uint32_t point_count {};
};

/** @brief A named set of polygons of a mesh source, with the material it shows, if its own. */
struct MeshAssemblyFaceSet {
    std::string name;
    std::vector<uint32_t> faces;
    bool exclusive {};
    std::optional<uint64_t> material;
};

/** @brief The face sets of a mesh source, kept in its facets. */
struct MeshAssemblyFaceSets {
    std::vector<MeshAssemblyFaceSet> sets;
};

/**
 * @struct MeshAssemblySubdivision
 * @brief Marks a mesh source as the cage of a subdivision surface, kept in its facets.
 *
 * Crease and corner indices name points as MeshAssemblyPointMap numbers them, or
 * vertices when the source has no map. Holes name polygons.
 */
struct MeshAssemblySubdivision {
    std::string scheme { "catmull-clark" };
    std::vector<int32_t> crease_indices;
    std::vector<int32_t> crease_lengths;
    std::vector<float> crease_sharpness;
    std::vector<int32_t> corner_indices;
    std::vector<float> corner_sharpness;
    std::vector<int32_t> holes;
    int32_t face_varying_boundary {};
    int32_t face_varying_corners {};
    int32_t boundary {};
};

enum class MeshAssemblyCurveType : uint8_t {
    Cubic,
    Linear,
    VariableOrder
};

enum class MeshAssemblyCurveBasis : uint8_t {
    None,
    Bezier,
    Bspline,
    CatmullRom,
    Hermite,
    Power
};

/** @brief How the control points of a curve source are meant to be read, kept in its facets. */
struct MeshAssemblyCurveForm {
    MeshAssemblyCurveType type { MeshAssemblyCurveType::Linear };
    MeshAssemblyCurveBasis basis { MeshAssemblyCurveBasis::None };
    bool periodic {};
    std::vector<uint8_t> orders;
    std::vector<float> knots;
};

/**
 * @struct MeshAssemblyVisibility
 * @brief The rows a file shows or hides explicitly, kept in the assembly's facets.
 *
 * A row without an entry inherits from its parent. keys, when present, holds 1 for
 * shown and 0 for hidden over time, each held until the next.
 */
struct MeshAssemblyVisibility {
    struct Entry {
        uint64_t row {};
        bool visible { true };
        std::optional<Kinesis::TimedValues<float>> keys;
    };

    std::vector<Entry> entries;
};

/**
 * @struct MeshAssemblyMetadata
 * @brief The named values a file attaches to its scene and rows, uninterpreted, kept in the assembly's facets.
 *
 * An entry with no row belongs to the scene. Nested names are joined with '/'.
 */
struct MeshAssemblyMetadata {
    using Value = std::variant<bool, int64_t, uint64_t, double, glm::vec3, std::string>;

    struct Entry {
        std::optional<uint64_t> row;
        std::string key;
        Value value;
    };

    std::vector<Entry> entries;
};

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
    std::optional<Kinesis::OutOfRange> before;
    std::optional<Kinesis::OutOfRange> after;
};

/**
 * @struct MeshAssemblyMotion
 * @brief A named set of channels, each targeting a different placement row.
 */
struct MeshAssemblyMotion {
    std::string name;
    std::vector<MeshAssemblyChannel> channels;
    std::vector<MeshAssemblyWeights> weights {};
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
