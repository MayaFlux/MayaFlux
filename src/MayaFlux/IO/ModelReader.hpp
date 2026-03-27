#pragma once

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::IO {

/**
 * @class ModelReader
 * @brief Assimp-backed loader for 3D model files.
 *
 * Produces vector<Kakshya::MeshData> — one entry per assimp mesh in the
 * imported scene. Each MeshData owns its vertex bytes (vector<uint8_t> in
 * the canonical 60-byte interleaved MeshVertex format) and index array
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
 * FileReader contract:
 * create_container() and load_into_container() are no-ops for this reader;
 * mesh data does not go through the SignalSourceContainer streaming path.
 * Use load() or the IOManager::load_mesh() convenience wrapper instead.
 */
class MAYAFLUX_API ModelReader : public FileReader {
public:
    ModelReader();
    ~ModelReader() override;

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
    mutable std::string m_last_error;

    [[nodiscard]] Kakshya::MeshData extract_single_mesh(
        const void* ai_mesh,
        std::string_view mesh_name,
        std::string_view material_name) const;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
