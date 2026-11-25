#pragma once

#include "ModelWriter.hpp"

namespace MayaFlux::IO {

/**
 * @class AssimpModelWriter
 * @brief ModelWriter implementation backed by Assimp::Exporter.
 *
 * Packs one or more Kakshya::MeshData into an aiScene (one aiMesh per
 * MeshData, one aiMaterial per aiMesh sourced from the MeshData's submesh
 * Region: name, material_name, diffuse_path) and calls Assimp::Exporter.
 *
 * Vertex bytes must be in the canonical 68-byte MeshVertex layout
 * (MeshData::layout.stride_bytes == sizeof(Kakshya::MeshVertex)), the same
 * assumption ModelReader/MeshInsertion make on the way in. A MeshData with
 * any other stride is rejected rather than misinterpreted.
 *
 * Mesh-only: this writer targets face geometry exclusively. It does not
 * attempt point/line export. Assimp's own topology support for those is
 * format-dependent and, for two formats (STL, Collada), silently drops the
 * geometry entirely rather than erroring. That case is out of scope here by
 * design. A MeshData that keeps its polygons (MeshData::faces) is written as those
 * polygons, and any other MeshData as its triangles.
 *
 * format_id resolution (ModelWriteOptions::format_id):
 * Assimp dispatches by an internal format id string, not by extension;
 * several extensions are genuinely ambiguous (.obj with or without a
 * sidecar .mtl; .fbx binary vs. ascii both use the same extension). When
 * format_id is empty, this table supplies a default:
 *
 *   .dae  -> "collada"
 *   .obj  -> "obj"      (with .mtl; pass "objnomtl" explicitly to omit it)
 *   .stl  -> "stlb"      (binary; pass "stl" explicitly for ascii)
 *   .ply  -> "ply"        (ascii; pass "plyb" explicitly for binary)
 *   .3ds  -> "3ds"
 *   .gltf -> "gltf2"      (not the legacy v1 "gltf" format id)
 *   .glb  -> "glb2"
 *   .fbx  -> "fbx"        (binary; pass "fbxa" explicitly for ascii)
 *   .x3d  -> "x3d"
 *   .json -> "assjson"    (Assimp's own scene dump, round-trips through
 *                          Assimp faithfully; not a general DCC target)
 *
 * Textures: a path is written as the material has it. A texture an assembly holds as pixels is
 * embedded for gltf and glb and written as a PNG beside the file otherwise.
 *
 * Texture coordinates are flipped back on the way out, so a model read, written and read again
 * keeps them.
 *
 * What survives is what Assimp's exporter for the format writes. With 6.0.6, gltf keeps the
 * hierarchy, shared meshes, materials, skins, node animation and morph targets, and drops
 * cameras, lights and morph animation. obj, ply and stl keep geometry. Rows sharing a name are
 * given distinct ones. Step and cubic spline keys are written as linear.
 */
class MAYAFLUX_API AssimpModelWriter : public ModelWriter {
public:
    AssimpModelWriter() = default;
    ~AssimpModelWriter() override = default;

    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    bool write(
        const std::string& filepath,
        const std::vector<Kakshya::MeshData>& meshes,
        const ModelWriteOptions& options = {}) override;

    bool write(
        const std::string& filepath,
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        const ModelWriteOptions& options = {}) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    /**
     * @brief Register this writer with the ModelWriterRegistry.
     *
     * Called from engine/subsystem init. Idempotent.
     */
    static void register_with_registry();

private:
    mutable std::string m_last_error;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
