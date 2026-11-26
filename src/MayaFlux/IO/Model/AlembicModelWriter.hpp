#pragma once

#include "ModelWriter.hpp"

namespace MayaFlux::IO {

/**
 * @class AlembicModelWriter
 * @brief Writes meshes or a whole mesh assembly as an Alembic (Ogawa) archive.
 *
 * An assembly keeps its hierarchy. A row that only shows something, with an identity
 * transform and no motion, becomes that object itself; any other row becomes a transform,
 * or a plain group, holding it. Rows sharing a source and material become instances.
 *
 * - Meshes become PolyMesh objects, or SubD ones where the source has
 *   MeshAssemblySubdivision, with their polygons, normals, uvs, colours, further vertex
 *   sets, MeshAssemblyAttributes and face sets.
 * - Points and curves keep their widths, ids, velocities, attributes and curve form.
 * - Lenses become cameras and MeshAssemblyLight sources become lights.
 * - Attached mesh, spatial and lens frames are written sample by sample at their times.
 * - The first motion's channels become transform samples at their own key times.
 * - Materials, their textures and properties, user metadata and visibility are kept.
 *
 * Texture coordinates of meshes and curves are turned back to Alembic's upward v.
 *
 * Textures held as pixels are written as PNG files beside the archive, which cannot hold
 * them. Skins, morph targets and further motions have no place in Alembic and are skipped
 * with a warning. ModelWriteOptions::format_id is ignored.
 */
class MAYAFLUX_API AlembicModelWriter : public ModelWriter {
public:
    AlembicModelWriter() = default;
    ~AlembicModelWriter() override = default;

    [[nodiscard]] bool can_write(const std::string& filepath) const override;

    bool write(
        const std::string& filepath,
        const std::vector<Kakshya::MeshData>& meshes,
        const ModelWriteOptions& options = {}) override;

    bool write(
        const std::string& filepath,
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        const ModelWriteOptions& options = {}) override;

    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { "abc" }; }
    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    static void register_with_registry();

private:
    std::string m_last_error;
};

} // namespace MayaFlux::IO
