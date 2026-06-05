#pragma once

#include "FieldOperator.hpp"
#include "MeshOperator.hpp"

#include "MayaFlux/Nodes/Graphics/GpuComputeNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class MeshFieldOperator
 * @brief Chain operator that applies Tendency field deformation to the
 *        vertex data of individual MeshNetwork slots.
 *
 * Holds one FieldOperator per bound slot, keyed by slot index.
 * bind() creates or replaces the per-slot FieldOperator and initialises
 * it from the slot's current MeshVertex array so the reference frame
 * is always the geometry as it existed at bind time.
 *
 * process_slot() runs the slot's FieldOperator::process(dt) then writes
 * the resulting MeshVertex array back via
 * slot.node->set_mesh_vertices(). That call sets m_vertex_data_dirty on
 * the MeshWriterNode, which any_slot_dirty() in MeshNetworkProcessor
 * already checks.
 *
 * Slots without a bound FieldOperator are skipped entirely.
 *
 * This operator belongs in the OperatorChain, not as the primary operator.
 * MeshTransformOperator runs first (as primary) to propagate world
 * transforms; MeshFieldOperator runs after to deform vertex positions in
 * local space.
 *
 * Usage:
 * @code
 * auto field_op = std::make_shared<MeshFieldOperator>();
 * net->get_operator_chain()->emplace<MeshFieldOperator>();
 *
 * // Retrieve the typed pointer from the chain or hold it before emplacing.
 * field_op->bind(torso_idx, FieldTarget::POSITION,
 *     Kinesis::make_radial_pull(glm::vec3(0), 4.0F));
 * @endcode
 */
class MAYAFLUX_API MeshFieldOperator : public MeshOperator {
public:
    MeshFieldOperator() = default;
    ~MeshFieldOperator() override = default;

    // -------------------------------------------------------------------------
    // Field binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a VectorField to a slot.
     * @param slot_index Target slot index.
     * @param target     Vertex attribute to drive (POSITION, COLOR, NORMAL, TANGENT).
     * @param field      VectorField: glm::vec3 -> glm::vec3.
     *
     * Creates a FieldOperator for the slot if one does not yet exist.
     * The FieldOperator is initialised from the slot's current vertex data.
     * Subsequent binds on the same slot append fields to the existing operator.
     */
    void bind(uint32_t slot_index, FieldTarget target, Kinesis::VectorField field);

    /**
     * @brief Bind a SpatialField to a slot.
     * @param slot_index Target slot index.
     * @param target     Vertex attribute to drive (SCALAR or UV).
     * @param field      SpatialField: glm::vec3 -> float.
     */
    void bind(uint32_t slot_index, FieldTarget target, Kinesis::SpatialField field);

    /**
     * @brief Bind a UVField to a slot.
     * @param slot_index Target slot index.
     * @param target     Must be FieldTarget::UV.
     * @param field      UVField: glm::vec3 -> glm::vec2.
     */
    void bind(uint32_t slot_index, FieldTarget target, Kinesis::UVField field);

    /**
     * @brief Remove all fields bound to a target on a specific slot.
     * @param slot_index Target slot index.
     * @param target     Target to clear.
     */
    void unbind(uint32_t slot_index, FieldTarget target);

    /**
     * @brief Remove the entire FieldOperator for a slot.
     * @param slot_index Target slot index.
     */
    void unbind_slot(uint32_t slot_index);

    /**
     * @brief Remove all per-slot FieldOperators.
     */
    void unbind_all();

    /**
     * @brief Set the field application mode for a slot's FieldOperator.
     * @param slot_index Target slot index.  The FieldOperator must already exist.
     * @param mode       ABSOLUTE or ACCUMULATE.
     */
    void set_mode(uint32_t slot_index, FieldMode mode);

    // -------------------------------------------------------------------------
    // MeshOperator interface
    // -------------------------------------------------------------------------

    /**
     * @brief Run the slot's FieldOperator (if bound) and write results back to
     *        slot.node via set_mesh_vertices().
     */
    void process_slot(MeshSlot& slot, float dt) override;

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "MeshField";
    }

    // -------------------------------------------------------------------------
    // GPU execution
    // -------------------------------------------------------------------------

    /**
     * @brief Attach a GPU executor and switch to async dispatch mode.
     *
     * Constructs a GpuComputeNode from the executor. On process() calls,
     * compute_frame() is driven on the node instead of the per-slot CPU
     * FieldOperator loop.
     *
     * The on_complete callback expects gpu_result.primary to contain vertex
     * data for each slot packed sequentially: slot 0 vertices, then slot 1
     * vertices, etc. Vertex count per slot must match the node's current
     * get_mesh_vertex_count() at the time the callback fires. The callback
     * writes back via slot.node->set_mesh_vertices().
     *
     * Slot vertex counts are snapshotted at set_gpu_executor() time from the
     * current m_slots state. Call set_gpu_executor() after slots are fully
     * populated. If slot geometry changes after attachment, call
     * set_gpu_executor() again to rebuild the snapshot.
     *
     * CPU FieldOperator bindings are preserved but ignored while a GPU
     * executor is attached. Passing nullptr clears the GPU path.
     *
     * @param executor Pre-configured ShaderExecutionContext. nullptr clears.
     * @param continuous If true the node re-arms after every completed dispatch.
     */
    void set_gpu_executor(
        std::shared_ptr<Yantra::ShaderExecutionContext<>> executor,
        bool continuous = true);

    /**
     * @brief Update push constants on the attached GPU executor.
     *
     * Arms the next dispatch. No-op if no GPU executor is attached.
     *
     * @tparam T Trivially copyable struct matching the shader push constant layout.
     * @param data Push constant data.
     */
    template <typename T>
    void push_constants(const T& data)
    {
        if (m_executor)
            m_executor->push(data);
        if (m_compute_node)
            m_compute_node->set_dirty();
    }

    /**
     * @brief Returns true if a GPU executor is currently attached.
     */
    [[nodiscard]] bool has_gpu_executor() const { return m_compute_node != nullptr; }

    // Override process() to drive compute_frame() on the GPU path.
    void process(float dt) override;

private:
    std::unordered_map<uint32_t, std::shared_ptr<FieldOperator>> m_field_ops;

    /**
     * @brief Return an existing FieldOperator for the slot, or create and
     *        initialise one from the slot's current vertex data.
     * @param slot       Slot whose FieldOperator to retrieve or create.
     * @param slot_index Slot index (key into m_field_ops).
     * @return Pointer to the (possibly new) FieldOperator.
     */
    [[nodiscard]] std::shared_ptr<FieldOperator>
    get_or_create(MeshSlot& slot, uint32_t slot_index);

    std::shared_ptr<Yantra::ShaderExecutionContext<>> m_executor;
    std::shared_ptr<Nodes::GpuSync::GpuComputeNode> m_compute_node;

    /**
     * @brief Vertex count per slot index, snapshotted at set_gpu_executor() time.
     *
     * Used by the on_complete callback to slice gpu_result.primary into
     * per-slot spans without re-querying slot nodes.
     */
    std::vector<size_t> m_gpu_slot_vertex_counts;
};

} // namespace MayaFlux::Nodes::Network
