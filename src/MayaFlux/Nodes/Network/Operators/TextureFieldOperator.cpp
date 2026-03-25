#include "TextureFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

TextureFieldOperator::TextureFieldOperator(UVMode mode)
    : m_mode(mode)
{
}

// =============================================================================
// Initialization
// =============================================================================

void TextureFieldOperator::store_reference(const void* data, size_t count)
{
    m_count = count;
    const size_t total = count * k_stride;
    m_reference_data.resize(total);
    m_vertex_data.resize(total);
    std::memcpy(m_reference_data.data(), data, total);
    std::memcpy(m_vertex_data.data(), data, total);
    m_dirty = true;
}

void TextureFieldOperator::initialize(const std::vector<PointVertex>& vertices)
{
    m_vertex_type = VertexType::POINT;
    store_reference(vertices.data(), vertices.size());

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TextureFieldOperator initialized with {} PointVertex", vertices.size());
}

void TextureFieldOperator::initialize(const std::vector<LineVertex>& vertices)
{
    m_vertex_type = VertexType::LINE;
    store_reference(vertices.data(), vertices.size());

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TextureFieldOperator initialized with {} LineVertex", vertices.size());
}

// =============================================================================
// Processing
// =============================================================================

void TextureFieldOperator::process(float)
{
    if (m_count == 0 || m_uv_fields.empty())
        return;

    if (m_mode == UVMode::ABSOLUTE)
        std::memcpy(m_vertex_data.data(), m_reference_data.data(), m_count * k_stride);

    for (size_t i = 0; i < m_count; ++i) {
        const glm::vec3 pos = position_at(i);

        glm::vec2 uv(0.0F);
        for (const auto& field : m_uv_fields)
            uv += field(pos);

        uv_at(i) = uv;
    }

    m_dirty = true;
}

// =============================================================================
// Field binding
// =============================================================================

void TextureFieldOperator::bind(Kinesis::UVField field)
{
    m_uv_fields.push_back(std::move(field));
}

void TextureFieldOperator::unbind_all()
{
    m_uv_fields.clear();
}

// =============================================================================
// Vertex accessors
// =============================================================================

glm::vec3 TextureFieldOperator::position_at(size_t i) const
{
    return *reinterpret_cast<const glm::vec3*>(
        m_reference_data.data() + i * k_stride + k_position_offset);
}

glm::vec2& TextureFieldOperator::uv_at(size_t i)
{
    return *reinterpret_cast<glm::vec2*>(
        m_vertex_data.data() + i * k_stride + k_uv_offset);
}

// =============================================================================
// GraphicsOperator interface
// =============================================================================

std::span<const uint8_t> TextureFieldOperator::get_vertex_data() const
{
    return { m_vertex_data.data(), m_vertex_data.size() };
}

std::span<const uint8_t> TextureFieldOperator::get_vertex_data_for_collection(uint32_t) const
{
    return get_vertex_data();
}

Kakshya::VertexLayout TextureFieldOperator::get_vertex_layout() const
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

size_t TextureFieldOperator::get_vertex_count() const { return m_count; }
size_t TextureFieldOperator::get_point_count() const { return m_count; }
bool TextureFieldOperator::is_vertex_data_dirty() const { return m_dirty; }
void TextureFieldOperator::mark_vertex_data_clean() { m_dirty = false; }

std::vector<PointVertex> TextureFieldOperator::extract_point_vertices() const
{
    std::vector<PointVertex> result(m_count);
    std::memcpy(result.data(), m_vertex_data.data(), m_count * k_stride);
    return result;
}

std::vector<LineVertex> TextureFieldOperator::extract_line_vertices() const
{
    std::vector<LineVertex> result(m_count);
    std::memcpy(result.data(), m_vertex_data.data(), m_count * k_stride);
    return result;
}

const char* TextureFieldOperator::get_vertex_type_name() const
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

void TextureFieldOperator::set_parameter(std::string_view param, double value)
{
    if (param == "mode")
        m_mode = value > 0.5 ? UVMode::ACCUMULATE : UVMode::ABSOLUTE;
}

std::optional<double> TextureFieldOperator::query_state(std::string_view query) const
{
    if (query == "point_count")
        return static_cast<double>(m_count);
    if (query == "field_count")
        return static_cast<double>(m_uv_fields.size());
    return std::nullopt;
}

void TextureFieldOperator::apply_one_to_one(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    GraphicsOperator::apply_one_to_one(param, source);
}

void* TextureFieldOperator::get_data_at(size_t global_index)
{
    if (global_index >= m_count)
        return nullptr;
    return m_vertex_data.data() + global_index * k_stride;
}

} // namespace MayaFlux::Nodes::Network
