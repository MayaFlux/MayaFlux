#include "MeshAssemblyInsertion.hpp"

namespace MayaFlux::Kakshya {

MeshAssemblyInsertion::MeshAssemblyInsertion(MeshAssemblyContainer& target)
    : m_target(&target)
{
}

size_t MeshAssemblyInsertion::add_mesh(MeshData geometry, std::string name,
    std::optional<Kinesis::TimeSampling> time, TopologyVariance variance)
{
    return m_target->add_source({ .geometry = std::move(geometry), .name = std::move(name), .time = std::move(time), .variance = variance });
}

size_t MeshAssemblyInsertion::add_points(SpatialData geometry, std::string name,
    std::optional<Kinesis::TimeSampling> time, TopologyVariance variance)
{
    return m_target->add_source({ .geometry = std::move(geometry), .name = std::move(name), .time = std::move(time), .variance = variance });
}

std::optional<uint64_t> MeshAssemblyInsertion::add_node(
    std::string name, std::optional<uint64_t> parent, const glm::mat4& local)
{
    return m_target->add_placement({ .name = std::move(name), .parent = parent, .local = local });
}

std::optional<uint64_t> MeshAssemblyInsertion::add_instance(std::string name, uint64_t source,
    std::optional<uint64_t> parent, const glm::mat4& local, std::optional<uint64_t> material)
{
    return m_target->add_placement({ .name = std::move(name), .parent = parent, .source = source, .material = material, .local = local });
}

std::vector<uint64_t> MeshAssemblyInsertion::instance(uint64_t source, std::span<const glm::mat4> placements,
    std::optional<uint64_t> parent, std::string_view name_stem, std::optional<uint64_t> material)
{
    const auto& data = m_target->get_data();
    const bool parent_ok = !parent || *parent < m_target->get_total_elements();
    const bool source_ok = source < data.sources.size();
    const bool material_ok = !material || *material < data.materials.size();
    if (!parent_ok || !source_ok || !material_ok) {
        return {};
    }

    std::vector<uint64_t> rows;
    rows.reserve(placements.size());
    for (size_t i = 0; i < placements.size(); ++i) {
        const auto row = add_instance(std::string(name_stem) + "_" + std::to_string(i), source, parent, placements[i], material);
        if (row) {
            rows.push_back(*row);
        }
    }
    return rows;
}

std::optional<MeshAssemblyGraft> MeshAssemblyInsertion::graft(const MeshAssemblyContainer& other,
    std::optional<uint64_t> parent, const glm::mat4& local, std::string_view name)
{
    if (parent && *parent >= m_target->get_total_elements()) {
        return std::nullopt;
    }

    MeshAssemblyGraft result;
    const auto& data = other.get_data();

    for (const auto& material : data.materials) {
        result.materials.push_back(m_target->add_material(material));
    }

    for (size_t i = 0; i < data.sources.size(); ++i) {
        const auto index = m_target->add_source(data.sources[i]);
        result.sources.push_back(index);
        if (const auto frames = other.mesh_frames(i)) {
            std::ignore = m_target->set_mesh_frames(index, frames);
        }
        if (const auto frames = other.spatial_frames(i)) {
            std::ignore = m_target->set_spatial_frames(index, frames, other.spatial_max_vertices(i));
        }
    }

    std::optional<uint64_t> holder = parent;
    if (!name.empty()) {
        result.joint = m_target->add_placement({ .name = std::string(name), .parent = parent, .local = local });
        holder = result.joint;
    }

    const size_t rows = other.get_total_elements();
    result.rows.reserve(rows);
    for (size_t row = 0; row < rows; ++row) {
        auto placement = other.placement(row).value();
        if (placement.parent) {
            placement.parent = result.rows[*placement.parent];
        } else {
            placement.parent = holder;
            if (name.empty()) {
                placement.local = local * placement.local;
            }
        }
        if (placement.source) {
            placement.source = result.sources[*placement.source];
        }
        if (placement.material) {
            placement.material = result.materials[*placement.material];
        }
        result.rows.push_back(m_target->add_placement(placement).value());
    }

    for (const auto& motion : data.motions) {
        MeshAssemblyMotion moved = motion;
        for (auto& channel : moved.channels) {
            channel.row = result.rows[channel.row];
        }
        result.motions.push_back(m_target->add_motion(std::move(moved)).value());
    }
    return result;
}

} // namespace MayaFlux::Kakshya
