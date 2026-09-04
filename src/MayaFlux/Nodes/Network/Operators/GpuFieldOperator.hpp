#pragma once

#include "NetworkOperator.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Kinesis/Tendency/DualField.hpp"
#include "MayaFlux/Kinesis/Tendency/FieldBinding.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Nodes::Network {

using FieldTarget = Kinesis::FieldTarget;
using FieldMode = Kinesis::FieldMode;

/**
 * @class GpuFieldOperator
 * @brief Chain operator that declares Tendency field deformation as a compute
 *        shader rather than evaluating it on the CPU.
 *
 * Holds no vertex data and no Vulkan objects. It owns bindings and emits a
 * ShaderSpec; a ComputeProcessor in the buffer chain owns the dispatch. This is
 * a NetworkOperator rather than a GraphicsOperator, so
 * NetworkGeometryProcessor's dynamic_cast fails and the operator contributes an
 * empty render slice without any participates_in_rendering bookkeeping.
 *
 * Vertex-agnostic. Addressing is derived from the supplied VertexLayout, whose
 * attribute offsets are identical across for_points, for_lines, for_meshes and
 * for_raw, so one operator serves any of them. Pass for_raw(stride) for
 * pre-packed data that fits no struct.
 *
 * Bound fields are DualFields: the same authored text that FieldOperator would
 * consume through .cpu is emitted here through .source as a GLSL function. A
 * field can be moved between the two operators without being rewritten.
 *
 * Field domain is always the vertex position read from the record, matching
 * Tendency<glm::vec3, R>. FieldMode::ABSOLUTE is therefore only meaningful when
 * the target is not POSITION: writing an absolute position would require a
 * reference copy of the original vertices, which this operator does not
 * allocate. bind() rejects that combination.
 *
 * @code
 * namespace MayaFlux::Fields {
 * using namespace MayaFlux::ShaderCompat;
 * const auto swirl = MF_FIELD(swirl, [](vec3 p) -> vec3 {
 *     return cross(vec3(0.0f, 1.0f, 0.0f), p) * 2.0f;
 * });
 * }
 *
 * auto op = net->get_operator_chain()->emplace<GpuFieldOperator>(
 *     Kakshya::VertexLayout::for_points());
 * op->bind(FieldTarget::POSITION | FieldTarget::NORMAL, Fields::swirl);
 * @endcode
 */
class MAYAFLUX_API GpuFieldOperator : public NetworkOperator {
public:
    /**
     * @param layout Vertex layout describing the record the shader will write.
     *               stride_bytes and every bound attribute's offset_in_vertex
     *               must be divisible by 4; construction fails loudly otherwise.
     */
    explicit GpuFieldOperator(Kakshya::VertexLayout layout);

    ~GpuFieldOperator() override = default;

    // -------------------------------------------------------------------------
    // Field binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a three-component field to one or more vec3 targets.
     * @param target Mask over POSITION, COLOR, NORMAL and TANGENT.
     * @param field  Dual-source field with a usable shader half.
     *
     * Binding nothing and returning is the response to an empty mask, a bit the
     * layout does not carry, a component-count mismatch on any bit, a field
     * whose shader half failed to parse, or a function name already emitted
     * with a different body. Validation covers the whole mask before anything
     * is stored, so a partly-valid mask binds nothing rather than part.
     *
     * Several fields may drive the same target. They sum before the write,
     * matching FieldOperator, and NORMAL and TANGENT normalise after the sum.
     */
    void bind(FieldTarget target, const Kinesis::DualVectorField& field);

    /**
     * @brief Bind a scalar field. Target must be exactly SCALAR.
     */
    void bind(FieldTarget target, const Kinesis::DualSpatialField& field);

    /**
     * @brief Bind a two-component field. Target must be exactly UV.
     */
    void bind(FieldTarget target, const Kinesis::DualUVField& field);

    /**
     * @brief Clear the given targets.
     *
     * Removes each bit in the mask from every binding that carries it. A
     * binding left with no targets is dropped; one that still drives another
     * target survives.
     */
    void unbind(FieldTarget target);

    /**
     * @brief Number of bound fields. One binding may drive several targets.
     */
    [[nodiscard]] size_t binding_count() const { return m_bindings.size(); }

    void set_mode(FieldMode mode);
    [[nodiscard]] FieldMode get_mode() const { return m_mode; }

    /**
     * @brief Descriptor binding index the vertex SSBO occupies.
     *
     * Must match what the owning processor pushes. Default 0.
     */
    void set_vertex_binding(uint32_t binding);

    /**
     * @brief Workgroup size along x. Default 256.
     */
    void set_workgroup_size(uint32_t x);

    // -------------------------------------------------------------------------
    // Shader
    // -------------------------------------------------------------------------

    /**
     * @brief Assemble the compute spec for the current bindings.
     *
     * Emits one GLSL function per distinct bound field, then a kernel that
     * reads the position once and writes every touched target in a single
     * pass. Fields sharing a target sum before the write; NORMAL and TANGENT
     * normalise after the sum. Cached until a bind, unbind, or configuration
     * change invalidates it. Returns nullopt when nothing is bound, when the
     * layout carries no position attribute, or when ABSOLUTE mode meets a
     * POSITION binding.
     *
     * ABSOLUTE with POSITION is refused here rather than at bind because mode
     * may change after a field is bound. Writing an absolute position needs a
     * reference copy of the original vertices, which this operator does not
     * allocate.
     */
    [[nodiscard]] std::optional<Portal::Graphics::ShaderSpec> build_spec() const;

    /**
     * @brief Push constant bytes for the current cycle.
     *
     * Written by process(). Layout matches the block declared by build_spec().
     */
    [[nodiscard]] std::span<const uint8_t> push_constants() const;

    /**
     * @brief Layout the emitted shader addresses.
     */
    [[nodiscard]] const Kakshya::VertexLayout& get_layout() const { return m_layout; }

    // -------------------------------------------------------------------------
    // NetworkOperator interface
    // -------------------------------------------------------------------------

    /**
     * @brief Update push constant values. Performs no GPU work.
     */
    void process(float dt) override;

    /**
     * @brief No settable parameters.
     *
     * dt is owned by process() and rewritten every cycle, so accepting it here
     * would silently discard the value. When mapped parameters reach chain
     * operators, they land as new push constant fields rather than as writes to
     * this one.
     */
    void set_parameter(std::string_view param, double value) override;

    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "GpuFieldOperator";
    }

private:
    /**
     * @struct Binding
     * @brief One target and the field driving it.
     */
    struct Binding {
        FieldTarget targets;
        Kinesis::FieldSource source;
        uint32_t components;
    };

    Kakshya::VertexLayout m_layout;
    uint32_t m_stride_words {};
    uint32_t m_vertex_binding {};
    uint32_t m_workgroup_size { 256 };
    std::vector<Binding> m_bindings;
    FieldMode m_mode { FieldMode::ACCUMULATE };

    float m_dt { 0.016F };
    std::vector<uint8_t> m_push_constants;

    mutable std::optional<Portal::Graphics::ShaderSpec> m_spec_cache;

    void invalidate();

    /**
     * @brief Locate a target's word offset and component count in the layout.
     * @return Nullopt when the layout carries no attribute of that modality.
     */
    [[nodiscard]] std::optional<std::pair<uint32_t, uint32_t>>
    resolve_target(FieldTarget target) const;

    /**
     * @brief Shared validation for the three bind overloads.
     */
    bool accept(FieldTarget target, const Kinesis::FieldSource& source,
        uint32_t components);
};

} // namespace MayaFlux::Nodes::Network
