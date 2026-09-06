#pragma once

namespace MayaFlux::Buffers::detail {

/**
 * @struct NeighbourWalk
 * @brief Knobs for append_neighbour_walk. Defaults are a fixed 27-cell walk
 *        counting candidates within one cell_size.
 */
struct NeighbourWalk {
    std::string reach = "1"; ///< Cell-block half-extent, a GLSL expression or int local name.
    std::string skip_self = "j == i"; ///< Candidate skipped when this GLSL predicate holds.
    bool cluster_scoped = false; ///< Emit the cross_cluster / cluster_id guard.
    std::string radius = "cell_size"; ///< Distance cutoff, a GLSL expression.
    std::string on_hit; ///< Statements run per in-range candidate; j, bj, pj, p in scope. One per line, no braces, no indent.
};

/**
 * @brief Append the spatial-hash neighbour-gather loop nest to a kernel body.
 *
 * The kernel body must already have in scope: vec3 p; uint stride_words,
 * position_offset; the grid push constants grid_min_x/y/z, cell_size,
 * dim_x/y/z; and the vertices, cell_start, cell_count, particle_index
 * bindings. When walk.cluster_scoped is set, also uint my_cluster, uint
 * cross_cluster and the cluster_id binding.
 */
inline void append_neighbour_walk(std::string& body, const NeighbourWalk& walk)
{
    const std::string& r = walk.reach;

    body += "    vec3 gmin = vec3(grid_min_x, grid_min_y, grid_min_z);\n";
    body += "    uvec3 dims = uvec3(dim_x, dim_y, dim_z);\n";
    body += "    ivec3 base = ivec3(floor((p - gmin) / cell_size));\n";
    body += "    base = clamp(base, ivec3(0), ivec3(dims) - ivec3(1));\n";
    body += "\n";
    body += "    for (int dz = -" + r + "; dz <= " + r + "; dz = dz + 1) {\n";
    body += "        for (int dy = -" + r + "; dy <= " + r + "; dy = dy + 1) {\n";
    body += "            for (int dx = -" + r + "; dx <= " + r + "; dx = dx + 1) {\n";
    body += "                ivec3 nc = base + ivec3(dx, dy, dz);\n";
    body += "                if (nc.x < 0 || nc.y < 0 || nc.z < 0 || "
            "nc.x >= int(dim_x) || nc.y >= int(dim_y) || nc.z >= int(dim_z)) {\n";
    body += "                    continue;\n";
    body += "                }\n";
    body += "                uint cell = uint(nc.x) + uint(nc.y) * dim_x + uint(nc.z) * dim_x * dim_y;\n";
    body += "                uint start = cell_start[cell];\n";
    body += "                uint count = cell_count[cell];\n";
    body += "                for (uint k = 0u; k < count; k = k + 1u) {\n";
    body += "                    uint j = particle_index[start + k];\n";
    body += "                    if (" + walk.skip_self + ") { continue; }\n";
    if (walk.cluster_scoped) {
        body += "                    if (cross_cluster == 0u && cluster_id[j] != my_cluster) "
                "{ continue; }\n";
    }
    body += "                    uint bj = j * stride_words;\n";
    body += "                    vec3 pj = vec3(vertices[bj + position_offset], "
            "vertices[bj + position_offset + 1u], vertices[bj + position_offset + 2u]);\n";
    body += "                    if (length(pj - p) < " + walk.radius + ") {\n";

    for (std::size_t start = 0; start < walk.on_hit.size();) {
        const std::size_t nl = walk.on_hit.find('\n', start);
        const std::size_t end = nl == std::string::npos ? walk.on_hit.size() : nl;
        body += "                        ";
        body += walk.on_hit.substr(start, end - start);
        body += "\n";
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }

    body += "                    }\n";
    body += "                }\n";
    body += "            }\n";
    body += "        }\n";
    body += "    }\n";
}

} // namespace MayaFlux::Buffers::detail
