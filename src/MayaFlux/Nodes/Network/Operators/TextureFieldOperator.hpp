#pragma once

#include "GraphicsOperator.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @enum UVMode
 * @brief How UV fields are applied each frame
 */
enum class UVMode : uint8_t {
    ABSOLUTE, ///< Recompute UV from reference position each frame — deterministic
    ACCUMULATE ///< Add displacement to current UV each frame — produces drift
};

/**
 * @class TextureFieldOperator
 * @brief CPU-side UV generation via UVField tendency evaluation
 *
 * Evaluates one or more UVField tendencies (glm::vec3 -> glm::vec2) at each
 * vertex position and writes the accumulated result into the UV attribute
 * (byte offset 28) of the 60-byte PointVertex / LineVertex layout.
 *
 * This is the CPU path for UV generation. For GPU-side texture sampling
 * combined with UV projection, use NetworkTextureBuffer which adds a
 * UVFieldProcessor compute pass on top of the geometry upload.
 *
 * Works identically with PointVertex and LineVertex — both share the same
 * 60-byte layout and UV offset.
 *
 * ABSOLUTE mode: restores reference vertex state before evaluation.
 * Deterministic, stateless, frame-rate independent.
 *
 * ACCUMULATE mode: applies on top of current UV state. Produces animated
 * UV drift, frame-rate dependent.
 *
 * Multiple UVFields are accumulated by addition before writing.
 *
 * Usage:
 * @code
 * auto uv_op = cloud->create_operator<TextureFieldOperator>();
 * uv_op->bind(Kinesis::UVProjection::cylindrical(
 *     glm::vec3(0.0F, 1.0F, 0.0F),
 *     glm::vec3(0.0F),
 *     1.0F, 2.0F));
 * @endcode
 */
class MAYAFLUX_API TextureFieldOperator : public GraphicsOperator {
public:
    explicit TextureFieldOperator(UVMode mode = UVMode::ABSOLUTE);

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

    // -------------------------------------------------------------------------
    // Field binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a UVField for evaluation
     * @param field UVField: glm::vec3 -> glm::vec2
     *
     * Multiple fields accumulate additively. Evaluation order matches
     * bind order.
     */
    void bind(Kinesis::UVField field);

    /**
     * @brief Remove all bound UV fields
     */
    void unbind_all();

    /**
     * @brief Set UV application mode
     * @param mode ABSOLUTE or ACCUMULATE
     */
    void set_mode(UVMode mode) { m_mode = mode; }

    /**
     * @brief Get current UV mode
     */
    [[nodiscard]] UVMode get_mode() const { return m_mode; }

    // -------------------------------------------------------------------------
    // GraphicsOperator interface
    // -------------------------------------------------------------------------

    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    /**
     * @brief Extract current vertex data as PointVertex array
     */
    [[nodiscard]] std::vector<PointVertex> extract_point_vertices() const;

    /**
     * @brief Extract current vertex data as LineVertex array
     */
    [[nodiscard]] std::vector<LineVertex> extract_line_vertices() const;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "TextureField"; }
    [[nodiscard]] size_t get_point_count() const override;
    [[nodiscard]] const char* get_vertex_type_name() const override;

    void apply_one_to_one(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source) override;

protected:
    void* get_data_at(size_t global_index) override;

private:
    enum class VertexType : uint8_t { NONE,
        POINT,
        LINE };

    UVMode m_mode;
    VertexType m_vertex_type { VertexType::NONE };
    size_t m_count { 0 };

    std::vector<uint8_t> m_reference_data;
    std::vector<uint8_t> m_vertex_data;
    bool m_dirty { false };

    std::vector<Kinesis::UVField> m_uv_fields;

    static constexpr size_t k_stride = 60;
    static constexpr size_t k_position_offset = 0;
    static constexpr size_t k_uv_offset = 28;

    [[nodiscard]] glm::vec3 position_at(size_t i) const;
    [[nodiscard]] glm::vec2& uv_at(size_t i);

    void store_reference(const void* data, size_t count);
};

} // namespace MayaFlux::Nodes::Network
