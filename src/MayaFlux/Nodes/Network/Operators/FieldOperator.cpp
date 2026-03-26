#include "FieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

namespace MayaFlux::Nodes::Network {

FieldOperator::FieldOperator(FieldMode mode)
    : m_mode(mode)
{
}

// =========================================================================
// Initialization
// =========================================================================

void FieldOperator::store_reference(const void* data, size_t count)
{
    m_count = count;
    size_t total = count * k_stride;
    m_reference_data.resize(total);
    m_vertex_data.resize(total);
    std::memcpy(m_reference_data.data(), data, total);
    std::memcpy(m_vertex_data.data(), data, total);
    m_dirty = true;
}

void FieldOperator::initialize(const std::vector<PointVertex>& vertices)
{
    m_vertex_type = VertexType::POINT;
    store_reference(vertices.data(), vertices.size());

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "FieldOperator initialized with {} PointVertex", vertices.size());
}

void FieldOperator::initialize(const std::vector<LineVertex>& vertices)
{
    m_vertex_type = VertexType::LINE;
    store_reference(vertices.data(), vertices.size());

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "FieldOperator initialized with {} LineVertex", vertices.size());
}

// =========================================================================
// Processing
// =========================================================================

void FieldOperator::process(float)
{
    if (m_count == 0)
        return;

    if (m_mode == FieldMode::ABSOLUTE) {
        std::memcpy(m_vertex_data.data(), m_reference_data.data(), m_count * k_stride);
    }

    bool has_position = !m_position_fields.empty();
    bool has_color = !m_color_fields.empty();
    bool has_normal = !m_normal_fields.empty();
    bool has_tangent = !m_tangent_fields.empty();
    bool has_scalar = !m_scalar_fields.empty();
    bool has_uv = !m_uv_fields.empty();

    for (size_t i = 0; i < m_count; ++i) {
        glm::vec3& pos = vec3_at(i, k_position_offset);

        if (has_position) {
            glm::vec3 displacement(0.0F);
            for (const auto& field : m_position_fields) {
                displacement += field(pos);
            }
            pos += displacement;
        }

        if (has_color) {
            glm::vec3 color(0.0F);
            for (const auto& field : m_color_fields) {
                color += field(pos);
            }
            vec3_at(i, k_color_offset) = color;
        }

        if (has_normal) {
            glm::vec3 normal(0.0F);
            for (const auto& field : m_normal_fields) {
                normal += field(pos);
            }
            float len = glm::length(normal);
            vec3_at(i, k_normal_offset) = len > 1e-6F ? normal / len : glm::vec3(0.0F, 0.0F, 1.0F);
        }

        if (has_tangent) {
            glm::vec3 tangent(0.0F);
            for (const auto& field : m_tangent_fields) {
                tangent += field(pos);
            }
            float len = glm::length(tangent);
            vec3_at(i, k_tangent_offset) = len > 1e-6F ? tangent / len : glm::vec3(1.0F, 0.0F, 0.0F);
        }

        if (has_scalar) {
            float value = 0.0F;
            for (const auto& field : m_scalar_fields) {
                value += field(pos);
            }
            float_at(i, k_scalar_offset) = value;
        }

        if (has_uv) {
            const glm::vec3 ref = ref_position_at(i);
            glm::vec2 uv(0.0F);
            for (const auto& field : m_uv_fields)
                uv += field(ref);
            *reinterpret_cast<glm::vec2*>(
                m_vertex_data.data() + i * k_stride + k_uv_offset) = uv;
        }
    }

    m_dirty = true;
}

// =========================================================================
// Field binding
// =========================================================================

void FieldOperator::bind(FieldTarget target, Kinesis::VectorField field)
{
    switch (target) {
    case FieldTarget::POSITION:
        m_position_fields.push_back(std::move(field));
        break;
    case FieldTarget::COLOR:
        m_color_fields.push_back(std::move(field));
        break;
    case FieldTarget::NORMAL:
        m_normal_fields.push_back(std::move(field));
        break;
    case FieldTarget::TANGENT:
        m_tangent_fields.push_back(std::move(field));
        break;
    default:
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot bind VectorField to scalar target {:d}",
            static_cast<int>(target));
        break;
    }
}

void FieldOperator::bind(FieldTarget target, Kinesis::SpatialField field)
{
    switch (target) {
    case FieldTarget::SCALAR:
        m_scalar_fields.push_back(std::move(field));
        break;
    case FieldTarget::UV:
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "UV target requires a UVField, not a SpatialField. "
            "Use bind(FieldTarget::UV, Kinesis::UVField) instead.");
        break;
    default:
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot bind SpatialField to vector target {:d}",
            static_cast<int>(target));
        break;
    }
}

void FieldOperator::bind(FieldTarget target, Kinesis::UVField field)
{
    if (target != FieldTarget::UV) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "UVField can only be bound to FieldTarget::UV");
        return;
    }
    m_uv_fields.push_back(std::move(field));
}

void FieldOperator::unbind(FieldTarget target)
{
    switch (target) {
    case FieldTarget::POSITION:
        m_position_fields.clear();
        break;
    case FieldTarget::COLOR:
        m_color_fields.clear();
        break;
    case FieldTarget::NORMAL:
        m_normal_fields.clear();
        break;
    case FieldTarget::TANGENT:
        m_tangent_fields.clear();
        break;
    case FieldTarget::SCALAR:
        m_scalar_fields.clear();
        break;
    case FieldTarget::UV:
        m_uv_fields.clear();
        break;
    }
}

void FieldOperator::unbind_all()
{
    m_position_fields.clear();
    m_color_fields.clear();
    m_normal_fields.clear();
    m_tangent_fields.clear();
    m_scalar_fields.clear();
    m_uv_fields.clear();
}

// =========================================================================
// Vertex accessors
// =========================================================================

glm::vec3& FieldOperator::vec3_at(size_t i, size_t offset)
{
    return *reinterpret_cast<glm::vec3*>(m_vertex_data.data() + i * k_stride + offset);
}

float& FieldOperator::float_at(size_t i, size_t offset)
{
    return *reinterpret_cast<float*>(m_vertex_data.data() + i * k_stride + offset);
}

glm::vec3 FieldOperator::ref_position_at(size_t i) const
{
    return *reinterpret_cast<const glm::vec3*>(m_reference_data.data() + i * k_stride + k_position_offset);
}

// =========================================================================
// GraphicsOperator interface
// =========================================================================

std::span<const uint8_t> FieldOperator::get_vertex_data() const
{
    return { m_vertex_data.data(), m_vertex_data.size() };
}

std::span<const uint8_t> FieldOperator::get_vertex_data_for_collection(uint32_t) const
{
    return get_vertex_data();
}

Kakshya::VertexLayout FieldOperator::get_vertex_layout() const
{
    Kakshya::VertexLayout layout;
    switch (m_vertex_type) {
    case VertexType::POINT:
        layout = Kakshya::VertexLayout::for_points(k_stride);
        break;
    case VertexType::LINE:
        layout = Kakshya::VertexLayout::for_lines(k_stride);
        break;
    default:
        return {};
    }
    layout.vertex_count = static_cast<uint32_t>(m_count);
    return layout;
}

size_t FieldOperator::get_vertex_count() const
{
    return m_count;
}

bool FieldOperator::is_vertex_data_dirty() const
{
    return m_dirty;
}

void FieldOperator::mark_vertex_data_clean()
{
    m_dirty = false;
}

size_t FieldOperator::get_point_count() const
{
    return m_count;
}

const char* FieldOperator::get_vertex_type_name() const
{
    switch (m_vertex_type) {
    case VertexType::POINT:
        return "PointVertex";
    case VertexType::LINE:
        return "LineVertex";
    default:
        return "Unknown";
    }
}

std::vector<PointVertex> FieldOperator::extract_point_vertices() const
{
    std::vector<PointVertex> result(m_count);
    std::memcpy(result.data(), m_vertex_data.data(), m_count * k_stride);
    return result;
}

std::vector<LineVertex> FieldOperator::extract_line_vertices() const
{
    std::vector<LineVertex> result(m_count);
    std::memcpy(result.data(), m_vertex_data.data(), m_count * k_stride);
    return result;
}

void FieldOperator::set_parameter(std::string_view param, double value)
{
    if (param == "mode") {
        m_mode = value > 0.5 ? FieldMode::ACCUMULATE : FieldMode::ABSOLUTE;
    }
}

std::optional<double> FieldOperator::query_state(std::string_view query) const
{
    if (query == "point_count") {
        return static_cast<double>(m_count);
    }
    if (query == "field_count") {
        return static_cast<double>(
            m_position_fields.size() + m_color_fields.size()
            + m_normal_fields.size() + m_tangent_fields.size()
            + m_scalar_fields.size() + m_uv_fields.size());
    }
    return std::nullopt;
}

void FieldOperator::apply_one_to_one(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    GraphicsOperator::apply_one_to_one(param, source);
}

void* FieldOperator::get_data_at(size_t global_index)
{
    if (global_index >= m_count)
        return nullptr;
    return m_vertex_data.data() + global_index * k_stride;
}

} // namespace MayaFlux::Nodes::Network
