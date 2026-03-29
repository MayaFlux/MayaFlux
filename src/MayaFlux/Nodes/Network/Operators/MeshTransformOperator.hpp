#pragma once

#include "MeshOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class MeshTransformOperator
 * @brief Primary operator that drives slot local transforms via Tendency fields
 *        and propagates world transforms down the slot DAG.
 *
 * Each slot may have at most one bound TemporalField (glm::mat4 -> glm::mat4).
 * On process_slot() the field is evaluated with dt as the scalar input,
 * converting it to a glm::vec3 (unused) placeholder -- in practice the mat4
 * overload is used: the field receives the slot's current local_transform and
 * returns the new local_transform for the cycle.
 *
 * Because transform propagation requires the parent's world_transform to
 * already be resolved before a child is processed, process_slot() is called
 * in topological order (guaranteed by MeshOperator::process()). Each call:
 *   1. Evaluates the bound field (if any) to update local_transform.
 *   2. Derives world_transform as parent_world * local_transform.
 *   3. Sets slot.dirty = true so MeshNetworkProcessor triggers a re-upload.
 *
 * Slots without a bound field still receive world transform propagation --
 * their local_transform is used as-is, matching the behaviour of the manual
 * example in the fracture example (which drives local_transform and dirty
 * directly without an operator).
 *
 * The TemporalField signature: glm::mat4 -> glm::mat4.  In Kinesis terms
 * this is modelled as a std::function<glm::mat4(float)> where the float
 * argument is the accumulated time in seconds. The operator tracks per-slot
 * accumulated time independently.
 *
 * Usage:
 * @code
 * auto xf_op = net->create_operator<MeshTransformOperator>();
 *
 * // rotate slot 0 around Y at 1 rad/s
 * xf_op->bind(0, [](float t) {
 *     return glm::rotate(glm::mat4(1.0F), t, glm::vec3(0, 1, 0));
 * });
 * @endcode
 */
class MAYAFLUX_API MeshTransformOperator : public MeshOperator {
public:
    /**
     * @brief Field type: maps accumulated time (seconds) to a local glm::mat4.
     */
    using TransformField = std::function<glm::mat4(float)>;

    MeshTransformOperator() = default;
    ~MeshTransformOperator() override = default;

    // -------------------------------------------------------------------------
    // Field binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a TransformField to a slot.
     * @param slot_index Target slot index (as returned by MeshNetwork::add_slot()).
     * @param field      Function mapping accumulated time to local glm::mat4.
     *
     * Replaces any previously bound field for that slot.
     * Pass an empty std::function to remove a binding.
     */
    void bind(uint32_t slot_index, TransformField field);

    /**
     * @brief Remove the TransformField bound to a slot.
     * @param slot_index Target slot index.
     */
    void unbind(uint32_t slot_index);

    /**
     * @brief Remove all bound fields.
     */
    void unbind_all();

    // -------------------------------------------------------------------------
    // MeshOperator interface
    // -------------------------------------------------------------------------

    /**
     * @brief Evaluate the bound field (if any), update local_transform, then
     *        propagate the world_transform from the parent.
     */
    void process_slot(MeshSlot& slot, float dt) override;

    void process(float dt) override;

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "MeshTransform";
    }

private:
    std::unordered_map<uint32_t, TransformField> m_fields;

    /**
     * @brief Per-slot accumulated time in seconds, keyed by slot index.
     */
    std::unordered_map<uint32_t, float> m_accumulated_time;

    /**
     * @brief Wall-clock timestamp of the last process() call.
     *
     * Used to compute a real dt independent of the num_samples argument,
     * which is always 1 for visual-rate networks and carries no time information.
     */
    std::chrono::steady_clock::time_point m_last_tick { std::chrono::steady_clock::now() };

    /**
     * @brief Resolve the world transform of a slot's parent, or identity if root.
     * @param slot Slot whose parent_index is queried.
     * @return Parent's world_transform, or identity mat4 for root slots.
     */
    [[nodiscard]] glm::mat4 parent_world(const MeshSlot& slot) const;
};

} // namespace MayaFlux::Nodes::Network
