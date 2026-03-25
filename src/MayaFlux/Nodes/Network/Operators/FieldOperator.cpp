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

void FieldOperator::initialize(const std::vector<PointVertex>& vertices)
{
    m_vertex_type = VertexType::POINT;
    m_reference_positions.clear();
    m_reference_positions.reserve(vertices.size());

    m_vertex_data.resize(vertices.size() * k_stride);
    std::memcpy(m_vertex_data.data(), vertices.data(), vertices.size() * k_stride);

    for (const auto& v : vertices) {
        m_reference_positions.push_back(v.position);
    }

    m_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "FieldOperator initialized with {} PointVertex", vertices.size());
}

void FieldOperator::initialize(const std::vector<LineVertex>& vertices)
{
    m_vertex_type = VertexType::LINE;
    m_reference_positions.clear();
    m_reference_positions.reserve(vertices.size());

    m_vertex_data.resize(vertices.size() * k_stride);
    std::memcpy(m_vertex_data.data(), vertices.data(), vertices.size() * k_stride);

    for (const auto& v : vertices) {
        m_reference_positions.push_back(v.position);
    }

    m_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "FieldOperator initialized with {} LineVertex", vertices.size());
}

// =========================================================================
// Processing
// =========================================================================

void FieldOperator::process(float)
{
    if (m_vertex_data.empty())
        return;

    size_t count = m_reference_positions.size();

    if (m_mode == FieldMode::ABSOLUTE) {
        for (size_t i = 0; i < count; ++i) {
            position_at(i) = m_reference_positions[i];
        }
    }

    for (size_t i = 0; i < count; ++i) {
        glm::vec3 pos = position_at(i);

        for (const auto& field : m_position_fields) {
            pos += field(position_at(i));
        }
        position_at(i) = pos;

        for (const auto& field : m_scalar_fields) {
            scalar_at(i) = field(position_at(i));
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
    case FieldTarget::NORMAL:
    case FieldTarget::TANGENT:
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "VectorField binding for {:d} not yet implemented",
            static_cast<int>(target));
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
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "SpatialField binding for UV not yet implemented");
        break;
    default:
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot bind SpatialField to vector target {:d}",
            static_cast<int>(target));
        break;
    }
}

void FieldOperator::unbind(FieldTarget target)
{
    switch (target) {
    case FieldTarget::POSITION:
        m_position_fields.clear();
        break;
    case FieldTarget::SCALAR:
        m_scalar_fields.clear();
        break;
    default:
        break;
    }
}

void FieldOperator::unbind_all()
{
    m_position_fields.clear();
    m_scalar_fields.clear();
}

// =========================================================================
// Vertex accessors
// =========================================================================

glm::vec3& FieldOperator::position_at(size_t i)
{
    return *reinterpret_cast<glm::vec3*>(m_vertex_data.data() + i * k_stride + k_position_offset);
}

glm::vec3& FieldOperator::color_at(size_t i)
{
    return *reinterpret_cast<glm::vec3*>(m_vertex_data.data() + i * k_stride + k_color_offset);
}

float& FieldOperator::scalar_at(size_t i)
{
    return *reinterpret_cast<float*>(m_vertex_data.data() + i * k_stride + k_scalar_offset);
}

glm::vec3& FieldOperator::normal_at(size_t i)
{
    return *reinterpret_cast<glm::vec3*>(m_vertex_data.data() + i * k_stride + k_normal_offset);
}

glm::vec3& FieldOperator::tangent_at(size_t i)
{
    return *reinterpret_cast<glm::vec3*>(m_vertex_data.data() + i * k_stride + k_tangent_offset);
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
    layout.vertex_count = static_cast<uint32_t>(m_reference_positions.size());
    return layout;
}

size_t FieldOperator::get_vertex_count() const
{
    return m_reference_positions.size();
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
    return m_reference_positions.size();
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
    size_t count = m_reference_positions.size();
    std::vector<PointVertex> result(count);
    std::memcpy(result.data(), m_vertex_data.data(), count * k_stride);
    return result;
}

std::vector<LineVertex> FieldOperator::extract_line_vertices() const
{
    size_t count = m_reference_positions.size();
    std::vector<LineVertex> result(count);
    std::memcpy(result.data(), m_vertex_data.data(), count * k_stride);
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
        return static_cast<double>(get_point_count());
    }
    if (query == "field_count") {
        return static_cast<double>(m_position_fields.size() + m_scalar_fields.size());
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
    if (global_index >= m_reference_positions.size())
        return nullptr;
    return m_vertex_data.data() + global_index * k_stride;
}

} // namespace MayaFlux::Nodes::Network
