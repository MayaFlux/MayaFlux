#pragma once

#include "Influence.hpp"

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Tendency/FieldBinding.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Nexus {

namespace detail {

    template <typename D, typename R>
    Kinesis::Tendency<D, R> forwarding(const std::shared_ptr<Kinesis::Tendency<D, R>>& cell)
    {
        return { .fn = [cell](const D& d) -> R { return cell->fn ? cell->fn(d) : R(0.0F); } };
    }

} // namespace detail

/**
 * @brief Target that feeds an influence of fields into an operator's vertex attributes.
 * @tparam Field Kinesis::VectorField, SpatialField or UVField, matching @p target.
 * @param op     FieldOperator, as a raw pointer from create_operator or a shared_ptr.
 * @param target Vertex attribute mask the field drives.
 *
 * Binds one forwarding field to @p op now, and every time the influence runs the
 * target replaces what that field forwards to. Other fields bound to the same
 * target are left alone, which unbinding and rebinding each tick would not do.
 * The operator is not retained. The influence and the operator must run on the
 * same tick domain.
 */
template <typename Field, typename Operator>
[[nodiscard]] typename Influence<Field>::Target bind_field(
    const Operator& op,
    Kinesis::FieldTarget target)
{
    auto cell = std::make_shared<Field>();
    op->bind(target, detail::forwarding(cell));
    return [cell](const Field& field) { *cell = field; };
}

/**
 * @brief Same as above, for one slot of a MeshFieldOperator.
 * @param slot Slot index the field drives.
 */
template <typename Field, typename Operator>
[[nodiscard]] typename Influence<Field>::Target bind_field(
    const Operator& op,
    uint32_t slot,
    Kinesis::FieldTarget target)
{
    auto cell = std::make_shared<Field>();
    op->bind(slot, target, detail::forwarding(cell));
    return [cell](const Field& field) { *cell = field; };
}

/**
 * @brief Target that feeds an influence of vector fields into a PhysicsOperator as a force.
 * @param op PhysicsOperator, as a raw pointer from create_operator or a shared_ptr.
 *
 * Adds one forwarding force field now, and every time the influence runs the
 * target replaces what that field forwards to. Force fields already added to
 * @p op are left alone. The operator is not retained.
 */
template <typename Operator>
[[nodiscard]] Influence<Kinesis::VectorField>::Target bind_force(const Operator& op)
{
    auto cell = std::make_shared<Kinesis::VectorField>();
    op->add_force_field(detail::forwarding(cell));
    return [cell](const Kinesis::VectorField& field) { *cell = field; };
}

/**
 * @brief Target that feeds an influence of vector fields into one instance's position.
 * @param op   InstanceFieldOperator, as a raw pointer or a shared_ptr.
 * @param slot Instance slot the field drives.
 */
template <typename Operator>
[[nodiscard]] Influence<Kinesis::VectorField>::Target bind_position(const Operator& op, uint32_t slot)
{
    auto cell = std::make_shared<Kinesis::VectorField>();
    op->bind_position(slot, detail::forwarding(cell));
    return [cell](const Kinesis::VectorField& field) { *cell = field; };
}

/**
 * @brief Target that feeds an influence of transform functions into one slot.
 * @param op   InstanceFieldOperator or MeshTransformOperator, raw pointer or shared_ptr.
 * @param slot Slot the function drives.
 *
 * The influence's value is the operator's own TransformField: for an
 * InstanceFieldOperator a function from the slot's current transform to its next
 * one, for a MeshTransformOperator a function from accumulated seconds to the
 * slot's local transform.
 */
template <typename Operator>
[[nodiscard]] auto bind_transform(const Operator& op, uint32_t slot)
{
    using Transform = typename std::remove_cvref_t<decltype(*op)>::TransformField;

    auto cell = std::make_shared<Transform>();
    if constexpr (requires { op->bind_transform(slot, std::declval<Transform>()); }) {
        op->bind_transform(slot, [cell](const glm::mat4& current) {
            return *cell ? (*cell)(current) : current;
        });
    } else {
        op->bind(slot, [cell](float seconds) {
            return *cell ? (*cell)(seconds) : glm::mat4(1.0F);
        });
    }
    return typename Influence<Transform>::Target { [cell](const Transform& field) { *cell = field; } };
}

/**
 * @brief Target that moves or resizes an Expanse's bounds from an influence of boxes.
 * @param expanse Expanse, raw pointer or shared_ptr, retained by the target.
 */
template <typename Region>
[[nodiscard]] Influence<Kinesis::AABB3D>::Target bind_bounds(const Region& expanse)
{
    return [expanse](const Kinesis::AABB3D& bounds) { expanse->set_bounds(bounds); };
}

/**
 * @brief Target that sets the parameter block of a GpuFieldOperator's parametric fields.
 * @param op GpuFieldOperator, raw pointer or shared_ptr; retained by the target.
 *
 * The influence's value is the vec4 parametric fields receive as their third
 * argument. It is pushed every dispatch, so nothing is rebound or recompiled.
 */
template <typename Operator>
[[nodiscard]] Influence<glm::vec4>::Target bind_params(const Operator& op)
{
    return [op](const glm::vec4& params) { op->set_params(params); };
}

/**
 * @brief Target that pushes an influence's value to a GPU executor as push constants.
 * @tparam T Trivially copyable struct matching the shader's push constant layout.
 * @param target An operator with push_constants (MeshFieldOperator,
 *        InstanceFieldOperator) or a shader processor with set_push_constant_data;
 *        raw pointer or shared_ptr, retained by the target.
 *
 * For an operator this arms the next dispatch of its executor. This is how an
 * Agent's state reaches a slot-transform kernel, for example one assembled from
 * a DualField of mat4 through ShaderSpec::Assemble::function(). For a shader
 * processor the push block is grown to hold T at @p offset here, so a shader
 * that already declares it is not drawn before the block exists. A shader that
 * keeps push constants of its own passes the byte offset after them and
 * declares both in one block.
 */
template <typename T, typename Receiver>
[[nodiscard]] typename Influence<T>::Target bind_push_constants(const Receiver& target, size_t offset = 0)
{
    if constexpr (requires { target->get_push_constant_data(); }) {
        if (target->get_push_constant_data().size() < offset + sizeof(T)) {
            target->set_push_constant_size(offset + sizeof(T));
        }
    }

    return [target, offset](const T& data) {
        if constexpr (requires { target->push_constants(data); }) {
            target->push_constants(data);
        } else {
            if (target->get_push_constant_data().size() < offset + sizeof(T)) {
                target->set_push_constant_size(offset + sizeof(T));
            }
            std::memcpy(target->get_push_constant_data().data() + offset, &data, sizeof(T));
        }
    };
}

/**
 * @brief Target that pushes an InfluenceBlock to a render processor for the library's lit shaders.
 * @param target A render processor, raw pointer or shared_ptr, retained by the target.
 * @param offset Byte offset of the block when the shader keeps push constants ahead of it.
 *
 * Pairs with InfluenceBlock::from as the producer.
 */
template <typename Receiver>
[[nodiscard]] Influence<InfluenceBlock>::Target bind_influence_block(const Receiver& target, size_t offset = 0)
{
    return bind_push_constants<InfluenceBlock>(target, offset);
}

} // namespace MayaFlux::Nexus
