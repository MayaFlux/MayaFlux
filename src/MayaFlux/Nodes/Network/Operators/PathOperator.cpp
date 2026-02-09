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

void PathOperator::initialize(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors)
{
    if (positions.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot initialize PathOperator with zero positions");
        return;
    }

    glm::vec3 color_tint = colors.empty() ? glm::vec3(1.0F) : colors[0];
    add_path(positions, m_default_mode, color_tint, 1.0F);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PathOperator initialized with {} control points in 1 path",
        positions.size());
}

void PathOperator::initialize_paths(
    const std::vector<std::vector<glm::vec3>>& paths,
    Kinesis::InterpolationMode mode,
    const std::vector<glm::vec3>& color_tints,
    const std::vector<float>& thickness_scales)
{
    for (size_t i = 0; i < paths.size(); ++i) {
        glm::vec3 color_tint = (i < color_tints.size()) ? color_tints[i] : glm::vec3(1.0F);
        float thickness_scale = (i < thickness_scales.size()) ? thickness_scales[i] : 1.0F;
        add_path(paths[i], mode, color_tint, thickness_scale);
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "PathOperator initialized with {} paths",
        paths.size());
}

//-----------------------------------------------------------------------------
// Advanced Initialization (Multiple Paths)
//-----------------------------------------------------------------------------

void PathOperator::add_path(
    const std::vector<glm::vec3>& control_points,
    Kinesis::InterpolationMode mode,
    glm::vec3 color_tint,
    float thickness_scale)
{
    if (control_points.empty()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot add path with zero control points");
        return;
    }

    PathCollection path;
    path.generator = std::make_shared<GpuSync::PathGeneratorNode>(
        mode,
        m_default_samples_per_segment,
        1024);
    path.control_points = control_points;
    path.color_tint = color_tint;
    path.thickness_scale = thickness_scale;

    path.generator->set_control_points(control_points);
    path.generator->set_path_color(color_tint);
    path.generator->set_path_thickness(m_default_thickness * thickness_scale);
    path.generator->compute_frame();

    m_paths.push_back(std::move(path));

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Added path #{} with {} control points, {} generated vertices",
        m_paths.size(), control_points.size(),
        m_paths.back().generator->get_generated_vertex_count());
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
        path.generator->compute_frame();
    }
}

//-----------------------------------------------------------------------------
// GraphicsOperator Interface (Data Extraction)
//-----------------------------------------------------------------------------

std::vector<glm::vec3> PathOperator::extract_positions() const
{
    std::vector<glm::vec3> positions;

    for (const auto& path : m_paths) {
        positions.insert(positions.end(),
            path.control_points.begin(),
            path.control_points.end());
    }

    return positions;
}

std::vector<glm::vec3> PathOperator::extract_colors() const
{
    std::vector<glm::vec3> colors;

    for (const auto& path : m_paths) {
        colors.insert(colors.end(),
            path.control_points.size(),
            path.color_tint);
    }

    return colors;
}

std::span<const uint8_t> PathOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (m_paths.empty() || idx >= m_paths.size()) {
        return {};
    }
    return m_paths[idx].generator->get_vertex_data();
}

std::span<const uint8_t> PathOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& group : m_paths) {
        auto span = group.generator->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

const Kakshya::VertexLayout& PathOperator::get_vertex_layout() const
{
    if (m_paths.empty()) {
        static Kakshya::VertexLayout empty_layout;
        return empty_layout;
    }
    return *m_paths[0].generator->get_vertex_layout();
}

size_t PathOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& path : m_paths) {
        total += path.generator->get_vertex_count();
    }
    return total;
}

bool PathOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_paths,
        [](const auto& group) { return group.generator->needs_gpu_update(); });
}

void PathOperator::mark_vertex_data_clean()
{
    for (auto& path : m_paths) {
        path.generator->clear_gpu_update_flag();
    }
}

size_t PathOperator::get_point_count() const
{
    size_t total = 0;
    for (const auto& path : m_paths) {
        total += path.control_points.size();
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
            path.generator->set_tension(value);
        }
    } else if (param == "samples_per_segment") {
        m_default_samples_per_segment = static_cast<Eigen::Index>(value);
        for (auto& path : m_paths) {
            path.generator->set_samples_per_segment(static_cast<Eigen::Index>(value));
        }
    } else if (param == "thickness") {
        m_default_thickness = static_cast<float>(value);
        for (auto& path : m_paths) {
            path.generator->set_path_thickness(m_default_thickness * path.thickness_scale);
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
        path.generator->set_samples_per_segment(samples);
    }
}

void PathOperator::set_tension(double tension)
{
    for (auto& path : m_paths) {
        path.generator->set_tension(tension);
    }
}

void PathOperator::set_global_thickness(float thickness)
{
    m_default_thickness = thickness;
    for (auto& path : m_paths) {
        path.generator->set_path_thickness(thickness * path.thickness_scale);
    }
}

void PathOperator::set_global_color(const glm::vec3& color)
{
    for (auto& path : m_paths) {
        path.color_tint = color;
        path.generator->set_path_color(color);
    }
}

} // namespace MayaFlux::Nodes::Network
