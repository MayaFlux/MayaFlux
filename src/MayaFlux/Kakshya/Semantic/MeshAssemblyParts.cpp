#include "MeshAssemblyParts.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Kakshya {

MeshAssemblyRowKind row_kind(const MeshAssemblyContainer& assembly, size_t row)
{
    const auto placement = assembly.placement(row);
    if (!placement || !placement->source) {
        return MeshAssemblyRowKind::Joint;
    }
    if (assembly.mesh(*placement->source)) {
        return MeshAssemblyRowKind::Mesh;
    }
    if (const auto* spatial = assembly.spatial(*placement->source)) {
        return spatial->topology == Portal::Graphics::PrimitiveTopology::POINT_LIST
            ? MeshAssemblyRowKind::Points
            : MeshAssemblyRowKind::Curves;
    }
    return MeshAssemblyRowKind::Joint;
}

std::vector<MeshAssemblyPart> partition_assembly(
    const MeshAssemblyContainer& assembly, const MeshAssemblyPartition& partition)
{
    const size_t total = assembly.get_total_elements();

    MeshAssemblyPartContext context { .assembly = assembly, .groups = assembly.groups(), .group_of = {} };
    context.group_of.assign(total, std::nullopt);
    for (size_t group = 0; group < context.groups.size(); ++group) {
        for (const uint64_t row : context.groups[group].rows) {
            if (row < total) {
                context.group_of[row] = group;
            }
        }
    }

    std::vector<MeshAssemblyPart> parts;
    std::map<std::tuple<uint8_t, uint64_t, bool>, size_t> found;

    for (size_t row = 0; row < total; ++row) {
        const auto kind = row_kind(assembly, row);
        if (kind == MeshAssemblyRowKind::Joint) {
            continue;
        }

        MeshAssemblyPartKey key = partition ? partition(context, row) : MeshAssemblyPartKey {};
        if (kind != MeshAssemblyRowKind::Mesh) {
            key.instanced = false;
        }

        const auto id = std::tuple { static_cast<uint8_t>(kind), key.key, key.instanced };
        auto it = found.find(id);
        if (it == found.end()) {
            it = found.emplace(id, parts.size()).first;
            parts.push_back({ .kind = kind, .key = key, .rows = {} });
        }
        parts[it->second].rows.push_back(row);
    }
    return parts;
}

} // namespace MayaFlux::Kakshya
