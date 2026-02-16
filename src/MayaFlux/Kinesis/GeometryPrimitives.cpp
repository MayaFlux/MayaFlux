#include "GeometryPrimitives.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::Kinesis {

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

std::vector<Nodes::LineVertex> compute_path_normals(
    const std::vector<Nodes::LineVertex>& path_vertices,
    float normal_length,
    size_t stride)
{
    if (path_vertices.size() < 2 || stride == 0) {
        return {};
    }

    std::vector<Nodes::LineVertex> normals;
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

std::vector<Nodes::LineVertex> compute_path_tangents(
    const std::vector<Nodes::LineVertex>& path_vertices,
    float tangent_length,
    size_t stride)
{
    if (path_vertices.size() < 2 || stride == 0) {
        return {};
    }

    std::vector<Nodes::LineVertex> tangents;
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

std::vector<Nodes::LineVertex> compute_path_curvature(
    const std::vector<Nodes::LineVertex>& path_vertices,
    float curvature_scale,
    size_t stride)
{
    if (path_vertices.size() < 3 || stride == 0) {
        return {};
    }

    std::vector<Nodes::LineVertex> curvatures;
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

std::vector<Nodes::LineVertex> reparameterize_by_arc_length(
    const std::vector<Nodes::LineVertex>& path_vertices,
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

    std::vector<Nodes::LineVertex> resampled;
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
        points.push_back({ glm::vec2(x, y), i });
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

std::vector<Nodes::LineVertex> apply_color_gradient(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors,
    const std::vector<float>& color_positions,
    float default_thickness)
{
    if (positions.empty() || colors.empty()) {
        return {};
    }

    std::vector<Nodes::LineVertex> vertices;
    vertices.reserve(positions.size());

    if (colors.size() == 1) {
        for (const auto& pos : positions) {
            vertices.push_back({ .position = pos,
                .color = colors[0],
                .thickness = default_thickness });
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
            .thickness = default_thickness });
    }

    return vertices;
}

std::vector<Nodes::LineVertex> apply_uniform_color(
    const std::vector<glm::vec3>& positions,
    const glm::vec3& color,
    float default_thickness)
{
    std::vector<Nodes::LineVertex> vertices;
    vertices.reserve(positions.size());

    for (const auto& pos : positions) {
        vertices.push_back({ .position = pos,
            .color = color,
            .thickness = default_thickness });
    }

    return vertices;
}

std::vector<Nodes::LineVertex> apply_vertex_colors(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& colors,
    float default_thickness)
{
    if (positions.size() != colors.size()) {
        return {};
    }

    std::vector<Nodes::LineVertex> vertices;
    vertices.reserve(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        vertices.push_back({ .position = positions[i],
            .color = colors[i],
            .thickness = default_thickness });
    }

    return vertices;
}

} // namespace MayaFlux::Kinesis
