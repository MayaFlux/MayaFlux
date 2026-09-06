#pragma once

namespace MayaFlux::Kinesis {

/**
 * @enum FieldTarget
 * @brief What a Tendency drives when applied to a vertex record.
 *
 * A bitmask. One field may drive several attributes from a single bind, which
 * is a convenience on the CPU path and a requirement on the GPU one: a
 * DualField emits exactly one GLSL function, so binding the same field twice
 * through separate calls would be a redefinition. Binding once against a mask
 * emits the function once and calls it per target bit.
 *
 * The listed bits correspond to the vertex attributes in the offset table
 * shared by VertexLayout::for_points, for_lines, for_meshes and for_raw. That
 * list is not a closed set: targets outside it are expected, and an operator
 * that cannot drive one either ignores it or rejects the bind with a
 * diagnostic naming the bit. Validity is a property of the operator and the
 * layout it addresses, not of the enum.
 *
 * @code
 * op->bind(FieldTarget::POSITION | FieldTarget::NORMAL, displacement);
 * @endcode
 */
enum class FieldTarget : uint16_t {
    NONE = 0U,
    POSITION = 1U << 0U,
    COLOR = 1U << 1U,
    NORMAL = 1U << 2U,
    TANGENT = 1U << 3U,
    SCALAR = 1U << 4U,
    UV = 1U << 5U,
};

MF_BITMASK_OPERATORS(FieldTarget)

/**
 * @enum FieldMode
 * @brief How a field result combines with the value already present.
 *
 * Exclusive rather than a mask: a target is either replaced or added to.
 */
enum class FieldMode : uint8_t {
    ABSOLUTE,
    ACCUMULATE
};

/**
 * @brief Single-bit targets known to this version, in vertex layout order.
 *
 * Iteration source for code that expands a mask into per-attribute work. A
 * target absent from this array is not invalid, only unknown here; operators
 * that introduce their own extend the iteration rather than the enum's
 * meaning.
 */
inline constexpr std::array<FieldTarget, 6> k_field_targets {
    FieldTarget::POSITION,
    FieldTarget::COLOR,
    FieldTarget::NORMAL,
    FieldTarget::TANGENT,
    FieldTarget::SCALAR,
    FieldTarget::UV,
};

/**
 * @brief Targets that accept a three-component field.
 *
 * Validate a mask with any_flag(mask & ~k_vector_targets), which is nonzero
 * exactly when the mask reaches a target a VectorField cannot drive.
 */
inline constexpr FieldTarget k_vector_targets
    = FieldTarget::POSITION | FieldTarget::COLOR
    | FieldTarget::NORMAL | FieldTarget::TANGENT;

} // namespace MayaFlux::Kinesis
