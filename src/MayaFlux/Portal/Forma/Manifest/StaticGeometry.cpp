#include "StaticGeometry.hpp"

#include "MayaFlux/Kinesis/Geometry2D.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    std::optional<Kinesis::AABB2D> placement_region(
        const RectPlacementInputs& placement, std::string& error)
    {
        const glm::vec4 rect = placement.rect;
        if (!(rect.z > 0.F) || !(rect.w > 0.F)) {
            error = "rect needs a positive width and height";
            return std::nullopt;
        }
        return Kinesis::AABB2D::from_origin_size({ rect.x, rect.y }, { rect.z, rect.w });
    }

    std::vector<uint8_t> polygon_bytes(std::span<const glm::vec2> ndc,
        std::span<const uint32_t> starts,
        std::span<const std::array<uint32_t, 3>> triangles,
        std::span<const glm::vec4> colors, float thickness)
    {
        std::vector<uint8_t> out;

        if (thickness > 0.F) {
            std::vector<size_t> bounds { 0 };
            bounds.insert(bounds.end(), starts.begin(), starts.end());
            bounds.push_back(ndc.size());

            std::vector<Kakshya::LineVertex> verts;
            verts.reserve(ndc.size() * 2);
            for (size_t ring = 0; ring + 1 < bounds.size(); ++ring) {
                const size_t begin = bounds[ring];
                const size_t count = bounds[ring + 1] - begin;
                for (size_t k = 0; k < count; ++k) {
                    const size_t from = begin + k;
                    const size_t to = begin + (k + 1) % count;
                    verts.push_back(Kakshya::LineVertex {
                        .position = { ndc[from].x, ndc[from].y, 0.F },
                        .color = colors[from],
                        .thickness = thickness,
                    });
                    verts.push_back(Kakshya::LineVertex {
                        .position = { ndc[to].x, ndc[to].y, 0.F },
                        .color = colors[to],
                        .thickness = thickness,
                    });
                }
            }
            Geometry::write_verts(out, verts);
            return out;
        }

        std::vector<Kakshya::Vertex> verts;
        verts.reserve(triangles.size() * 3);
        for (const auto& triangle : triangles) {
            for (const uint32_t k : triangle) {
                verts.push_back(Kakshya::Vertex {
                    .position = { ndc[k].x, ndc[k].y, 0.F },
                    .color = colors[k],
                });
            }
        }
        Geometry::write_verts(out, Kakshya::to_mesh_vertices(verts));
        return out;
    }

}

std::optional<Geometry::Form<std::monostate>> vertex_geometry_form(
    const VertexGeometryInputs& input, const RectPlacementInputs& placement, std::string& error)
{
    if (!Detail::vertex_count_matches_topology(input.topology, input.vertices.size())) {
        error = "vertex count does not match the topology";
        return std::nullopt;
    }
    const auto region = placement_region(placement, error);
    if (!region)
        return std::nullopt;

    std::vector<Kakshya::Vertex> vertices = input.vertices;
    std::vector<glm::vec2> positions;
    positions.reserve(vertices.size());
    for (Kakshya::Vertex& vertex : vertices) {
        const glm::vec2 ndc = region->point_at({ vertex.position.x, vertex.position.y });
        vertex.position = { ndc.x, ndc.y, vertex.position.z };
        positions.push_back(ndc);
    }

    const Containment hit = containment_of(positions, input.topology);
    const size_t capacity = vertices.size() * sizeof(Kakshya::Vertex);
    GeometryFn<std::monostate> fn = [vertices = std::move(vertices), hit](
                                        std::monostate, std::vector<uint8_t>& out, Element& element) {
        Geometry::write_verts(out, vertices);
        apply_containment(element, hit);
    };
    return Geometry::Form<std::monostate> { std::move(fn), input.topology, capacity };
}

std::optional<Geometry::Form<bool>> PolygonFactory::operator()(
    const PolygonInputs& input, const RectPlacementInputs& placement, std::string& error) const
{
    const bool per_vertex = !input.colors.empty();
    if (input.points.size() < 3) {
        error = "points needs at least three x y pairs";
        return std::nullopt;
    }
    if (per_vertex && input.colors.size() != input.points.size()) {
        error = "colors needs one r g b a per point";
        return std::nullopt;
    }
    if (!(input.thickness >= 0.F)) {
        error = "thickness may not be negative";
        return std::nullopt;
    }

    std::vector<glm::vec2> unit = input.points;
    std::vector<glm::vec4> vertex_colors = input.colors;
    std::vector<uint32_t> starts;
    for (const PolygonHole& hole : input.holes) {
        if (hole.points.size() < 3) {
            error = "a hole needs at least three x y pairs";
            return std::nullopt;
        }
        if (per_vertex ? hole.colors.size() != hole.points.size() : !hole.colors.empty()) {
            error = "a hole needs one r g b a per point exactly when the outline has colors";
            return std::nullopt;
        }
        starts.push_back(static_cast<uint32_t>(unit.size()));
        unit.insert(unit.end(), hole.points.begin(), hole.points.end());
        vertex_colors.insert(vertex_colors.end(), hole.colors.begin(), hole.colors.end());
    }
    if (!per_vertex)
        vertex_colors.assign(unit.size(), input.color);

    const auto region = placement_region(placement, error);
    if (!region)
        return std::nullopt;

    std::vector<glm::vec2> ndc;
    ndc.reserve(unit.size());
    for (const glm::vec2 point : unit)
        ndc.push_back(region->point_at(point));

    const bool stroke = input.thickness > 0.F;
    std::vector<std::array<uint32_t, 3>> triangles;
    if (!stroke) {
        triangles = Kinesis::triangulate_polygon(unit, starts);
        if (triangles.empty()) {
            error = "the outline and holes do not form a valid region";
            return std::nullopt;
        }
    }

    const std::vector<uint8_t> base = polygon_bytes(ndc, starts, triangles, vertex_colors, input.thickness);
    std::vector<uint8_t> active = base;
    if (input.active) {
        const std::vector<glm::vec4> active_colors(unit.size(), *input.active);
        active = polygon_bytes(ndc, starts, triangles, active_colors, input.thickness);
    }

    using Topology = Graphics::PrimitiveTopology;
    const Topology topology = stroke ? Topology::LINE_LIST : Topology::TRIANGLE_LIST;
    std::vector<glm::vec2> drawn;
    if (stroke) {
        drawn = ndc;
    } else {
        drawn.reserve(triangles.size() * 3);
        for (const auto& triangle : triangles) {
            for (const uint32_t k : triangle)
                drawn.push_back(ndc[k]);
        }
    }

    Containment hit = containment_of(drawn, topology);
    if (!input.containment.empty()) {
        const auto chosen = containments.resolve(input.containment,
            ContainmentSource { .positions = drawn, .topology = topology, .placement = region }, error);
        if (!chosen)
            return std::nullopt;
        hit = *chosen;
    }

    const size_t capacity = stroke
        ? ndc.size() * 2 * sizeof(Kakshya::LineVertex)
        : triangles.size() * 3 * sizeof(Kakshya::MeshVertex);
    GeometryFn<bool> fn = [base, active, hit](
                              bool on, std::vector<uint8_t>& out, Element& element) {
        out = on ? active : base;
        apply_containment(element, hit);
    };

    Geometry::Form<bool> form { std::move(fn), topology, capacity };
    if (!input.interaction.empty()) {
        const auto wire = interactions.resolve(input.interaction, InteractionSource { .placement = region }, error);
        if (!wire)
            return std::nullopt;
        form.wire = *wire;
    }
    return form;
}

}
