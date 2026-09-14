#include "AssemblyOperator.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

std::shared_ptr<GpuSync::GeometryLeafNode> AssemblyOperator::add_geometry(
    std::vector<glm::vec3> positions,
    Portal::Graphics::PrimitiveTopology topology)
{
    auto node = std::make_shared<GpuSync::GeometryLeafNode>(std::move(positions));
    node->set_primitive_topology(topology);
    node->compute_frame();
    m_items.push_back(node);
    return node;
}

std::shared_ptr<GpuSync::GeometryLeafNode> AssemblyOperator::add_geometry(
    std::vector<Kakshya::Vertex> vertices,
    Portal::Graphics::PrimitiveTopology topology)
{
    auto node = std::make_shared<GpuSync::GeometryLeafNode>(std::move(vertices));
    node->set_primitive_topology(topology);
    node->compute_frame();
    m_items.push_back(node);
    return node;
}

std::shared_ptr<GpuSync::MeshWriterNode> AssemblyOperator::add_geometry(const Kakshya::MeshData& mesh)
{
    auto node = std::make_shared<GpuSync::MeshWriterNode>();
    node->set_mesh(mesh);
    node->compute_frame();
    m_items.push_back(node);
    return node;
}

void AssemblyOperator::add_geometry(std::shared_ptr<GpuSync::GeometryWriterNode> node)
{
    if (!node) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "AssemblyOperator::add_geometry: null node ignored");
        return;
    }
    node->compute_frame();
    m_items.push_back(std::move(node));
}

bool AssemblyOperator::remove_geometry(const std::shared_ptr<GpuSync::GeometryWriterNode>& node)
{
    auto it = std::find(m_items.begin(), m_items.end(), node);
    if (it == m_items.end()) {
        return false;
    }
    m_items.erase(it);
    return true;
}

void AssemblyOperator::process(float /*dt*/)
{
    for (auto& item : m_items) {
        item->compute_frame();
    }
}

std::span<const uint8_t> AssemblyOperator::get_vertex_data_for_collection(uint32_t idx) const
{
    if (idx >= m_items.size()) {
        return {};
    }
    return m_items[idx]->get_vertex_data();
}

std::span<const uint8_t> AssemblyOperator::get_vertex_data() const
{
    m_vertex_data_aggregate.clear();
    for (const auto& item : m_items) {
        auto span = item->get_vertex_data();
        m_vertex_data_aggregate.insert(
            m_vertex_data_aggregate.end(),
            span.begin(), span.end());
    }
    return { m_vertex_data_aggregate.data(), m_vertex_data_aggregate.size() };
}

Kakshya::VertexLayout AssemblyOperator::get_vertex_layout() const
{
    if (m_items.empty()) {
        return Kakshya::VertexLayout::for_points(sizeof(Kakshya::Vertex));
    }

    auto layout_opt = m_items[0]->get_vertex_layout();
    if (!layout_opt) {
        return Kakshya::VertexLayout::for_points(sizeof(Kakshya::Vertex));
    }

    auto layout = *layout_opt;
    layout.vertex_count = static_cast<uint32_t>(get_vertex_count());
    return layout;
}

size_t AssemblyOperator::get_vertex_count() const
{
    size_t total = 0;
    for (const auto& item : m_items) {
        total += item->get_vertex_count();
    }
    return total;
}

bool AssemblyOperator::is_vertex_data_dirty() const
{
    return std::ranges::any_of(
        m_items,
        [](const auto& item) { return item->needs_gpu_update(); });
}

void AssemblyOperator::mark_vertex_data_clean()
{
    for (auto& item : m_items) {
        item->clear_gpu_update_flag();
    }
}

std::vector<GraphicsOperator::DirtyVertexRange> AssemblyOperator::dirty_vertex_ranges() const
{
    std::vector<DirtyVertexRange> ranges;

    uint32_t offset = 0;
    uint32_t index = 0;
    for (const auto& item : m_items) {
        const auto count = static_cast<uint32_t>(item->get_vertex_count());
        if (item->needs_gpu_update()) {
            ranges.push_back(DirtyVertexRange {
                .group_index = index,
                .vertex_offset = offset,
                .vertex_count = count });
        }
        offset += count;
        ++index;
    }

    return ranges;
}

size_t AssemblyOperator::get_point_count() const
{
    return get_vertex_count();
}

std::vector<uint32_t> AssemblyOperator::build_cluster_ids() const
{
    std::vector<uint32_t> ids(get_vertex_count(), 0U);

    if (m_items.size() <= 1) {
        return ids;
    }

    size_t offset = 0;
    uint32_t cluster = 0;
    for (const auto& item : m_items) {
        const size_t count = item->get_vertex_count();
        for (size_t i = 0; i < count && offset + i < ids.size(); ++i) {
            ids[offset + i] = cluster;
        }
        offset += count;
        ++cluster;
    }

    return ids;
}

std::optional<Portal::Graphics::PrimitiveTopology> AssemblyOperator::declared_topology() const
{
    if (m_items.empty()) {
        return std::nullopt;
    }
    return m_items[0]->get_primitive_topology();
}

std::vector<DrawRun> AssemblyOperator::topology_runs() const
{
    std::vector<DrawRun> runs;
    uint32_t offset = 0;

    for (const auto& item : m_items) {
        const auto count = static_cast<uint32_t>(item->get_vertex_count());
        const auto topo = item->get_primitive_topology();

        if (!runs.empty() && runs.back().topology == topo && is_concatenable_topology(topo)) {
            runs.back().vertex_count += count;
        } else {
            runs.push_back(DrawRun { .topology = topo, .vertex_offset = offset, .vertex_count = count });
        }

        offset += count;
    }

    return runs;
}

void AssemblyOperator::set_parameter(std::string_view /*param*/, double /*value*/)
{
}

std::optional<double> AssemblyOperator::query_state(std::string_view query) const
{
    if (query == "item_count") {
        return static_cast<double>(m_items.size());
    }
    if (query == "vertex_count") {
        return static_cast<double>(get_vertex_count());
    }
    return std::nullopt;
}

void* AssemblyOperator::get_data_at(size_t /*global_index*/)
{
    return nullptr;
}

} // namespace MayaFlux::Nodes::Network
