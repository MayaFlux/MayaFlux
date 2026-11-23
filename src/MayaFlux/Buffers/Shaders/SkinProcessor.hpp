#pragma once

#include "ComputeProcessor.hpp"

#include "MayaFlux/Kakshya/NDData/MeshSkin.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Buffers {

class StateFieldBuffer;

/**
 * @brief The spec of the Skin kernel: rest, joints, weights, palette and vertices, in that binding order.
 * @param workgroup Invocations per group, from 1 to 1024.
 */
[[nodiscard]] MAYAFLUX_API Portal::Graphics::ShaderSpec skin_spec(uint32_t workgroup = 256);

/**
 * @class SkinProcessor
 * @brief Compute pass that skins the vertices of an attached StateFieldBuffer from a palette of joint matrices.
 *
 * Every cycle it reads the rest position, normal and tangent of each vertex, blends the palette
 * matrices of the vertex's joints by its weights and writes the skinned position, normal and tangent
 * into the attached buffer, leaving every other word of a record as the producer uploaded it. The
 * rest records, the joints and the weights live as state fields of the buffer, uploaded once when the
 * processor attaches, and the palette is a state field that is rewritten when a new palette arrives.
 * Because it writes from the rest records and never from the vertices it last wrote, it needs no
 * restore of the vertices from the host to stay correct, and a field pass added after it displaces
 * the skinned surface.
 *
 * The palette is published from any thread by set_palette, as the matrices MeshSkin's joints give,
 * one for each joint in the skin's order, in the space of the mesh (Kinesis::skin_palette). Until a
 * palette has been published the pass does not run and the mesh stays in the pose it was uploaded in.
 * The matrices must be affine: the last row is not read.
 *
 * The attached buffer's records are MeshVertex records, possibly followed by more words
 * (stride_words is read from the buffer's vertex layout). A range of the buffer, such as one slot of a
 * network's concatenated vertices, is chosen with set_first_vertex.
 *
 * A buffer that carries several skins gives each processor a tag, which names its state fields.
 */
class MAYAFLUX_API SkinProcessor : public ComputeProcessor {
public:
    /**
     * @param skin The skin. It must be valid for as many vertices as rest_vertices holds.
     * @param rest_vertices The mesh's rest records, MeshVertex after MeshVertex.
     * @param tag Name that keeps this processor's state fields apart from another's on the same buffer.
     *        Throws std::invalid_argument when the skin is null or invalid, or the records do not match it.
     */
    SkinProcessor(
        std::shared_ptr<const Kakshya::MeshSkin> skin,
        std::span<const uint8_t> rest_vertices,
        std::string tag = {});

    ~SkinProcessor() override = default;

    /**
     * @brief Publish a palette, one matrix for each joint of the skin.
     * @return False, leaving the last palette in place, when the count is not the skin's joint count.
     *
     * Safe from any thread. The pass reads the newest palette on its next cycle.
     */
    bool set_palette(std::vector<glm::mat4> palette);

    /** @brief Index in the attached buffer's records of the skin's first vertex. Zero by default. */
    void set_first_vertex(uint32_t first_vertex);

    /**
     * @brief Ask for the index of the skin's first vertex each cycle, for a buffer whose ranges move.
     * @param source Called on the graphics thread before each dispatch. Null returns to the fixed index.
     */
    void set_first_vertex_source(std::function<uint32_t()> source);

    [[nodiscard]] const std::shared_ptr<const Kakshya::MeshSkin>& get_skin() const noexcept { return m_skin; }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

    std::array<uint32_t, 3> calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer) override;

    void collect_push_writes(
        const std::shared_ptr<VKBuffer>& buffer,
        std::vector<Portal::Graphics::PushDescriptorWrite>& writes) override;

private:
    struct PushBlock {
        uint32_t first_vertex {};
        uint32_t vertex_count {};
        uint32_t stride_words {};
        uint32_t palette_base {};
    };

    static_assert(sizeof(PushBlock) == 16);

    std::shared_ptr<const Kakshya::MeshSkin> m_skin;
    std::vector<float> m_rest;
    std::string m_tag;
    PushBlock m_params;
    std::function<uint32_t()> m_first_source;

    Memory::SnapshotCell<std::vector<glm::mat4>> m_palette;
    std::atomic<uint64_t> m_palette_revision { 0 };
    uint64_t m_uploaded_revision { 0 };

    std::shared_ptr<StateFieldBuffer> m_state_buffer;
    std::shared_ptr<VKBuffer> m_staging;
    std::array<uint32_t, 5> m_binding_index {};
    bool m_declared {};

    [[nodiscard]] std::string field_name(std::string_view role) const;
};

} // namespace MayaFlux::Buffers
