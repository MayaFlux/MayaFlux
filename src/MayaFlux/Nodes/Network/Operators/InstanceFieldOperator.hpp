#pragma once

#include "InstanceOperator.hpp"

#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"

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

private:
    std::unordered_map<uint32_t, TransformField> m_transform_fields;
    std::unordered_map<uint32_t, PositionField> m_position_fields;
};

} // namespace MayaFlux::Nodes::Network
