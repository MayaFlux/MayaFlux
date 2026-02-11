#include "PathOperator.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

PathOperator::PathOperator(
    Kinesis::InterpolationMode mode,
    Eigen::Index samples_per_segment)
    : m_default_mode(mode)
    , m_default_samples_per_segment(samples_per_segment)
{
    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PathOperator created with mode {}, {} samples per segment",
        static_cast<int>(mode), samples_per_segment);
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Simple Initialization)
//-----------------------------------------------------------------------------

void PathOperator::initialize(const std::vector<LineVertex>& vertices)
{
    if (vertices.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot initialize PathOperator with zero vertices");
        return;
    }

    m_paths.clear();
    add_path(vertices, m_default_mode);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PathOperator initialized with {} control vertices", vertices.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Paths)
//-----------------------------------------------------------------------------

void PathOperator::initialize_paths(
    const std::vector<std::vector<LineVertex>>& paths,
    Kinesis::InterpolationMode mode)
{
    for (const auto& path : paths) {
        add_path(path, mode);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PathOperator initialized with {} paths",
        paths.size());
}

void PathOperator::add_path(
    const std::vector<LineVertex>& control_vertices,
    Kinesis::InterpolationMode mode)
{
    if (control_vertices.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot add path with zero control vertices");
        return;
    }

    auto path = std::make_shared<GpuSync::PathGeneratorNode>(
        mode,
        m_default_samples_per_segment,
        1024);

    path->set_control_points(control_vertices);
    path->set_path_thickness(m_default_thickness);
    path->compute_frame();

    m_paths.push_back(std::move(path));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added path #{} with {} control vertices, {} generated vertices",
        m_paths.size(), control_vertices.size(),
        m_paths.back()->get_generated_vertex_count());
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

void PathOperator::process(float /*dt*/)
{
    if (m_paths.empty()) {
        return;
    }

    for (auto& path : m_paths) {
        path->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Data Extraction)
//-----------------------------------------------------------------------------

std::vector<LineVertex> PathOperator::extract_vertices() const
{
    std::vector<LineVertex> positions;

    for (const auto& path : m_paths) {
        auto points = path->get_all_vertices();
        for (const auto& pt : points) {
            positions.push_back(pt);
        }
    }

    return positions;
}

std::span<const uint8_t> PathOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (m_paths.empty() || idx >= m_paths.size()) {
        return {};
    }
    return m_paths[idx]->get_vertex_data();
}

std::span<const uint8_t> PathOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& group : m_paths) {
        auto span = group->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

Kakshya::VertexLayout PathOperator::get_vertex_layout() const
{
    if (m_paths.empty()) {
        return {};
    }

    auto layout_opt = m_paths[0]->get_vertex_layout();
    if (!layout_opt) {
        return {};
    }

    return *layout_opt;
}

size_t PathOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& path : m_paths) {
        total += path->get_vertex_count();
    }
    return total;
}

bool PathOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_paths,
        [](const auto& group) { return group->needs_gpu_update(); });
}

void PathOperator::mark_vertex_data_clean()
{
    for (auto& path : m_paths) {
        path->clear_gpu_update_flag();
    }
}

size_t PathOperator::get_point_count() const
{
    size_t total = 0;
    for (const auto& path : m_paths) {
        total += path->get_all_vertex_count();
    }
    return total;
}

//-----------------------------------------------------------------------------
// Parameter Control
//-----------------------------------------------------------------------------

void PathOperator::set_parameter(std::string_view param, double value)
{
    if (param == "tension") {
        for (auto& path : m_paths) {
            path->set_tension(value);
        }
    } else if (param == "samples_per_segment") {
        m_default_samples_per_segment = static_cast<Eigen::Index>(value);
        for (auto& path : m_paths) {
            path->set_samples_per_segment(static_cast<Eigen::Index>(value));
        }
    } else if (param == "thickness") {
        m_default_thickness = static_cast<float>(value);
        for (auto& path : m_paths) {
            path->set_path_thickness(m_default_thickness);
        }
    }
}

std::optional<double> PathOperator::query_state(std::string_view query) const
{
    if (query == "control_point_count") {
        return static_cast<double>(get_point_count());
    }
    if (query == "vertex_count") {
        return static_cast<double>(get_vertex_count());
    }
    if (query == "path_count") {
        return static_cast<double>(m_paths.size());
    }
    return std::nullopt;
}

//-----------------------------------------------------------------------------
// Path Configuration
//-----------------------------------------------------------------------------

void PathOperator::set_samples_per_segment(Eigen::Index samples)
{
    m_default_samples_per_segment = samples;
    for (auto& path : m_paths) {
        path->set_samples_per_segment(samples);
    }
}

void PathOperator::set_tension(double tension)
{
    for (auto& path : m_paths) {
        path->set_tension(tension);
    }
}

void PathOperator::set_global_thickness(float thickness)
{
    m_default_thickness = thickness;
    for (auto& path : m_paths) {
        path->set_path_thickness(thickness);
    }
}

void PathOperator::set_global_color(const glm::vec3& color)
{
    for (auto& path : m_paths) {
        path->set_path_color(color);
    }
}

void* PathOperator::get_data_at(size_t global_index)
{
    size_t offset = 0;
    for (auto& group : m_paths) {
        const auto& vertices = group->get_all_vertices();
        if (global_index < offset + vertices.size()) {
            size_t local_index = global_index - offset;
            return const_cast<LineVertex*>(&vertices[local_index]);
        }
        offset += vertices.size();
    }
    return nullptr;
}

} // namespace MayaFlux::Nodes::Network
