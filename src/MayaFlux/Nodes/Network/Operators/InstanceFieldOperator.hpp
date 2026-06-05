#pragma once

#include "InstanceOperator.hpp"

#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"

#include "MayaFlux/Nodes/Graphics/GpuComputeNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class InstanceFieldOperator
 * @brief Drives GeometrySlot transforms from stateless Kinesis fields.
 *
 * Each slot may have at most one PositionField or one TransformField bound,
 * keyed by slot index. On each cycle the bound field is evaluated against the
 * slot's current state and the result is written back into slot.transform.
 *
 * PositionField receives the slot's current translation (column 3 of
 * slot.transform as glm::vec3) and returns a new translation. Only column 3
 * is replaced; the rotational part of the matrix is preserved.
 *
 * TransformField receives the slot's current transform as glm::mat4 and
 * returns a replacement mat4. The full matrix is replaced.
 *
 * Both field types are evaluated statelessly - no accumulated time, no
 * internal integration. The field is a pure function of current slot state.
 * For time-varying behaviour, close over a shared atomic or external clock in
 * the field callable.
 *
 * Slots without a bound field are untouched.
 */
class MAYAFLUX_API InstanceFieldOperator : public InstanceOperator {
public:
    using TransformField = std::function<glm::mat4(const glm::mat4&)>;
    using PositionField = Kinesis::VectorField;

    /**
     * @brief Bind a TransformField to a slot.
     * @param slot_index Target slot index.
     * @param field      Function mapping current slot.transform to a new mat4.
     *
     * Replaces any previously bound field (position or transform) for the slot.
     */
    void bind_transform(uint32_t slot_index, TransformField field);

    /**
     * @brief Bind a PositionField to a slot.
     * @param slot_index Target slot index.
     * @param field      VectorField mapping current slot translation to a new translation.
     *
     * Replaces any previously bound field (position or transform) for the slot.
     */
    void bind_position(uint32_t slot_index, PositionField field);

    /**
     * @brief Remove all bindings for a slot.
     * @param slot_index Target slot index.
     */
    void unbind(uint32_t slot_index);

    /**
     * @brief Remove all bindings for all slots.
     */
    void unbind_all();

    void process_slot(GeometrySlot& slot, float dt) override;

    [[nodiscard]] std::string_view get_type_name() const override { return "InstanceField"; }

    /**
     * @brief Attach a GPU executor and switch to async dispatch mode.
     *
     * Constructs a GpuComputeNode from the executor. On each process() call
     * the node's compute_frame() is driven instead of the CPU field loop.
     * The on_complete callback reads gpu_result.primary as tightly-packed
     * mat4 rows (16 floats per slot, in slot-index order) and writes into
     * m_slots directly.
     *
     * CPU field bindings are preserved but ignored while a GPU executor is
     * attached. Passing nullptr clears the GPU path and restores CPU evaluation.
     *
     * @param executor Pre-configured ShaderExecutionContext. Output binding must
     *                 be sized for slot_count * 16 * sizeof(float). nullptr clears.
     * @param continuous If true the node re-arms after every completed dispatch.
     */
    void set_gpu_executor(
        std::shared_ptr<Yantra::ShaderExecutionContext<>> executor,
        bool continuous = true);

    /**
     * @brief Update push constants on the attached GPU executor.
     *
     * Arms the next dispatch by calling set_dirty() after the push. No-op if
     * no GPU executor is attached.
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
    std::unordered_map<uint32_t, TransformField> m_transform_fields;
    std::unordered_map<uint32_t, PositionField> m_position_fields;

    std::shared_ptr<Yantra::ShaderExecutionContext<>> m_executor;
    std::shared_ptr<Nodes::GpuSync::GpuComputeNode> m_compute_node;
};

} // namespace MayaFlux::Nodes::Network
