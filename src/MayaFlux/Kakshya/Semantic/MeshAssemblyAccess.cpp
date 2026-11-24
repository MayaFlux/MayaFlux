#include "MeshAssemblyAccess.hpp"

namespace MayaFlux::Kakshya {

MeshAssemblyAccess::MeshAssemblyAccess(const MeshAssemblyContainer& assembly)
    : m_assembly(&assembly)
{
    const size_t rows = assembly.get_total_elements();
    m_parent.resize(rows);
    m_children.resize(rows);
    m_names.reserve(rows);

    for (size_t row = 0; row < rows; ++row) {
        const auto placement = assembly.placement(row);
        m_names.push_back(placement ? placement->name : std::string {});
        m_parent[row] = placement ? placement->parent : std::nullopt;
        if (m_parent[row]) {
            m_children[*m_parent[row]].push_back(row);
        } else {
            m_roots.push_back(row);
        }
    }
}

std::optional<MeshAssemblyRow> MeshAssemblyAccess::row(size_t row) const
{
    auto placement = m_assembly->placement(row);
    if (!placement) {
        return std::nullopt;
    }
    return MeshAssemblyRow { .index = row, .placement = std::move(*placement), .kind = row_kind(*m_assembly, row) };
}

std::span<const uint64_t> MeshAssemblyAccess::children(size_t row) const noexcept
{
    return row < m_children.size() ? std::span<const uint64_t>(m_children[row]) : std::span<const uint64_t> {};
}

std::vector<uint64_t> MeshAssemblyAccess::subtree(size_t row) const
{
    std::vector<uint64_t> found;
    if (row >= m_children.size()) {
        return found;
    }

    std::vector<uint64_t> pending { row };
    while (!pending.empty()) {
        const uint64_t next = pending.back();
        pending.pop_back();
        found.push_back(next);
        for (auto it = m_children[next].rbegin(); it != m_children[next].rend(); ++it) {
            pending.push_back(*it);
        }
    }
    return found;
}

std::vector<uint64_t> MeshAssemblyAccess::ancestors(size_t row) const
{
    std::vector<uint64_t> found;
    if (row >= m_parent.size()) {
        return found;
    }
    for (auto parent = m_parent[row]; parent; parent = m_parent[*parent]) {
        found.push_back(*parent);
    }
    return found;
}

size_t MeshAssemblyAccess::depth(size_t row) const
{
    return ancestors(row).size();
}

std::string MeshAssemblyAccess::path(size_t row) const
{
    if (row >= m_names.size()) {
        return {};
    }

    auto chain = ancestors(row);
    std::ranges::reverse(chain);
    chain.push_back(row);

    std::string joined;
    for (const uint64_t link : chain) {
        if (!joined.empty()) {
            joined += '/';
        }
        joined += m_names[link];
    }
    return joined;
}

std::optional<uint64_t> MeshAssemblyAccess::find(std::string_view name) const
{
    const auto it = std::ranges::find(m_names, name);
    return it == m_names.end() ? std::nullopt : std::optional<uint64_t> { static_cast<uint64_t>(it - m_names.begin()) };
}

std::optional<uint64_t> MeshAssemblyAccess::find_path(std::string_view wanted) const
{
    for (size_t row = 0; row < m_names.size(); ++row) {
        if (path(row) == wanted) {
            return row;
        }
    }
    return std::nullopt;
}

std::vector<uint64_t> MeshAssemblyAccess::find_all(std::string_view name) const
{
    std::vector<uint64_t> found;
    for (size_t row = 0; row < m_names.size(); ++row) {
        if (m_names[row] == name) {
            found.push_back(row);
        }
    }
    return found;
}

std::vector<uint64_t> MeshAssemblyAccess::rows_of_source(size_t source) const
{
    std::vector<uint64_t> found;
    for (size_t row = 0; row < m_names.size(); ++row) {
        if (source_of(row) == std::optional<uint64_t> { source }) {
            found.push_back(row);
        }
    }
    return found;
}

std::vector<uint64_t> MeshAssemblyAccess::rows_of_kind(MeshAssemblyRowKind kind) const
{
    std::vector<uint64_t> found;
    for (size_t row = 0; row < m_names.size(); ++row) {
        if (row_kind(*m_assembly, row) == kind) {
            found.push_back(row);
        }
    }
    return found;
}

std::optional<glm::mat4> MeshAssemblyAccess::world(size_t row) const
{
    if (row >= m_names.size()) {
        return std::nullopt;
    }

    glm::mat4 composed { 1.0F };
    auto chain = ancestors(row);
    std::ranges::reverse(chain);
    chain.push_back(row);
    for (const uint64_t link : chain) {
        composed *= m_assembly->placement(link)->local;
    }
    return composed;
}

const MeshAssemblySource* MeshAssemblyAccess::source(size_t source) const noexcept
{
    const auto& sources = m_assembly->get_data().sources;
    return source < sources.size() ? &sources[source] : nullptr;
}

const MeshAssemblyMaterial* MeshAssemblyAccess::material(size_t material) const noexcept
{
    const auto& materials = m_assembly->get_data().materials;
    return material < materials.size() ? &materials[material] : nullptr;
}

const MeshAssemblyMotion* MeshAssemblyAccess::motion(size_t motion) const noexcept
{
    const auto& motions = m_assembly->get_data().motions;
    return motion < motions.size() ? &motions[motion] : nullptr;
}

std::optional<MeshAccess> MeshAssemblyAccess::mesh(size_t source) const
{
    const auto* held = m_assembly->mesh(source);
    return held ? held->access() : std::nullopt;
}

std::optional<uint64_t> MeshAssemblyAccess::source_of(size_t row) const
{
    const auto placement = m_assembly->placement(row);
    return placement ? placement->source : std::nullopt;
}

const MeshAssemblyChannel* MeshAssemblyAccess::channel(size_t motion, size_t row) const noexcept
{
    const auto* held = this->motion(motion);
    if (!held) {
        return nullptr;
    }
    const auto it = std::ranges::find(held->channels, static_cast<uint64_t>(row), &MeshAssemblyChannel::row);
    return it == held->channels.end() ? nullptr : &*it;
}

bool MeshAssemblyAccess::animates(size_t source) const
{
    return m_assembly->mesh_frames(source) != nullptr || m_assembly->spatial_frames(source) != nullptr;
}

} // namespace MayaFlux::Kakshya
