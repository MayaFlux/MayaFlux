#pragma once

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::IO {

/**
 * @struct ModelWriteOptions
 * @brief Configuration for model writing.
 *
 * Format-specific knobs are interpreted by the concrete writer; unsupported
 * options are silently ignored.
 */
struct ModelWriteOptions {
    /**
     * @brief Assimp export format id, e.g. "gltf2", "objnomtl", "collada",
     *        "stl", "fbx", "fbxa".
     *
     * File extension alone is ambiguous for several formats Assimp exports:
     * OBJ with or without a sidecar .mtl, FBX binary vs. ASCII both use
     * ".fbx". Empty (the default) derives a format id from the extension
     * via a small, deliberately short allow-list; see AssimpModelWriter's
     * own doc for exactly which extensions resolve on their own and which
     * require this to be set explicitly.
     */
    std::string format_id;

    /**
     * @brief Name to embed for the exported material, if the target format
     *        carries one. Empty uses the source submesh's material_name.
     */
    std::string material_name_override;
};

/**
 * @class ModelWriter
 * @brief Abstract base for 3D model format writers.
 *
 * Parallels ImageWriter/VolumeWriter. A writer accepts one or more
 * Kakshya::MeshData (one per aiMesh in the exported scene, mirroring how
 * ModelReader produces one MeshData per aiMesh on import) and is
 * responsible for validating that the target format can express what it's
 * given: a format with no non-triangle primitive support rejects point/line
 * data rather than silently dropping it. See AssimpModelWriter's own doc
 * for the concrete list of formats verified safe for which topology.
 *
 * Writers are single-shot: one call to write() produces one file. MeshData
 * is always CPU-authoritative in this codebase (see MeshBuffer's mutation
 * path), so no GPU download step belongs here. A caller with GPU-only
 * mesh state (ComputeMeshBuffer) downloads to MeshData first via
 * IO::download_compute_mesh, then calls write() like any other source.
 */
class MAYAFLUX_API ModelWriter {
public:
    virtual ~ModelWriter() = default;

    /**
     * @brief Check whether this writer handles the given filepath.
     */
    [[nodiscard]] virtual bool can_write(const std::string& filepath) const = 0;

    /**
     * @brief Write one or more meshes to disk as a single scene.
     * @param filepath Destination path.
     * @param meshes   One or more meshes. Must each satisfy MeshData::is_valid().
     * @param options  Format-specific options.
     * @return true on success. On failure call get_last_error().
     */
    virtual bool write(
        const std::string& filepath,
        const std::vector<Kakshya::MeshData>& meshes,
        const ModelWriteOptions& options = {})
        = 0;

    /**
     * @brief File extensions handled by this writer (without dot).
     */
    [[nodiscard]] virtual std::vector<std::string> get_supported_extensions() const = 0;

    /**
     * @brief Last error message or empty string.
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;
};

using ModelWriterFactory = std::function<std::unique_ptr<ModelWriter>()>;

/**
 * @class ModelWriterRegistry
 * @brief Singleton registry dispatching model writes by file extension.
 *
 * Mirrors ImageWriterRegistry/VolumeWriterRegistry. Concrete writers
 * register themselves during subsystem init. create_writer(path) looks up
 * the extension and returns a fresh instance, or nullptr if none is
 * registered.
 */
class MAYAFLUX_API ModelWriterRegistry {
public:
    static ModelWriterRegistry& instance()
    {
        static ModelWriterRegistry registry;
        return registry;
    }

    void register_writer(
        const std::vector<std::string>& extensions,
        const ModelWriterFactory& factory)
    {
        for (const auto& ext : extensions) {
            m_factories[ext] = factory;
        }
    }

    [[nodiscard]] std::unique_ptr<ModelWriter> create_writer(const std::string& filepath) const
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }

        auto it = m_factories.find(ext);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> get_registered_extensions() const
    {
        std::vector<std::string> exts;
        exts.reserve(m_factories.size());
        for (const auto& [ext, _] : m_factories) {
            exts.push_back(ext);
        }
        return exts;
    }

private:
    std::unordered_map<std::string, ModelWriterFactory> m_factories;
};

} // namespace MayaFlux::IO
