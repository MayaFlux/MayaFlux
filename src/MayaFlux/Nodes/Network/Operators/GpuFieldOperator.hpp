#pragma once

#include "NetworkOperator.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Kinesis/Tendency/DualField.hpp"
#include "MayaFlux/Kinesis/Tendency/FieldBinding.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Nodes::Network {

using FieldTarget = Kinesis::FieldTarget;

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

 * Consumers compile a pipeline from build_spec() and hold it. revision()
 * increments whenever that spec changes, so a consumer records the revision it
 * built against and rebuilds when it differs. Rebinding is authoring-time work
 * and is not synchronised against a running process_batch, matching
 * OperatorChain's contract.
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

    /**
     * @brief Monotonic counter incremented whenever build_spec()'s result changes.
     *
     * A consumer that compiles a pipeline from build_spec() records the
     * revision it built against and rebuilds when it differs. A bool would not
     * survive two consumers, since the first to observe it would clear it.
     */
    [[nodiscard]] uint64_t revision() const noexcept { return m_revision; }

    // -------------------------------------------------------------------------
    // Shader
    // -------------------------------------------------------------------------

    /**
     * @brief Assemble the compute spec for the current bindings.
     *
     * Emits one GLSL function per distinct bound field, then a kernel that
     * guards against the dispatch tail, reads the position once, and writes
     * every touched target in a single pass. Fields sharing a target sum
     * before the write; NORMAL and TANGENT normalise after the sum. POSITION
     * accumulates onto the existing vertex, every other target is assigned,
     * matching FieldOperator.
     *
     * The kernel addresses a range rather than the whole buffer, since a
     * NetworkGeometryBuffer aggregates one slice per producing operator. The
     * range and the record stride arrive as push constants written by the
     * processor, so neither the caller nor the field author offsets anything,
     * and a layout or slice change needs no recompile. Attribute offsets are
     * baked, since those describe the record the spec was built against.
     *
     * Cached until a bind, unbind or configuration change invalidates it.
     * Returns nullopt when nothing is bound or the layout carries no position
     * attribute.
     */
    [[nodiscard]] std::optional<Portal::Graphics::ShaderSpec> build_spec() const;

    /**
     * @brief Layout the emitted shader addresses.
     */
    [[nodiscard]] const Kakshya::VertexLayout& get_layout() const { return m_layout; }

    // -------------------------------------------------------------------------
    // NetworkOperator interface
    // -------------------------------------------------------------------------

    /**
     * @brief No per-cycle work.
     *
     * The operator holds only declarations. Dispatch, push constants and the
     * vertex range all belong to the VertexFieldProcessor that consumes
     * build_spec(). Present because NetworkOperator requires it.
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
    uint64_t m_revision {};

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
