#include "GeometryPrimitives.hpp"
#include "MayaFlux/Kakshya/NDData/MeshInsertion.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::Kinesis {

namespace {
    constexpr std::array<Kakshya::TextureQuadVertex, 4> k_unit_quad = { {
        { .position = { -1.0F, -1.0F, 0.0F }, .texcoord = { 0.0F, 1.0F } },
        { .position = { 1.0F, -1.0F, 0.0F }, .texcoord = { 1.0F, 1.0F } },
        { .position = { -1.0F, 1.0F, 0.0F }, .texcoord = { 0.0F, 0.0F } },
        { .position = { 1.0F, 1.0F, 0.0F }, .texcoord = { 1.0F, 0.0F } },
    } };

} // namespace

//==============================================================================
// Primitive Generation
//==============================================================================

std::vector<glm::vec3> generate_circle(
    const glm::vec3& center,
    float radius,
    size_t segments,
    const glm::vec3& normal)
{
    if (segments < 3) {
        segments = 3;
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(segments + 1);

    glm::vec3 n = glm::normalize(normal);
    glm::vec3 u;

    if (std::abs(n.z) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0, 0, 1)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
    }

    glm::vec3 v = glm::cross(n, u);

    float angle_step = glm::two_pi<float>() / static_cast<float>(segments);

    for (size_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);

        glm::vec3 position = center + radius * (cos_a * u + sin_a * v);

        vertices.push_back(position);
    }

    return vertices;
}

std::vector<glm::vec3> generate_ellipse(
    const glm::vec3& center,
    float semi_major,
    float semi_minor,
    size_t segments,
    const glm::vec3& normal)
{
    if (segments < 3) {
        segments = 3;
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(segments + 1);

    glm::vec3 n = glm::normalize(normal);
    glm::vec3 u;

    if (std::abs(n.z) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0, 0, 1)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
    }

    glm::vec3 v = glm::cross(n, u);

    float angle_step = glm::two_pi<float>() / static_cast<float>(segments);

    for (size_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angle_step;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);

        glm::vec3 position = center + semi_major * cos_a * u + semi_minor * sin_a * v;

        vertices.push_back(position);
    }

    return vertices;
}

std::vector<glm::vec3> generate_rectangle(
    const glm::vec3& center,
    float width,
    float height,
    const glm::vec3& normal)
{
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 u;

    if (std::abs(n.z) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0, 0, 1)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
    }

    glm::vec3 v = glm::cross(n, u);

    float half_width = width * 0.5F;
    float half_height = height * 0.5F;

    std::vector<glm::vec3> vertices;
    vertices.reserve(5);

    vertices.push_back(center - half_width * u - half_height * v);

    vertices.push_back(center + half_width * u - half_height * v);

    vertices.push_back(center + half_width * u + half_height * v);

    vertices.push_back(center - half_width * u + half_height * v);

    vertices.push_back(vertices[0]);

    return vertices;
}

std::vector<glm::vec3> generate_regular_polygon(
    const glm::vec3& center,
    float radius,
    size_t sides,
    const glm::vec3& normal,
    float phase_offset)
{
    if (sides < 3) {
        sides = 3;
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(sides + 1);

    glm::vec3 n = glm::normalize(normal);
    glm::vec3 u;

    if (std::abs(n.z) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0, 0, 1)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
    }

    glm::vec3 v = glm::cross(n, u);

    float angle_step = glm::two_pi<float>() / static_cast<float>(sides);

    for (size_t i = 0; i <= sides; ++i) {
        float angle = static_cast<float>(i) * angle_step + phase_offset;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);

        glm::vec3 position = center + radius * (cos_a * u + sin_a * v);

        vertices.push_back(position);
    }

    return vertices;
}

//==============================================================================
// Transformations
//==============================================================================

void apply_transform(
    std::vector<glm::vec3>& vertices,
    const glm::mat4& transform)
{
    for (auto& vertex : vertices) {
        glm::vec4 pos(vertex, 1.0F);
        pos = transform * pos;
        vertex = glm::vec3(pos);
    }
}

void apply_rotation(
    std::vector<glm::vec3>& vertices,
    const glm::vec3& axis,
    float angle,
    const glm::vec3& origin)
{
    glm::vec3 normalized_axis = glm::normalize(axis);

    glm::mat4 transform = glm::translate(glm::mat4(1.0F), origin);
    transform = glm::rotate(transform, angle, normalized_axis);
    transform = glm::translate(transform, -origin);

    apply_transform(vertices, transform);
}

void apply_translation(
    std::vector<glm::vec3>& vertices,
    const glm::vec3& displacement)
{
    for (auto& vertex : vertices) {
        vertex += displacement;
    }
}

void apply_uniform_scale(
    std::vector<glm::vec3>& vertices,
    float scale,
    const glm::vec3& origin)
{
    if (scale <= 0.0F) {
        return;
    }

    for (auto& vertex : vertices) {
        glm::vec3 offset = vertex - origin;
        vertex = origin + offset * scale;
    }
}

void apply_scale(
    std::vector<glm::vec3>& vertices,
    const glm::vec3& scale,
    const glm::vec3& origin)
{
    for (auto& vertex : vertices) {
        glm::vec3 offset = vertex - origin;
        vertex = origin + offset * scale;
    }
}

//==============================================================================
// Differential Geometry
//==============================================================================

std::vector<Kakshya::LineVertex> compute_path_normals(
    const std::vector<Kakshya::LineVertex>& path_vertices,
    float normal_length,
    size_t stride)
{
    if (path_vertices.size() < 2 || stride == 0) {
        return {};
    }

    std::vector<Kakshya::LineVertex> normals;
    normals.reserve((path_vertices.size() - 1) / stride * 2);

    for (size_t i = 0; i < path_vertices.size() - 1; i += stride) {
        glm::vec3 p0 = path_vertices[i].position;
        glm::vec3 p1 = path_vertices[i + 1].position;

        glm::vec3 tangent = p1 - p0;
        float length = glm::length(tangent);

        if (length < 1e-6F) {
            continue;
        }

        tangent /= length;

        // Normal (perpendicular in XY plane)
        // For 2D: rotate tangent 90° counter-clockwise
        glm::vec3 normal(-tangent.y, tangent.x, 0.0F);
        normal = glm::normalize(normal) * normal_length;

        glm::vec3 midpoint = (p0 + p1) * 0.5F;

        glm::vec3 color = path_vertices[i].color;
        float thickness = path_vertices[i].thickness;

        normals.push_back({ .position = midpoint - normal * 0.5F,
            .color = color,
            .thickness = thickness });

        normals.push_back({ .position = midpoint + normal * 0.5F,
            .color = color,
            .thickness = thickness });
    }

    return normals;
}

std::vector<Kakshya::LineVertex> compute_path_tangents(
    const std::vector<Kakshya::LineVertex>& path_vertices,
    float tangent_length,
    size_t stride)
{
    if (path_vertices.size() < 2 || stride == 0) {
        return {};
    }

    std::vector<Kakshya::LineVertex> tangents;
    tangents.reserve((path_vertices.size() - 1) / stride * 2);

    for (size_t i = 0; i < path_vertices.size() - 1; i += stride) {
        glm::vec3 p0 = path_vertices[i].position;
        glm::vec3 p1 = path_vertices[i + 1].position;

        glm::vec3 tangent = p1 - p0;
        float length = glm::length(tangent);

        if (length < 1e-6F) {
            continue;
        }

        tangent = glm::normalize(tangent) * tangent_length;

        glm::vec3 color = path_vertices[i].color;
        float thickness = path_vertices[i].thickness;

        tangents.push_back({ .position = p0 - tangent * 0.5F,
            .color = color,
            .thickness = thickness });

        tangents.push_back({ .position = p0 + tangent * 0.5F,
            .color = color,
            .thickness = thickness });
    }

    return tangents;
}

std::vector<Kakshya::LineVertex> compute_path_curvature(
    const std::vector<Kakshya::LineVertex>& path_vertices,
    float curvature_scale,
    size_t stride)
{
    if (path_vertices.size() < 3 || stride == 0) {
        return {};
    }

    std::vector<Kakshya::LineVertex> curvatures;
    curvatures.reserve((path_vertices.size() - 2) / stride * 2);

    for (size_t i = 1; i < path_vertices.size() - 1; i += stride) {
        glm::vec3 p_prev = path_vertices[i - 1].position;
        glm::vec3 p_curr = path_vertices[i].position;
        glm::vec3 p_next = path_vertices[i + 1].position;

        glm::vec3 curvature = (p_next - 2.0F * p_curr + p_prev) * curvature_scale;

        glm::vec3 color = path_vertices[i].color;
        float thickness = path_vertices[i].thickness;

        curvatures.push_back({ .position = p_curr,
            .color = color,
            .thickness = thickness });

        curvatures.push_back({ .position = p_curr + curvature,
            .color = color,
            .thickness = thickness });
    }

    return curvatures;
}

//==============================================================================
// Parametric Curves
//==============================================================================

std::vector<glm::vec3> sample_parametric_curve(
    const std::function<glm::vec3(float)>& curve,
    size_t samples)
{
    if (samples < 2) {
        samples = 2;
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(samples);

    for (size_t i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples - 1);
        glm::vec3 position = curve(t);

        vertices.push_back(position);
    }

    return vertices;
}

std::vector<Kakshya::LineVertex> reparameterize_by_arc_length(
    const std::vector<Kakshya::LineVertex>& path_vertices,
    size_t num_samples)
{
    if (path_vertices.size() < 2 || num_samples < 2) {
        return path_vertices;
    }

    std::vector<float> arc_lengths;
    arc_lengths.reserve(path_vertices.size());
    arc_lengths.push_back(0.0F);

    float total_length = 0.0F;
    for (size_t i = 1; i < path_vertices.size(); ++i) {
        float segment_length = glm::distance(
            path_vertices[i].position,
            path_vertices[i - 1].position);
        total_length += segment_length;
        arc_lengths.push_back(total_length);
    }

    if (total_length < 1e-6F) {
        return path_vertices;
    }

    std::vector<Kakshya::LineVertex> resampled;
    resampled.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        float target_length = (static_cast<float>(i) / static_cast<float>(num_samples - 1)) * total_length;

        auto it = std::ranges::lower_bound(arc_lengths, target_length);
        size_t idx = std::distance(arc_lengths.begin(), it);

        if (idx == 0) {
            resampled.push_back(path_vertices[0]);
        } else if (idx >= path_vertices.size()) {
            resampled.push_back(path_vertices.back());
        } else {
            float s0 = arc_lengths[idx - 1];
            float s1 = arc_lengths[idx];
            float t = (target_length - s0) / (s1 - s0);

            glm::vec3 p0 = path_vertices[idx - 1].position;
            glm::vec3 p1 = path_vertices[idx].position;
            glm::vec3 position = glm::mix(p0, p1, t);

            glm::vec3 c0 = path_vertices[idx - 1].color;
            glm::vec3 c1 = path_vertices[idx].color;
            glm::vec3 color = glm::mix(c0, c1, t);

            resampled.push_back({ .position = position,
                .color = color,
                .thickness = 1.0F });
        }
    }

    return resampled;
}

//==============================================================================
// Geometric Operations
//==============================================================================

void project_onto_plane(
    std::vector<glm::vec3>& vertices,
    const glm::vec3& plane_point,
    const glm::vec3& plane_normal)
{
    glm::vec3 n = glm::normalize(plane_normal);

    for (auto& vertex : vertices) {
        glm::vec3 offset = vertex - plane_point;
        float distance = glm::dot(offset, n);
        vertex = vertex - distance * n;
    }
}

std::vector<glm::vec3> compute_convex_hull_2d(
    const std::vector<glm::vec3>& vertices,
    const glm::vec3& projection_normal)
{
    if (vertices.size() < 3) {
        return vertices;
    }

    glm::vec3 n = glm::normalize(projection_normal);
    glm::vec3 u;

    if (std::abs(n.z) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0, 0, 1)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)));
    }

    glm::vec3 v = glm::cross(n, u);

    struct Point2D {
        glm::vec2 pos;
        size_t index;
    };

    std::vector<Point2D> points;
    points.reserve(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        glm::vec3 offset = vertices[i];
        float x = glm::dot(offset, u);
        float y = glm::dot(offset, v);
        points.push_back({ .pos = glm::vec2(x, y), .index = i });
    }

    auto pivot_it = std::ranges::min_element(points,
        [](const Point2D& a, const Point2D& b) {
            if (std::abs(a.pos.y - b.pos.y) < 1e-6F) {
                return a.pos.x < b.pos.x;
            }
            return a.pos.y < b.pos.y;
        });

    std::swap(points[0], *pivot_it);
    Point2D pivot = points[0];

    std::sort(points.begin() + 1, points.end(),
        [&pivot](const Point2D& a, const Point2D& b) {
            glm::vec2 va = a.pos - pivot.pos;
            glm::vec2 vb = b.pos - pivot.pos;

            float cross = va.x * vb.y - va.y * vb.x;
            if (std::abs(cross) < 1e-6F) {
                return glm::length(va) < glm::length(vb);
            }
            return cross > 0.0F;
        });

    // Graham scan
    std::vector<size_t> hull;
    hull.push_back(points[0].index);
    hull.push_back(points[1].index);

    auto ccw = [](const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0.0F;
    };

    for (size_t i = 2; i < points.size(); ++i) {
        while (hull.size() >= 2) {
            size_t top = hull.back();
            size_t second = hull[hull.size() - 2];

            glm::vec2 p1 = points[second].pos;
            glm::vec2 p2 = points[top].pos;
            glm::vec2 p3 = points[i].pos;

            if (ccw(p1, p2, p3)) {
                break;
            }
            hull.pop_back();
        }
        hull.push_back(points[i].index);
    }

    std::vector<glm::vec3> result;
    result.reserve(hull.size() + 1);

    for (size_t idx : hull) {
        result.push_back(vertices[idx]);
    }

    result.push_back(vertices[hull[0]]);

    return result;
}

//==============================================================================
// Color Utilities
//==============================================================================

std::vector<Kakshya::Vertex> apply_color_gradient(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors,
    const std::vector<float>& color_positions,
    float scalar)
{
    if (positions.empty() || colors.empty()) {
        return {};
    }

    std::vector<Kakshya::Vertex> vertices;
    vertices.reserve(positions.size());

    if (colors.size() == 1) {
        for (const auto& pos : positions) {
            vertices.push_back({ .position = pos,
                .color = colors[0],
                .scalar = scalar });
        }
        return vertices;
    }

    std::vector<float> stops = color_positions;
    if (stops.empty()) {
        stops.reserve(colors.size());
        for (size_t i = 0; i < colors.size(); ++i) {
            stops.push_back(static_cast<float>(i) / static_cast<float>(colors.size() - 1));
        }
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(positions.size() - 1);

        glm::vec3 color;
        if (t <= stops[0]) {
            color = colors[0];
        } else if (t >= stops.back()) {
            color = colors.back();
        } else {
            size_t idx = 0;
            for (size_t j = 1; j < stops.size(); ++j) {
                if (t <= stops[j]) {
                    idx = j - 1;
                    break;
                }
            }

            float local_t = (t - stops[idx]) / (stops[idx + 1] - stops[idx]);
            color = glm::mix(colors[idx], colors[idx + 1], local_t);
        }

        vertices.push_back({ .position = positions[i],
            .color = color,
            .scalar = scalar });
    }

    return vertices;
}

std::vector<Kakshya::Vertex> apply_uniform_color(
    const std::vector<glm::vec3>& positions,
    const glm::vec3& color,
    float scalar)
{
    std::vector<Kakshya::Vertex> vertices;
    vertices.reserve(positions.size());

    for (const auto& pos : positions) {
        vertices.push_back({ .position = pos,
            .color = color,
            .scalar = scalar });
    }

    return vertices;
}

std::vector<Kakshya::Vertex> apply_vertex_colors(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors,
    float scalar)
{
    if (positions.size() != colors.size()) {
        return {};
    }

    std::vector<Kakshya::Vertex> vertices;
    vertices.reserve(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        vertices.push_back({ .position = positions[i],
            .color = colors[i],
            .scalar = scalar });
    }

    return vertices;
}

QuadGeometry generate_quad(glm::vec2 position, glm::vec2 scale, float rotation)
{
    const float cos_r = std::cos(rotation);
    const float sin_r = std::sin(rotation);

    QuadGeometry out { .layout = Kakshya::VertexLayout::for_textured_quad() };

    for (size_t i = 0; i < 4; ++i) {
        const float x = k_unit_quad[i].position.x * scale.x;
        const float y = k_unit_quad[i].position.y * scale.y;

        out.vertices[i].position = {
            x * cos_r - y * sin_r + position.x,
            x * sin_r + y * cos_r + position.y,
            0.0F,
        };
        out.vertices[i].texcoord = k_unit_quad[i].texcoord;
    }

    return out;
}

std::array<Kakshya::Vertex, 4> filled_rect(Kinesis::AABB2D region, glm::vec3 color)
{
    return { {
        { .position = { region.min.x, region.min.y, 0.F }, .color = color },
        { .position = { region.min.x, region.max.y, 0.F }, .color = color },
        { .position = { region.max.x, region.min.y, 0.F }, .color = color },
        { .position = { region.max.x, region.max.y, 0.F }, .color = color },
    } };
}

std::array<Kakshya::TextureQuadVertex, 4> textured_rect(Kinesis::AABB2D region)
{
    return { {
        { .position = { region.min.x, region.min.y, 0.F }, .texcoord = { 0.F, 1.F } },
        { .position = { region.min.x, region.max.y, 0.F }, .texcoord = { 0.F, 0.F } },
        { .position = { region.max.x, region.min.y, 0.F }, .texcoord = { 1.F, 1.F } },
        { .position = { region.max.x, region.max.y, 0.F }, .texcoord = { 1.F, 0.F } },
    } };
}

std::vector<Kakshya::Vertex> cuboid_wireframe(
    const glm::vec3& center, const glm::vec3& half, const glm::vec3& color)
{
    const glm::vec3 v[8] = {
        center + glm::vec3(-half.x, -half.y, -half.z),
        center + glm::vec3(half.x, -half.y, -half.z),
        center + glm::vec3(half.x, half.y, -half.z),
        center + glm::vec3(-half.x, half.y, -half.z),
        center + glm::vec3(-half.x, -half.y, half.z),
        center + glm::vec3(half.x, -half.y, half.z),
        center + glm::vec3(half.x, half.y, half.z),
        center + glm::vec3(-half.x, half.y, half.z),
    };
    return {
        { .position = v[0], .color = color },
        { .position = v[1], .color = color },
        { .position = v[1], .color = color },
        { .position = v[2], .color = color },
        { .position = v[2], .color = color },
        { .position = v[3], .color = color },
        { .position = v[3], .color = color },
        { .position = v[0], .color = color },
        { .position = v[4], .color = color },
        { .position = v[5], .color = color },
        { .position = v[5], .color = color },
        { .position = v[6], .color = color },
        { .position = v[6], .color = color },
        { .position = v[7], .color = color },
        { .position = v[7], .color = color },
        { .position = v[4], .color = color },
        { .position = v[0], .color = color },
        { .position = v[4], .color = color },
        { .position = v[1], .color = color },
        { .position = v[5], .color = color },
        { .position = v[2], .color = color },
        { .position = v[6], .color = color },
        { .position = v[3], .color = color },
        { .position = v[7], .color = color },
    };
}

Kakshya::MeshData generate_box(
    const glm::vec3& center,
    const glm::vec3& half_extents,
    uint32_t subdivisions)
{
    const uint32_t n = std::max(subdivisions, 1U);

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(uint32_t(6 * (n + 1) * (n + 1)));
    indices.reserve(uint32_t(6 * n * n * 6));

    struct FaceDef {
        glm::vec3 origin;
        glm::vec3 u_axis;
        glm::vec3 v_axis;
        glm::vec3 normal;
    };

    const std::array<FaceDef, 6> faces = { {
        { .origin = { -1, -1, 1 }, .u_axis = { 2, 0, 0 }, .v_axis = { 0, 2, 0 }, .normal = { 0, 0, 1 } },
        { .origin = { 1, -1, -1 }, .u_axis = { -2, 0, 0 }, .v_axis = { 0, 2, 0 }, .normal = { 0, 0, -1 } },
        { .origin = { 1, -1, 1 }, .u_axis = { 0, 0, -2 }, .v_axis = { 0, 2, 0 }, .normal = { 1, 0, 0 } },
        { .origin = { -1, -1, -1 }, .u_axis = { 0, 0, 2 }, .v_axis = { 0, 2, 0 }, .normal = { -1, 0, 0 } },
        { .origin = { -1, 1, 1 }, .u_axis = { 2, 0, 0 }, .v_axis = { 0, 0, -2 }, .normal = { 0, 1, 0 } },
        { .origin = { -1, -1, -1 }, .u_axis = { 2, 0, 0 }, .v_axis = { 0, 0, 2 }, .normal = { 0, -1, 0 } },
    } };

    for (const auto& f : faces) {
        const auto base = static_cast<uint32_t>(verts.size());

        for (uint32_t row = 0; row <= n; ++row) {
            const float fv = static_cast<float>(row) / static_cast<float>(n);
            for (uint32_t col = 0; col <= n; ++col) {
                const float fu = static_cast<float>(col) / static_cast<float>(n);
                const glm::vec3 p = f.origin + f.u_axis * fu + f.v_axis * fv;
                verts.push_back({
                    .position = center + p * half_extents,
                    .uv = { fu, 1.0F - fv },
                    .normal = f.normal,
                });
            }
        }

        const uint32_t stride = n + 1;
        for (uint32_t row = 0; row < n; ++row) {
            for (uint32_t col = 0; col < n; ++col) {
                const uint32_t a = base + row * stride + col;
                const uint32_t b = a + 1;
                const uint32_t c = a + stride;
                const uint32_t d = c + 1;
                indices.insert(indices.end(), { a, b, d, a, d, c });
            }
        }
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    return data;
}

Kakshya::MeshData generate_grid(
    const glm::vec3& center,
    float extent_x,
    float extent_z,
    uint32_t cols,
    uint32_t rows,
    const glm::vec3& normal)
{
    cols = std::max(cols, 1U);
    rows = std::max(rows, 1U);

    const glm::vec3 n = glm::normalize(normal);
    glm::vec3 u;
    if (std::abs(n.y) < 0.9F) {
        u = glm::normalize(glm::cross(n, glm::vec3(0.0F, 1.0F, 0.0F)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(1.0F, 0.0F, 0.0F)));
    }
    const glm::vec3 v = glm::normalize(glm::cross(u, n));

    const float half_x = extent_x * 0.5F;
    const float half_z = extent_z * 0.5F;

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(uint32_t(2 * (cols + 1) * (rows + 1)));
    indices.reserve(uint32_t(2 * cols * rows * 6));

    for (uint32_t row = 0; row <= rows; ++row) {
        const float fv = static_cast<float>(row) / static_cast<float>(rows);
        for (uint32_t col = 0; col <= cols; ++col) {
            const float fu = static_cast<float>(col) / static_cast<float>(cols);
            const glm::vec3 p = center
                + u * glm::mix(-half_x, half_x, fu)
                + v * glm::mix(-half_z, half_z, fv);
            verts.push_back({
                .position = p,
                .uv = { fu, 1.0F - fv },
                .normal = n,
            });
        }
    }

    const uint32_t stride = cols + 1;
    const auto vert_count = static_cast<uint32_t>(verts.size());

    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            const uint32_t a = row * stride + col;
            const uint32_t b = a + 1;
            const uint32_t c = a + stride;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { a, b, c, b, d, c });
        }
    }

    for (uint32_t row = 0; row <= rows; ++row) {
        const float fv = static_cast<float>(row) / static_cast<float>(rows);
        for (uint32_t col = 0; col <= cols; ++col) {
            const float fu = static_cast<float>(col) / static_cast<float>(cols);
            const glm::vec3 p = center
                + u * glm::mix(-half_x, half_x, fu)
                + v * glm::mix(-half_z, half_z, fv);
            verts.push_back({
                .position = p,
                .uv = { fu, 1.0F - fv },
                .normal = -n,
            });
        }
    }

    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            const uint32_t a = vert_count + row * stride + col;
            const uint32_t b = a + 1;
            const uint32_t c = a + stride;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { a, c, b, b, c, d });
        }
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    data.layout.vertex_count = static_cast<uint32_t>(verts.size());
    return data;
}

Kakshya::MeshData generate_parametric_surface(
    const std::function<glm::vec3(float, float)>& fn,
    uint32_t u_segs,
    uint32_t v_segs)
{
    u_segs = std::max(u_segs, 1U);
    v_segs = std::max(v_segs, 1U);

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(uint32_t((u_segs + 1) * (v_segs + 1)));
    indices.reserve(uint32_t(u_segs * v_segs * 6));

    constexpr float eps = 1e-4F;

    for (uint32_t row = 0; row <= v_segs; ++row) {
        const float fv = static_cast<float>(row) / static_cast<float>(v_segs);
        for (uint32_t col = 0; col <= u_segs; ++col) {
            const float fu = static_cast<float>(col) / static_cast<float>(u_segs);

            const glm::vec3 p = fn(fu, fv);
            const glm::vec3 pu = fn(fu + eps, fv) - fn(fu - eps, fv);
            const glm::vec3 pv = fn(fu, fv + eps) - fn(fu, fv - eps);
            const glm::vec3 n = glm::normalize(glm::cross(pu, pv));

            verts.push_back({
                .position = p,
                .uv = { fu, 1.0F - fv },
                .normal = n,
            });
        }
    }

    const uint32_t stride = u_segs + 1;
    for (uint32_t row = 0; row < v_segs; ++row) {
        for (uint32_t col = 0; col < u_segs; ++col) {
            const uint32_t a = row * stride + col;
            const uint32_t b = a + 1;
            const uint32_t c = a + stride;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { a, b, c, b, d, c });
        }
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    data.layout.vertex_count = static_cast<uint32_t>(verts.size());

    return data;
}

Kakshya::MeshData generate_tube(
    std::span<const glm::vec3> path,
    const std::function<float(float)>& radius_fn,
    uint32_t radial_segments,
    bool capped)
{
    const uint32_t seg = std::max(radial_segments, 3U);
    const auto n_pts = static_cast<uint32_t>(std::max<size_t>(path.size(), 2));

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(uint32_t(n_pts * (seg + 1) + (capped ? 2 * (seg + 1) : 0)));
    indices.reserve(uint32_t((n_pts - 1) * seg * 6 + (capped ? 2 * seg * 3 : 0)));

    glm::vec3 tangent = glm::normalize(path[1] - path[0]);
    glm::vec3 u_axis;
    if (std::abs(tangent.y) < 0.9F) {
        u_axis = glm::normalize(glm::cross(tangent, glm::vec3(0.0F, 1.0F, 0.0F)));
    } else {
        u_axis = glm::normalize(glm::cross(tangent, glm::vec3(1.0F, 0.0F, 0.0F)));
    }

    glm::vec3 v_axis = glm::normalize(glm::cross(tangent, u_axis));

    const float total_len = [&]() {
        float l = 0.0F;
        for (uint32_t i = 1; i < n_pts; ++i)
            l += glm::distance(path[i], path[i - 1]);
        return l;
    }();

    float arc = 0.0F;

    for (uint32_t pi = 0; pi < n_pts; ++pi) {
        if (pi > 0) {
            const glm::vec3 new_tan = (pi + 1 < n_pts)
                ? glm::normalize(path[pi + 1] - path[pi - 1])
                : glm::normalize(path[pi] - path[pi - 1]);
            const glm::vec3 axis = glm::cross(tangent, new_tan);
            const float axis_len = glm::length(axis);
            if (axis_len > 1e-6F) {
                const float angle = std::asin(glm::clamp(axis_len, 0.0F, 1.0F));
                const glm::mat4 rot = glm::rotate(glm::mat4(1.0F), angle,
                    glm::normalize(axis));
                u_axis = glm::vec3(rot * glm::vec4(u_axis, 0.0F));
                v_axis = glm::vec3(rot * glm::vec4(v_axis, 0.0F));
                tangent = new_tan;
            }
            arc += glm::distance(path[pi], path[pi - 1]);
        }

        const float t = (total_len > 1e-6F) ? (arc / total_len) : 0.0F;
        const float r = radius_fn(t);
        const float angle_step = glm::two_pi<float>() / static_cast<float>(seg);

        for (uint32_t s = 0; s <= seg; ++s) {
            const float a = static_cast<float>(s) * angle_step;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            const glm::vec3 radial = ca * u_axis + sa * v_axis;
            verts.push_back({
                .position = path[pi] + radial * r,
                .uv = { static_cast<float>(s) / static_cast<float>(seg), t },
                .normal = radial,
            });
        }
    }

    const uint32_t ring = seg + 1;
    for (uint32_t pi = 0; pi < n_pts - 1; ++pi) {
        for (uint32_t s = 0; s < seg; ++s) {
            const uint32_t a = pi * ring + s;
            const uint32_t b = a + 1;
            const uint32_t c = a + ring;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { a, b, c, b, d, c });
        }
    }

    if (capped) {
        for (int end = 0; end < 2; ++end) {
            const uint32_t ring_base = (end == 0) ? 0 : (n_pts - 1) * ring;
            const float t_cap = (end == 0) ? 0.0F : 1.0F;
            const glm::vec3 cap_nrm = (end == 0) ? -tangent : tangent;
            const glm::vec3 cap_pos = path[(end == 0) ? 0 : n_pts - 1];

            const auto centre_idx = static_cast<uint32_t>(verts.size());
            verts.push_back({
                .position = cap_pos,
                .uv = { 0.5F, t_cap },
                .normal = cap_nrm,
            });

            for (uint32_t s = 0; s < seg; ++s) {
                const uint32_t a = ring_base + s;
                const uint32_t b = ring_base + s + 1;
                if (end == 0) {
                    indices.insert(indices.end(), { centre_idx, b, a });
                } else {
                    indices.insert(indices.end(), { centre_idx, a, b });
                }
            }
        }
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    data.layout.vertex_count = static_cast<uint32_t>(verts.size());
    return data;
}

Kakshya::MeshData generate_revolution(
    const std::function<glm::vec2(float)>& profile_fn,
    uint32_t profile_segs,
    uint32_t radial_segs,
    float sweep_radians)
{
    profile_segs = std::max(profile_segs, 2U);
    radial_segs = std::max(radial_segs, 3U);

    std::vector<Kakshya::MeshVertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(size_t((profile_segs + 1)) * (radial_segs + 1));
    indices.reserve(uint32_t(profile_segs * radial_segs * 6));

    for (uint32_t pi = 0; pi <= profile_segs; ++pi) {
        const float t = static_cast<float>(pi) / static_cast<float>(profile_segs);
        const glm::vec2 p = profile_fn(t); // p.x = radius from Y axis, p.y = height

        for (uint32_t ri = 0; ri <= radial_segs; ++ri) {
            const float u = static_cast<float>(ri) / static_cast<float>(radial_segs);
            const float angle = u * sweep_radians;
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);

            const glm::vec3 pos { p.x * ca, p.y, p.x * sa };

            constexpr float eps = 1e-4F;
            const glm::vec2 dp = profile_fn(glm::clamp(t + eps, 0.0F, 1.0F))
                - profile_fn(glm::clamp(t - eps, 0.0F, 1.0F));
            const glm::vec3 profile_tan { dp.x * ca, dp.y, dp.x * sa };
            const glm::vec3 radial { ca, 0.0F, sa };
            const glm::vec3 nrm = glm::normalize(glm::cross(profile_tan, radial));

            verts.push_back({
                .position = pos,
                .uv = { u, 1.0F - t },
                .normal = nrm,
            });
        }
    }

    const uint32_t stride = radial_segs + 1;
    for (uint32_t pi = 0; pi < profile_segs; ++pi) {
        for (uint32_t ri = 0; ri < radial_segs; ++ri) {
            const uint32_t a = pi * stride + ri;
            const uint32_t b = a + 1;
            const uint32_t c = a + stride;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { a, b, c, b, d, c });
        }
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(Kakshya::MeshVertex)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
    data.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    data.layout.vertex_count = static_cast<uint32_t>(verts.size());
    return data;
}

} // namespace MayaFlux::Kinesis
