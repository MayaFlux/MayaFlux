#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::Kakshya {
struct MeshAssemblyMaterial;
}

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {
class MeshBuffer;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
class PointCloudNetwork;
}

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::IO {

/**
 * @struct ModelInfo
 * @brief Element counts of a loaded model scene.
 *
 * Stored on FileMetadata by ModelReader.
 */
struct ModelInfo {
    uint64_t mesh_count = 0; ///< Number of meshes in the scene
    uint64_t material_count = 0; ///< Number of materials in the scene
    uint64_t animation_count = 0; ///< Number of animations in the scene
    uint64_t texture_count = 0; ///< Number of embedded textures in the scene
};

MF_DESCRIBE(ModelInfo)

/**
 * @brief Callable that maps a raw material texture path to a GPU image.
 *
 * Receives the path exactly as stored in the model file. The default
 * resolver resolves relative to the model file's directory via
 * ImageReader::load_texture.
 */
using TextureResolver = std::function<std::shared_ptr<Core::VKImage>(const std::string& path)>;

/**
 * @class ModelReader
 * @brief Assimp-backed loader for 3D model files.
 *
 * Produces vector<Kakshya::MeshData> — one entry per assimp mesh in the
 * imported scene. Each MeshData owns its vertex bytes (vector<uint8_t> in
 * the canonical 68-byte interleaved MeshVertex format) and index array
 * (vector<uint32_t>). Submesh identity is preserved in the single-entry case
 * via MeshData::submeshes (a RegionGroup named "submeshes" with one Region).
 * Multi-mesh files produce one MeshData per aiMesh, each with its own
 * single-submesh RegionGroup.
 *
 * Supported formats: glTF 2.0 (.glb, .gltf), OBJ, FBX, PLY, STL, DAE,
 * and any other format Assimp supports on the target platform.
 *
 * Post-processing applied unconditionally:
 * - aiProcess_Triangulate: all faces become triangles
 * - aiProcess_GenSmoothNormals: generate normals if absent
 * - aiProcess_CalcTangentSpace: tangent + bitangent from UVs
 * - aiProcess_FlipUVs: V-flip to match Vulkan UV convention
 * - aiProcess_JoinIdenticalVertices: deduplicate vertices
 * - aiProcess_SortByPType: isolate TRIANGLE primitives
 *
 * set_keep_polygons(true) before open() changes that. Faces of any size reach the
 * MeshData as its polygons (MeshData::faces) with the triangles derived from them,
 * instead of being triangulated by Assimp, and a mesh is no longer split by primitive
 * type, so triangles and polygons of one source mesh stay one MeshData. Faces of fewer
 * than three indices are left out. Normals and tangents the file does not carry are
 * derived from the polygons by Kinesis (write_vertex_normals and write_vertex_tangents,
 * which leave any the file does carry alone) in place of the two generation steps.
 *
 * FileReader contract:
 * create_container() and load_into_container() are no-ops for this reader;
 * mesh data does not go through the SignalSourceContainer streaming path.
 * Use load(), create_mesh_assembly() or the IOManager::load_mesh() convenience wrapper instead.
 */
class MAYAFLUX_API ModelReader : public FileReader {
public:
    ModelReader();
    ~ModelReader() override;

    /**
     * @brief Keep polygons as they are in the file instead of triangulating them.
     *
     * Applies to the next open(). Off by default. See the class description for what
     * changes.
     */
    void set_keep_polygons(bool keep) noexcept { m_keep_polygons = keep; }

    /**
     * @brief Whether the next open() bakes the node tree into the vertices.
     *
     * Applies to the next open(). Off by default. On, Assimp (aiProcess_PreTransformVertices)
     * places every mesh where its node put it and leaves one root, so the scene has no
     * hierarchy to animate, and it drops the scene's animations, bones and morph targets, which
     * open() warns about when the file has any. Use it for a static
     * scene that should be one flat set of placed meshes.
     */
    void set_flatten(bool flatten) noexcept { m_flatten = flatten; }

    // -------------------------------------------------------------------------
    // Primary API — use these
    // -------------------------------------------------------------------------

    /**
     * @brief Load all meshes from a file in one call.
     *
     * Opens, imports, extracts, and closes in a single synchronous operation.
     * Returns an empty vector on failure; check get_last_error().
     *
     * @param filepath Path to the model file.
     * @return One MeshData per aiMesh in the scene, in scene order.
     */
    [[nodiscard]] std::vector<Kakshya::MeshData> load(const std::string& filepath);

    /**
     * @brief Load all meshes after open() has already been called.
     *
     * Extracts from the currently imported scene. Returns empty if no scene
     * is loaded. Does not call close().
     *
     * @return One MeshData per aiMesh in the scene.
     */
    [[nodiscard]] std::vector<Kakshya::MeshData> extract_meshes() const;

    /**
     * @brief The texture of a material that is cut out by an opacity map: its base colour with
     *        the map as alpha, which the textured mesh shaders discard below one half.
     * @param material A material of an assembly this reader's kind of file produced.
     * @param model_path The model file, whose directory the texture paths are relative to.
     * @return The uploaded image, or null when the material has no base colour, no opacity map of
     *         its own, or either cannot be read as 8 bit pixels. The caller holds it as any other
     *         texture from a resolver.
     *
     * A map the file connected as transparency (FBX) is read inverted.
     */
    [[nodiscard]] static std::shared_ptr<Core::VKImage> load_cutout_texture(
        const Kakshya::MeshAssemblyMaterial& material, const std::string& model_path);

    /**
     * @brief Construct one MeshBuffer per placed mesh of the currently loaded scene.
     *
     * Built from create_mesh_assembly(): each buffer's vertices are where its row places
     * them at rest, and a mesh several rows use gets a buffer for each. A skinned row's buffer
     * keeps its vertices as bound and is given the skin with the palette of the joints at
     * rest in the scene's space, so MeshBuffer::set_palette moves it. Returns empty if
     * no scene is loaded.
     * setup_processors() and setup_rendering() are left to the caller.
     *
     * If resolver is provided, it is called with the raw diffuse path from each
     * mesh's material. The returned VKImage is bound via bind_diffuse_texture().
     * A texture the file carries itself is decoded from the file and does not reach
     * the resolver. Null resolver skips texture binding entirely.
     *
     * @param resolver Optional texture resolver.
     * @return One MeshBuffer per mesh row, in row order.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::MeshBuffer>>
    create_mesh_buffers(const TextureResolver& resolver = nullptr) const;

    /**
     * @brief Construct a MeshNetwork from the scene's node tree.
     *
     * One slot per node, named from the node, parented as the tree is, with the
     * node's transform as local_transform and the composed world_transform
     * already resolved. A node with no mesh becomes a joint slot with no node.
     * The first mesh of a node lives on the node's own slot; each further mesh
     * becomes a child slot named from the mesh. A mesh referenced by several
     * nodes gets its own copy of the geometry in each slot.
     *
     * An identity scene root gets no slot: its meshes and children become root
     * slots, so a single-node file loads as one root slot per mesh. A scene
     * root with a transform is kept as a joint. Meshes no node references are
     * added as root slots, named from aiMesh::mName (or "mesh_N" if unnamed).
     * Each slot's MeshWriterNode holds the extracted vertex and index data.
     *
     * The network holds the tree's rows of create_mesh_assembly(), which come first there, so
     * slot i is row i of that assembly and
     * one of its motions plays on this network through Kriya::slot_sink. An identity scene root
     * that an animation targets is kept as a joint.
     *
     * If resolver is provided, it is called with the raw diffuse path from each
     * mesh's material. The returned VKImage is assigned to slot.diffuse_texture.
     * A texture the file carries itself is decoded from the file and does not reach
     * the resolver. Null resolver skips texture binding entirely.
     *
     * Requires open(). Does not call close().
     *
     * @param resolver Optional texture resolver.
     * @return Populated MeshNetwork, or nullptr if no scene is loaded.
     */
    [[nodiscard]] std::shared_ptr<Nodes::Network::MeshNetwork>
    create_mesh_network(const TextureResolver& resolver = nullptr) const;

    /**
     * @brief The scene's points and lines as point cloud networks.
     *
     * At most two: one holding every point, one holding every line, in that order, leaving
     * out a kind the scene does not have. See Nodes::Network::make_point_cloud_network.
     * Requires open(). Does not call close().
     */
    [[nodiscard]] std::vector<std::shared_ptr<Nodes::Network::PointCloudNetwork>>
    create_point_cloud_networks() const;

    /**
     * @brief Build a MeshAssemblyContainer from the scene.
     *
     * Every valid mesh becomes one source, stored once, so a mesh several nodes use
     * stays shared. Every material becomes a MeshAssemblyMaterial (name, diffuse path,
     * whether the path points into the file), with its textures as MeshAssemblyMaps and
     * its colours and factors as MeshAssemblySurface in its facets. The node tree becomes the placement rows
     * with the same rules as create_mesh_network(): one row per node, parented as the
     * tree is, a node with no mesh a joint, the first mesh of a node on the node's own
     * row and each further mesh a child row, an identity scene root with no row unless
     * an animation targets it, and meshes no node references as root rows.
     *
     * A camera becomes a Kakshya::MeshAssemblyLens source, perspective or orthographic, and a light a
     * MeshAssemblyLight source. Either sits on the row of its name when that row shows
     * nothing else and it is at the row's origin facing -Z, otherwise on a child row carrying its
     * own frame. Both look along -Z with +Y up, as Kinesis::view_from_frustum expects. Every
     * material property is in the material's MeshAssemblyProperties. Where several rows
     * share a name, a channel or bone naming it takes the first in tree order.
     *
     * A mesh of only points or only lines becomes a SpatialData source with color, uv and
     * normal attributes. UV and colour sets past the first are the source's
     * MeshAssemblyVertexSets, and its morph targets its MeshMorphTargets. Scene and row
     * metadata is the assembly's MeshAssemblyMetadata, uninterpreted: no unit or axis
     * conversion is applied.
     *
     * The tree's rows come first. After them come the rows that only a mesh network cannot
     * hold: the points and lines of a mesh beyond what its row shows, and camera and light
     * children. create_point_cloud_networks() takes the
     * points and lines, and MeshAssemblyRealization gives each kind its own target.
     *
     * Each Assimp animation becomes a motion. Morph weights are the motion's weights, linear
     * between keys, and a channel's behaviour before and after its keys is kept on it for
     * the sink to honour or not. A channel is resolved by name, to the
     * first row in tree order when several share it; a name that matches no row and a
     * row an animation keys twice are skipped with a warning. Times are seconds: key time over
     * mTicksPerSecond, with 0 ticks per second read as 1. A key's interpolation is Step
     * or Linear (rotations spherical), or CubicSpline with the tangents of its key: the glTF
     * importer of Assimp 6.0.6 and later hands over a cubic sampler as three keys at one time,
     * the incoming tangent, the value and the outgoing tangent, and they are read as one key.
     * Older Assimp keeps only the value, so such keys arrive as Linear. The scene's ModelInfo is
     * kept in the assembly's facets.
     *
     * Requires open(). Does not call close(). Textures are not resolved: the assembly
     * holds the paths.
     *
     * @return The assembly, or nullptr if no scene is loaded.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::MeshAssemblyContainer> create_mesh_assembly() const;

    // -------------------------------------------------------------------------
    // FileReader interface
    // -------------------------------------------------------------------------

    [[nodiscard]] bool can_read(const std::string& filepath) const override;

    bool open(const std::string& filepath,
        FileReadOptions options = FileReadOptions::ALL) override;

    void close() override;

    [[nodiscard]] bool is_open() const override { return m_is_open; }

    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;

    [[nodiscard]] std::vector<FileRegion> get_regions() const override;

    std::vector<Kakshya::DataVariant> read_all() override;

    std::vector<Kakshya::DataVariant> read_region(const FileRegion& region) override;

    /**
     * @brief No-op. Mesh data does not use SignalSourceContainer.
     * @return nullptr always.
     */
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;

    /**
     * @brief No-op. Mesh data does not use SignalSourceContainer.
     * @return false always.
     */
    bool load_into_container(
        std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    [[nodiscard]] std::vector<uint64_t> get_read_position() const override;

    bool seek(const std::vector<uint64_t>& position) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;

    [[nodiscard]] std::type_index get_data_type() const override
    {
        return typeid(std::vector<uint8_t>);
    }

    [[nodiscard]] std::type_index get_container_type() const override
    {
        return typeid(void);
    }

    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    [[nodiscard]] bool supports_streaming() const override { return false; }
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 0; }
    [[nodiscard]] size_t get_num_dimensions() const override { return 0; }
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override { return {}; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_is_open { false };
    bool m_keep_polygons { false };
    bool m_flatten { false };
    bool m_polygons_imported { false };
    mutable std::string m_last_error;

    /** @brief create_mesh_assembly, with or without the rows that follow the tree's own. */
    [[nodiscard]] std::shared_ptr<Kakshya::MeshAssemblyContainer> assemble(bool beside_the_tree) const;

    [[nodiscard]] Kakshya::MeshData extract_single_mesh(
        const void* ai_mesh,
        const void* ai_scene,
        std::string_view mesh_name,
        std::string_view material_name) const;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
#include "ModelReader.describe.hpp"
