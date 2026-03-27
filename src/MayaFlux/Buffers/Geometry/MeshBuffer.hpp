#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/MeshAccess.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

class MeshProcessor;
class RenderProcessor;

/**
 * @class MeshBuffer
 * @brief VKBuffer subclass that owns a MeshData and manages its GPU upload.
 *
 * MeshBuffer is to indexed triangle geometry what TextureBuffer is to pixel
 * data: it owns the CPU-side storage, creates a dedicated processor for
 * GPU upload, and exposes setup_rendering() to attach a RenderProcessor.
 *
 * The VKBuffer base manages the vertex Vulkan buffer (Usage::VERTEX).
 * MeshProcessor allocates and owns a separate Usage::INDEX VKBuffer and
 * links both into VKBufferResources via set_index_resources(), making the
 * indexed draw path available to RenderProcessor transparently.
 *
 * Two independent dirty flags govern re-upload:
 * - m_vertices_dirty: set when vertex bytes change (deformation, morph, etc.)
 * - m_indices_dirty: set when connectivity changes (topology mutation)
 *
 * Typical usage:
 * @code
 * auto buf = std::make_shared<MeshBuffer>(mesh_data);
 * buf->setup_rendering({ .target_window = window });
 * // Register with graphics subsystem — MeshProcessor uploads on first cycle.
 * @endcode
 *
 * Deformation:
 * @code
 * buf->set_vertex_data(new_bytes);  // marks vertices dirty
 * // MeshProcessor re-uploads vertex buffer on next graphics cycle.
 * @endcode
 */
class MAYAFLUX_API MeshBuffer : public VKBuffer {
public:
    /**
     * @brief Construct from an owning MeshData.
     *
     * The VKBuffer base is sized to vertex_bytes. MeshProcessor allocates
     * the index buffer separately on on_attach.
     *
     * @param data Loaded or generated mesh. Must be valid (is_valid() == true).
     */
    explicit MeshBuffer(Kakshya::MeshData data);

    ~MeshBuffer() override = default;

    /**
     * @brief Create and attach MeshProcessor as the default processor.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor targeting the given window.
     *
     * Always uses triangle.vert.spv / triangle.frag.spv unless overridden.
     * Topology is always TRIANGLE_LIST.
     */
    void setup_rendering(const RenderConfig& config);

    // -------------------------------------------------------------------------
    // CPU-side data access
    // -------------------------------------------------------------------------

    /**
     * @brief Produce a non-owning MeshAccess view over the internal MeshData.
     */
    [[nodiscard]] std::optional<Kakshya::MeshAccess> access() const
    {
        return m_mesh_data.access();
    }

    [[nodiscard]] uint32_t get_vertex_count() const noexcept
    {
        return m_mesh_data.vertex_count();
    }

    [[nodiscard]] uint32_t get_index_count() const noexcept
    {
        const auto* ib = std::get_if<std::vector<uint32_t>>(
            &m_mesh_data.index_variant);
        return ib ? static_cast<uint32_t>(ib->size()) : 0;
    }

    [[nodiscard]] uint32_t get_face_count() const noexcept
    {
        return m_mesh_data.face_count();
    }

    [[nodiscard]] const Kakshya::VertexLayout& get_vertex_layout_ref() const noexcept
    {
        return m_mesh_data.layout;
    }

    [[nodiscard]] const std::optional<Kakshya::RegionGroup>& get_submeshes() const noexcept
    {
        return m_mesh_data.submeshes;
    }

    // -------------------------------------------------------------------------
    // Mutation (deformation path)
    // -------------------------------------------------------------------------

    /**
     * @brief Replace vertex bytes entirely and mark vertices dirty.
     *
     * Intended for deformation: caller writes new interleaved bytes (same
     * stride as the original layout) and the next graphics cycle re-uploads.
     * Does not change index data or layout.
     *
     * @param bytes Raw interleaved vertex bytes. Size must be a multiple of
     *              m_mesh_data.layout.stride_bytes.
     */
    void set_vertex_data(std::span<const uint8_t> bytes);

    /**
     * @brief Replace index data entirely and mark indices dirty.
     *
     * For topology mutation. Size must be a multiple of 3.
     *
     * @param indices New triangle index list.
     */
    void set_index_data(std::span<const uint32_t> indices);

    /**
     * @brief Replace both vertex and index data atomically and mark both dirty.
     */
    void set_mesh_data(Kakshya::MeshData data);

    // -------------------------------------------------------------------------
    // Dirty flag queries (read by MeshProcessor)
    // -------------------------------------------------------------------------

    [[nodiscard]] bool vertices_dirty() const noexcept
    {
        return m_vertices_dirty.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool indices_dirty() const noexcept
    {
        return m_indices_dirty.load(std::memory_order_acquire);
    }

    void clear_vertices_dirty() noexcept
    {
        m_vertices_dirty.store(false, std::memory_order_release);
    }

    void clear_indices_dirty() noexcept
    {
        m_indices_dirty.store(false, std::memory_order_release);
    }

    // -------------------------------------------------------------------------
    // Processor / renderer access
    // -------------------------------------------------------------------------

    [[nodiscard]] std::shared_ptr<MeshProcessor> get_mesh_processor() const
    {
        return m_mesh_processor;
    }

    [[nodiscard]] std::shared_ptr<RenderProcessor> get_render_processor() const override
    {
        return m_render_processor;
    }

    /**
     * @brief Bind a diffuse texture to the render processor, if present.
     *
     * The binding name must match a descriptor slot in the shader. If the
     * render processor is not yet created, the texture will be bound on
     * creation if the default_texture_binding matches.
     */
    void bind_diffuse_texture(
        std::shared_ptr<Core::VKImage> image,
        std::string_view binding_name = "diffuseTex");

    /* Check if a diffuse texture is bound. */
    [[nodiscard]] bool has_diffuse_texture() const noexcept { return m_diffuse_texture != nullptr; }

    /* Diffuse texture is optional, so may return nullptr. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> get_diffuse_texture() const noexcept { return m_diffuse_texture; }

private:
    friend class MeshProcessor;

    Kakshya::MeshData m_mesh_data;
    std::shared_ptr<Core::VKImage> m_diffuse_texture;
    std::string m_diffuse_binding { "diffuseTex" };

    std::atomic<bool> m_vertices_dirty { true };
    std::atomic<bool> m_indices_dirty { true };

    std::shared_ptr<MeshProcessor> m_mesh_processor;
    std::shared_ptr<RenderProcessor> m_render_processor;
};

} // namespace MayaFlux::Buffers
