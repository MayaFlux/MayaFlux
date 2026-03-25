#pragma once

#include "GraphicsOperator.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @enum FieldTarget
 * @brief Vertex attribute targets for Tendency field evaluation
 */
enum class FieldTarget : uint8_t {
    POSITION,
    COLOR,
    NORMAL,
    TANGENT,
    SCALAR,
    UV
};

/**
 * @enum FieldMode
 * @brief How displacement fields are applied each frame
 */
enum class FieldMode : uint8_t {
    ABSOLUTE,
    ACCUMULATE
};

/**
 * @class FieldOperator
 * @brief Pure field-driven vertex manipulation via Tendency evaluation
 *
 * No velocity, no mass, no integration. Each frame, evaluates bound
 * Tendency fields at each vertex position and writes results into
 * the targeted vertex attributes.
 *
 * ABSOLUTE mode: resets to reference positions before evaluation.
 * Deterministic, stateless, frame-rate independent.
 *
 * ACCUMULATE mode: applies displacement on top of current positions.
 * Produces drift and evolution, frame-rate dependent.
 *
 * Works with both PointVertex and LineVertex (identical 60-byte layout).
 * Vertex type is determined at initialize() time.
 *
 * Usage with ParticleNetwork:
 * @code
 * auto field_op = particles->create_operator<FieldOperator>();
 * field_op->bind(FieldTarget::POSITION, Kinesis::VectorField { ... });
 * field_op->bind(FieldTarget::SCALAR, Kinesis::SpatialField { ... });
 * @endcode
 *
 * Usage with PointCloudNetwork:
 * @code
 * auto field_op = cloud->create_operator<FieldOperator>();
 * field_op->bind(FieldTarget::POSITION, Kinesis::VectorField { ... });
 * @endcode
 */
class MAYAFLUX_API FieldOperator : public GraphicsOperator {
public:
    explicit FieldOperator(FieldMode mode = FieldMode::ABSOLUTE);

    /**
     * @brief Initialize from PointVertex data
     * @param vertices Source vertices (positions stored as reference frame)
     */
    void initialize(const std::vector<PointVertex>& vertices);

    /**
     * @brief Initialize from LineVertex data
     * @param vertices Source vertices (positions stored as reference frame)
     */
    void initialize(const std::vector<LineVertex>& vertices);

    void process(float dt) override;

    // -----------------------------------------------------------------
    // Field binding
    // -----------------------------------------------------------------

    /**
     * @brief Bind a VectorField to POSITION target (displacement)
     * @param target Must be POSITION, COLOR, NORMAL, or TANGENT
     * @param field VectorField: glm::vec3 -> glm::vec3
     */
    void bind(FieldTarget target, Kinesis::VectorField field);

    /**
     * @brief Bind a SpatialField to SCALAR target (size/thickness modulation)
     * @param target Must be SCALAR
     * @param field SpatialField: glm::vec3 -> float
     */
    void bind(FieldTarget target, Kinesis::SpatialField field);

    /**
     * @brief Remove all fields bound to a target
     * @param target Target to clear
     */
    void unbind(FieldTarget target);

    /**
     * @brief Remove all bound fields
     */
    void unbind_all();

    /**
     * @brief Set field application mode
     * @param mode ABSOLUTE (reset each frame) or ACCUMULATE (stack displacements)
     */
    void set_mode(FieldMode mode) { m_mode = mode; }

    /**
     * @brief Get current field mode
     */
    [[nodiscard]] FieldMode get_mode() const { return m_mode; }

    // -----------------------------------------------------------------
    // GraphicsOperator interface
    // -----------------------------------------------------------------

    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;
    [[nodiscard]] std::vector<PointVertex> extract_point_vertices() const;
    [[nodiscard]] std::vector<LineVertex> extract_line_vertices() const;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Field"; }
    [[nodiscard]] size_t get_point_count() const override;
    [[nodiscard]] const char* get_vertex_type_name() const override;

    void apply_one_to_one(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source) override;

protected:
    void* get_data_at(size_t global_index) override;

private:
    /**
     * @enum VertexType
     * @brief Tracks which vertex type was used at initialization
     */
    enum class VertexType : uint8_t { NONE,
        POINT,
        LINE };

    FieldMode m_mode;
    VertexType m_vertex_type { VertexType::NONE };

    std::vector<glm::vec3> m_reference_positions;
    std::vector<uint8_t> m_vertex_data;
    bool m_dirty { false };

    std::vector<Kinesis::VectorField> m_position_fields;
    std::vector<Kinesis::SpatialField> m_scalar_fields;

    static constexpr size_t k_stride = 60;
    static constexpr size_t k_position_offset = 0;
    static constexpr size_t k_color_offset = 12;
    static constexpr size_t k_scalar_offset = 24;
    static constexpr size_t k_uv_offset = 28;
    static constexpr size_t k_normal_offset = 36;
    static constexpr size_t k_tangent_offset = 48;

    glm::vec3& position_at(size_t i);
    glm::vec3& color_at(size_t i);
    float& scalar_at(size_t i);
    glm::vec3& normal_at(size_t i);
    glm::vec3& tangent_at(size_t i);
};

} // namespace MayaFlux::Nodes::Network
