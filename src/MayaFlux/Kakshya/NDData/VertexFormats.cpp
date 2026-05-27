#include "VertexFormats.hpp"

namespace MayaFlux::Kakshya {

std::vector<PointVertex> to_point_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 size_range)
{
    std::vector<PointVertex> out;
    out.reserve(vertices.size());
    for (const auto& s : vertices) {
        out.push_back(to_point_vertex(s, size_range));
    }
    return out;
}

std::vector<LineVertex> to_line_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 thickness_range)
{
    std::vector<LineVertex> out;
    out.reserve(vertices.size());
    for (const auto& s : vertices) {
        out.push_back(to_line_vertex(s, thickness_range));
    }
    return out;
}

std::vector<MeshVertex> to_mesh_vertices(
    std::span<const Vertex> vertices,
    glm::vec2 weight_range)
{
    std::vector<MeshVertex> out;
    out.reserve(vertices.size());
    for (const auto& s : vertices) {
        out.push_back(to_mesh_vertex(s, weight_range));
    }
    return out;
}

} // namespace MayaFlux::Kakshya
