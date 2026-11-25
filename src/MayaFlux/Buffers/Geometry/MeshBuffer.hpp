#pragma once

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/MeshAccess.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/MeshSkin.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Nodes::GpuSync {
class MeshWriterNode;
}

namespace MayaFlux::Nodes::Network {
class GpuFieldOperator;
}

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

class MeshProcessor;
class RenderProcessor;
class SkinProcessor;

/**
 * @class MeshBuffer
 * @brief StateFieldBuffer subclass that owns a MeshData and manages its GPU upload.
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
 *
 * Replay: write_frame hands the buffer a shared MeshData from any thread, such as a
 * Reflow sink. The newest frame replaces the geometry on the next graphics cycle, and
 * the indices are uploaded only when they differ from the ones already on the GPU. The
 * buffer may start with no geometry (MeshData::empty()) and draws nothing until the
 * first frame, and a frame with no triangles hides it. The set_* mutators below are for
 * the thread that owns the buffer; write_frame is the any-thread path.
 *
 * Field pass: with a GpuFieldOperator set before setup_rendering, the field deforms the
 * vertices on the GPU and the buffer restores them every cycle, so each frame is the
 * field applied to the stored mesh and not an accumulation. The hash, claim, density and
 * population stages are not wired.
 */
class MAYAFLUX_API MeshBuffer : public StateFieldBuffer {
public:
    /**
     * @brief Construct from an owning MeshData.
     *
     * The VKBuffer base is sized to vertex_bytes, at least one vertex. MeshProcessor
     * allocates the index buffer separately, when the first valid geometry arrives.
     *
     * @param data Loaded or generated mesh, or MeshData::empty() for a buffer that
     *        waits for write_frame.
     */
    explicit MeshBuffer(Kakshya::MeshData data);

    /**
     * @brief Construct from a MeshWriterNode.
     *
     * The node is polled each graphics cycle. compute_frame() is called
     * on the node, and if its data changed, set_mesh_data() is called
     * internally before MeshProcessor uploads.
     */
    MeshBuffer(std::shared_ptr<Nodes::GpuSync::MeshWriterNode> node);

    ~MeshBuffer() override;

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

    /**
     * @brief Read-only access to the owning MeshData.
     *
     * For a caller that needs an owning copy (e.g. IO::ModelWriter, which
     * takes MeshData rather than a non-owning view since it must outlive the
     * synchronous export call). Prefer access() when a view suffices.
     */
    [[nodiscard]] const Kakshya::MeshData& get_mesh_data() const noexcept
    {
        return m_mesh_data;
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

    [[nodiscard]] std::shared_ptr<Nodes::GpuSync::MeshWriterNode> get_node() const
    {
        return m_node;
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

    /**
     * @brief Replace the geometry with a frame.
     * @param frame Frame to show from the next graphics cycle. Null shows nothing.
     *
     * May be called from any thread and never blocks. Only the newest frame is kept.
     */
    void write_frame(std::shared_ptr<const Kakshya::MeshData> frame);

    /** @brief The newest frame written, or null before the first. */
    [[nodiscard]] std::shared_ptr<const Kakshya::MeshData> current_frame() const;

    /**
     * @brief Deform the vertices on the GPU with a field operator.
     * @param field_op Operator whose bindings select the field pass, or null for none.
     *
     * Call before setup_rendering, which wires the pass. The operator's layout must be
     * the buffer's vertex record.
     */
    void set_field_operator(std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op);

    /**
     * @brief Skin the vertices on the GPU from a palette of joint matrices.
     * @param skin Joints and per vertex weights covering the buffer's vertices, or null for none.
     * @param palette Matrices the pass starts from, one for each joint, or empty to leave the
     *        mesh as bound until set_palette.
     *
     * Call before setup_rendering, which wires the pass ahead of the field pass, so a field
     * displaces the skinned surface. The mesh the buffer holds is the rest pose, and so is every
     * frame written with write_frame or set by the mutators, which is skinned as the first was.
     * Geometry with another vertex count than the skin covers is shown undeformed.
     */
    void set_skin(std::shared_ptr<const Kakshya::MeshSkin> skin, std::vector<glm::mat4> palette = {});

    /**
     * @brief Publish the palette the skin pass reads, one matrix for each joint, in the space of the mesh.
     * @return False when no skin pass is wired yet or the count is not the skin's joint count.
     *
     * Safe from any thread (see Kinesis::relative_transforms).
     */
    bool set_palette(std::vector<glm::mat4> palette);

    /**
     * @brief Morph the vertices on the GPU toward the shapes of a set of targets.
     * @param morphs Targets with a position for each of the buffer's vertices, or null for none.
     *
     * Call before setup_rendering. The targets are blended into the rest pose by their weights,
     * the file's own to start with, in the same pass that skins, before the skin.
     */
    void set_morphs(std::shared_ptr<const Kakshya::MeshMorphTargets> morphs);

    /**
     * @brief Publish the weight of every morph target. Safe from any thread.
     * @return False when no pass is wired yet or the count is not the target count.
     */
    bool set_morph_weights(std::vector<float> weights);

    /** @brief Whether a wired field pass displaces the vertices in place every cycle. */
    [[nodiscard]] bool restores_vertices() const;

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

    /**
     * @brief Bind a diffuse texture to the render processor, if present.
     *
     * The binding name must match a descriptor slot in the shader. If the
     * render processor is not yet created, the texture will be bound on
     * creation if the default_texture_binding matches. The buffer holds the
     * image until it is replaced or the buffer ends.
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

    void wire_field_pass();
    void wire_skin_pass();
    void update_skin_rest();

    Kakshya::MeshData m_mesh_data;
    std::shared_ptr<Nodes::GpuSync::MeshWriterNode> m_node;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_field_op;
    bool m_field_wired {};
    std::shared_ptr<const Kakshya::MeshSkin> m_skin;
    std::vector<glm::mat4> m_initial_palette;
    std::shared_ptr<const Kakshya::MeshMorphTargets> m_morphs;
    std::shared_ptr<SkinProcessor> m_skin_processor;
    Memory::SnapshotCell<std::shared_ptr<const Kakshya::MeshData>> m_pending;
    std::shared_ptr<Core::VKImage> m_diffuse_texture;
    std::string m_diffuse_binding { "diffuseTex" };

    std::atomic<bool> m_vertices_dirty { true };
    std::atomic<bool> m_indices_dirty { true };
    std::atomic<bool> m_rest_dirty { false };

    std::shared_ptr<MeshProcessor> m_mesh_processor;
};

} // namespace MayaFlux::Buffers
