#pragma once

#include "ComputeProcessor.hpp"

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/MeshSkin.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Buffers {

class StateFieldBuffer;

/**
 * @brief The spec of the Skin kernel: rest, joints, weights, palette, vertices, deltas and
 *        morph_weights, in that binding order.
 * @param workgroup Invocations per group, from 1 to 1024.
 */
[[nodiscard]] MAYAFLUX_API Portal::Graphics::ShaderSpec skin_spec(uint32_t workgroup = 256);

/**
 * @class SkinProcessor
 * @brief Compute pass that morphs and skins the vertices of an attached StateFieldBuffer.
 *
 * Every cycle it reads the rest position, normal and tangent of each vertex, adds each morph
 * target's difference from the rest by that target's weight, blends the palette matrices of the
 * vertex's joints by its weights and writes the result into the attached buffer, leaving every
 * other word of a record as the producer uploaded it. Either of the two may be absent. The rest
 * records, joints, weights, target differences and palette live as state fields of the buffer.
 * Because it writes from the rest records and never from the vertices it last wrote, it needs no
 * restore of position, normal and tangent from the host, and a field pass added after it displaces
 * the deformed surface.
 *
 * The palette is published from any thread by set_palette, one matrix for each joint in the
 * skin's order, in the space of the mesh (Kinesis::skin_palette), and morph weights by
 * set_morph_weights. A skin without a palette does not run and the mesh stays as uploaded. With
 * morph targets the pass runs from the start, with the targets' own weights and the mesh as bound.
 * The matrices must be affine: the last row is not read.
 *
 * The attached buffer's records are MeshVertex records, possibly followed by more words
 * (stride_words is read from the buffer's vertex layout). A range of the buffer, such as one slot of a
 * network's concatenated vertices, is chosen with set_first_vertex.
 *
 * A buffer that carries several of these gives each a tag, which names its state fields.
 */
class MAYAFLUX_API SkinProcessor : public ComputeProcessor {
public:
    /**
     * @param skin The skin, or null for morph targets alone. It must be valid for as many
     *        vertices as rest_vertices holds.
     * @param rest_vertices The mesh's rest records, MeshVertex after MeshVertex.
     * @param tag Name that keeps this processor's state fields apart from another's on the same buffer.
     * @param morphs The mesh's morph targets, or null. Each must give a position for every vertex.
     *        Throws std::invalid_argument when there is neither a skin nor a target, or either
     *        does not fit the records.
     */
    SkinProcessor(
        std::shared_ptr<const Kakshya::MeshSkin> skin,
        std::span<const uint8_t> rest_vertices,
        std::string tag = {},
        std::shared_ptr<const Kakshya::MeshMorphTargets> morphs = nullptr);

    ~SkinProcessor() override = default;

    /**
     * @brief Publish a palette, one matrix for each joint of the skin.
     * @return False, leaving the last palette in place, when there is no skin or the count is not
     *         its joint count.
     *
     * Safe from any thread. The pass reads the newest palette on its next cycle.
     */
    bool set_palette(std::vector<glm::mat4> palette);

    /**
     * @brief Publish the morph weights, one for each target in order.
     * @return False, leaving the last weights in place, when the count is not the target count.
     *
     * Safe from any thread.
     */
    bool set_morph_weights(std::vector<float> weights);

    /**
     * @brief Compute the palette each cycle instead of waiting for set_palette.
     * @param source Called on the graphics thread before each dispatch. A result of the wrong
     *        size keeps the last palette. Null returns to set_palette.
     */
    void set_palette_source(std::function<std::vector<glm::mat4>()> source);

    /**
     * @brief Replace the rest records the pass deforms, as a producer of new geometry does.
     *
     * For the thread that processes the buffer. Records of another vertex count than the skin or
     * targets cover suspend the pass until the count fits again, so the buffer shows them as given.
     */
    void set_rest(std::span<const uint8_t> rest_vertices);

    /** @brief Whether the last cycle wrote the deformed vertices. For the processing thread. */
    [[nodiscard]] bool is_active() const noexcept { return m_active; }

    /** @brief Index in the attached buffer's records of the first vertex. Zero by default. */
    void set_first_vertex(uint32_t first_vertex);

    /**
     * @brief Ask for the index of the first vertex each cycle, for a buffer whose ranges move.
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
        uint32_t target_count {};
    };

    static_assert(sizeof(PushBlock) == 20);

    std::shared_ptr<const Kakshya::MeshSkin> m_skin;
    std::shared_ptr<const Kakshya::MeshMorphTargets> m_morphs;
    std::vector<float> m_rest;
    std::vector<float> m_deltas;
    std::string m_tag;
    PushBlock m_params;
    std::function<uint32_t()> m_first_source;
    std::function<std::vector<glm::mat4>()> m_palette_source;
    std::vector<glm::mat4> m_sourced_palette;

    Memory::SnapshotCell<std::vector<glm::mat4>> m_palette;
    std::atomic<uint64_t> m_palette_revision { 0 };
    uint64_t m_uploaded_revision { 0 };

    Memory::SnapshotCell<std::vector<float>> m_morph_weights;
    std::atomic<uint64_t> m_weight_revision { 0 };
    uint64_t m_uploaded_weight_revision { 0 };

    std::shared_ptr<StateFieldBuffer> m_state_buffer;
    std::shared_ptr<VKBuffer> m_staging;
    std::array<uint32_t, 7> m_binding_index {};
    bool m_declared {};
    bool m_rest_pending {};
    bool m_rest_fits { true };
    bool m_active {};

    [[nodiscard]] std::string field_name(std::string_view role) const;
    [[nodiscard]] size_t joint_count() const noexcept;
    [[nodiscard]] size_t target_count() const noexcept;
    void pack_rest(std::span<const uint8_t> rest_vertices);
};

} // namespace MayaFlux::Buffers
