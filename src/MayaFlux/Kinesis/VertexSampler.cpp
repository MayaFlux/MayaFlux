#include "VertexSampler.hpp"

#include "MotionCurves.hpp"

#include <glm/gtc/constants.hpp>

namespace MayaFlux::Kinesis {

namespace {

    SampleResult sample_random_volume(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        glm::vec3 pos {
            rng(b.min.x, b.max.x),
            rng(b.min.y, b.max.y),
            rng(b.min.z, b.max.z)
        };
        const glm::vec3 ext = b.extent();
        return { .position = pos, .color = (pos - b.min) / ext, .scalar = 0.5F };
    }

    SampleResult sample_random_surface(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        static constexpr glm::vec3 k_face_colors[6] {
            { 0.8F, 0.3F, 0.3F }, { 1.0F, 0.4F, 0.4F },
            { 0.3F, 0.8F, 0.3F }, { 0.4F, 1.0F, 0.4F },
            { 0.3F, 0.3F, 0.8F }, { 0.4F, 0.4F, 1.0F }
        };

        const int face = static_cast<int>(rng(0, 6));
        glm::vec3 pos;

        switch (face) {
        case 0:
            pos = { b.min.x, rng(b.min.y, b.max.y), rng(b.min.z, b.max.z) };
            break;
        case 1:
            pos = { b.max.x, rng(b.min.y, b.max.y), rng(b.min.z, b.max.z) };
            break;
        case 2:
            pos = { rng(b.min.x, b.max.x), b.min.y, rng(b.min.z, b.max.z) };
            break;
        case 3:
            pos = { rng(b.min.x, b.max.x), b.max.y, rng(b.min.z, b.max.z) };
            break;
        case 4:
            pos = { rng(b.min.x, b.max.x), rng(b.min.y, b.max.y), b.min.z };
            break;
        default:
            pos = { rng(b.min.x, b.max.x), rng(b.min.y, b.max.y), b.max.z };
            break;
        }

        return { .position = pos, .color = k_face_colors[face], .scalar = 1.0F };
    }

    SampleResult sample_grid(const SamplerBounds& b, size_t idx, size_t total)
    {
        const size_t gs = static_cast<size_t>(std::cbrt(static_cast<double>(total))) + 1;
        const glm::vec3 spacing = b.extent() / static_cast<float>(gs);
        const size_t x = idx % gs;
        const size_t y = (idx / gs) % gs;
        const size_t z = idx / (gs * gs);

        const glm::vec3 pos = b.min + glm::vec3(static_cast<float>(x) * spacing.x, static_cast<float>(y) * spacing.y, static_cast<float>(z) * spacing.z);

        const glm::vec3 color {
            static_cast<float>(x) / static_cast<float>(gs),
            static_cast<float>(y) / static_cast<float>(gs),
            static_cast<float>(z) / static_cast<float>(gs)
        };

        return { .position = pos, .color = color, .scalar = 0.5F };
    }

    SampleResult sample_sphere_volume(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        const float mr = b.max_radius();
        const float radius = mr * std::cbrt(static_cast<float>(rng(0.0F, 1.0F)));
        const auto theta = static_cast<float>(rng(0.0F, glm::two_pi<double>()));
        const float phi = std::acos(static_cast<float>(rng(-1.0F, 1.0F)));

        const glm::vec3 pos = b.center() + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));

        const float norm = radius / mr;
        return {
            .position = pos,
            .color = glm::mix(glm::vec3(1.0F, 0.8F, 0.2F), glm::vec3(0.2F, 0.4F, 1.0F), norm),
            .scalar = 1.0F - norm
        };
    }

    SampleResult sample_sphere_surface(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        const float radius = b.max_radius();
        const auto theta = static_cast<float>(rng(0.0F, glm::two_pi<double>()));
        const float phi = std::acos(static_cast<float>(rng(-1.0F, 1.0F)));

        const glm::vec3 pos = b.center() + glm::vec3(radius * std::sin(phi) * std::cos(theta), radius * std::sin(phi) * std::sin(theta), radius * std::cos(phi));

        const float lat = std::sin(phi);
        return {
            .position = pos,
            .color = { (std::sin(theta) + 1.0F) * 0.5F, phi / glm::pi<float>(), (std::cos(theta) + 1.0F) * 0.5F },
            .scalar = lat
        };
    }

    SampleResult sample_uniform_grid(const SamplerBounds& b, size_t idx, size_t total)
    {
        const auto ppa = static_cast<size_t>(std::cbrt(static_cast<double>(total)));
        const glm::vec3 step = b.extent() / static_cast<float>(ppa - 1 > 0 ? ppa - 1 : 1);
        const size_t x = idx % ppa;
        const size_t y = (idx / ppa) % ppa;
        const size_t z = idx / (ppa * ppa);

        const glm::vec3 pos = b.min + glm::vec3(static_cast<float>(x) * step.x, static_cast<float>(y) * step.y, static_cast<float>(z) * step.z);

        const glm::vec3 color {
            static_cast<float>(x) / static_cast<float>(ppa - 1 > 0 ? ppa - 1 : 1),
            static_cast<float>(y) / static_cast<float>(ppa - 1 > 0 ? ppa - 1 : 1),
            static_cast<float>(z) / static_cast<float>(ppa - 1 > 0 ? ppa - 1 : 1)
        };

        const float t = glm::length(pos - b.center()) / b.max_radius();
        return { .position = pos, .color = color, .scalar = t };
    }

    SampleResult sample_random_sphere(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        const auto theta = static_cast<float>(rng(0.0F, glm::two_pi<double>()));
        const float phi = std::acos(static_cast<float>(rng(-1.0F, 1.0F)));
        const float radius = b.max_radius() * static_cast<float>(std::cbrt(rng(0.0F, 1.0F)));

        const glm::vec3 pos = b.center() + radius * glm::vec3(std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi));

        return {
            .position = pos,
            .color = { radius / b.max_radius(), theta / glm::two_pi<float>(), phi / glm::pi<float>() },
            .scalar = radius / b.max_radius()
        };
    }

    SampleResult sample_random_cube(const SamplerBounds& b, Stochastic::Stochastic& rng)
    {
        const glm::vec3 pos {
            rng(b.min.x, b.max.x),
            rng(b.min.y, b.max.y),
            rng(b.min.z, b.max.z)
        };
        return { .position = pos, .color = (pos - b.min) / b.extent(), .scalar = 0.5F };
    }

    std::vector<SampleResult> sample_perlin_field(
        const SamplerBounds& b,
        size_t count,
        Stochastic::Stochastic& rng)
    {
        auto perlin = Stochastic::perlin(4, 0.5);
        std::vector<SampleResult> out;
        out.reserve(count);

        while (out.size() < count) {
            const glm::vec3 p {
                rng(b.min.x, b.max.x),
                rng(b.min.y, b.max.y),
                rng(b.min.z, b.max.z)
            };
            if (perlin.at(p.x, p.y, p.z) > rng(0.0, 1.0)) {
                out.push_back({ .position = p, .color = (p - b.min) / b.extent(), .scalar = 0.5F });
            }
        }

        return out;
    }

    std::vector<SampleResult> sample_brownian_path(
        const SamplerBounds& b,
        size_t count,
        Stochastic::Stochastic& rng)
    {
        auto alg_backup = rng.get_algorithm();
        rng.set_algorithm(Stochastic::Algorithm::BROWNIAN);

        std::vector<SampleResult> out;
        out.reserve(count);

        glm::vec3 pos = b.center();
        for (size_t i = 0; i < count; ++i) {
            pos += glm::vec3(rng(-1.0, 1.0), rng(-1.0, 1.0), rng(-1.0, 1.0)) * 0.1F;
            pos = glm::clamp(pos, b.min, b.max);
            out.push_back({ .position = pos,
                .color = glm::vec3(static_cast<float>(i) / static_cast<float>(count)),
                .scalar = static_cast<float>(i) / static_cast<float>(count) });
        }
        rng.set_algorithm(alg_backup);

        return out;
    }

    std::vector<SampleResult> sample_stratified_cube(
        const SamplerBounds& b,
        size_t count,
        Stochastic::Stochastic& rng)
    {
        const auto ppa = static_cast<size_t>(std::cbrt(count));
        const glm::vec3 step = b.extent() / static_cast<float>(ppa);
        std::vector<SampleResult> out;
        out.reserve(ppa * ppa * ppa);

        for (size_t x = 0; x < ppa; ++x) {
            for (size_t y = 0; y < ppa; ++y) {
                for (size_t z = 0; z < ppa; ++z) {
                    const glm::vec3 jitter { rng(-0.5F, 0.5F), rng(-0.5F, 0.5F), rng(-0.5F, 0.5F) };
                    const glm::vec3 pos = b.min + (glm::vec3(x, y, z) + 0.5F + jitter) * step;
                    out.push_back({ .position = pos, .color = (pos - b.min) / b.extent(), .scalar = 0.6F });
                }
            }
        }

        return out;
    }

    std::vector<SampleResult> sample_spline_path(
        const SamplerBounds& b,
        size_t count,
        Stochastic::Stochastic& rng)
    {
        Eigen::MatrixXd ctrl(3, 6);
        for (int i = 0; i < 6; ++i) {
            ctrl.col(i) = Eigen::Vector3d(
                rng(b.min.x, b.max.x),
                rng(b.min.y, b.max.y),
                rng(b.min.z, b.max.z));
        }

        Eigen::MatrixXd path = generate_interpolated_points(ctrl, static_cast<Eigen::Index>(count), InterpolationMode::CATMULL_ROM);

        std::vector<SampleResult> out;
        out.reserve(path.cols());
        for (Eigen::Index i = 0; i < path.cols(); ++i) {
            const glm::vec3 pos(path(0, i), path(1, i), path(2, i));
            out.push_back({ .position = pos, .color = glm::vec3(0.1F, 0.8F, 0.4F), .scalar = 0.5F });
        }

        return out;
    }

    std::vector<SampleResult> sample_fibonacci_sphere(const SamplerBounds& b, size_t count)
    {
        const float phi = glm::pi<float>() * (3.0F - std::sqrt(5.0F));
        const float mr = b.max_radius();
        const glm::vec3 ext = b.extent();
        std::vector<SampleResult> out;
        out.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const float y = 1.0F - (static_cast<float>(i) / static_cast<float>(count - 1)) * 2.0F;
            const float radius = std::sqrt(1.0F - y * y);
            const float theta = phi * static_cast<float>(i);
            const glm::vec3 pos = b.center() + mr * glm::vec3(std::cos(theta) * radius, y, std::sin(theta) * radius);
            out.push_back({ .position = pos, .color = (pos - b.min) / ext, .scalar = 1.0F });
        }

        return out;
    }

    std::vector<SampleResult> sample_fibonacci_spiral(const SamplerBounds& b, size_t count)
    {
        const float golden_angle = glm::pi<float>() * (3.0F - std::sqrt(5.0F));
        const float mr = b.max_radius();
        std::vector<SampleResult> out;
        out.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const float r = mr * std::sqrt(static_cast<float>(i) / static_cast<float>(count));
            const float theta = static_cast<float>(i) * golden_angle;
            const glm::vec3 pos = b.center() + glm::vec3(r * std::cos(theta), r * std::sin(theta), 0.0F);
            out.push_back({ .position = pos,
                .color = { r / mr, 0.5F, 1.0F - r / mr },
                .scalar = r / mr });
        }

        return out;
    }

    std::vector<SampleResult> sample_lissajous(const SamplerBounds& b, size_t count)
    {
        static constexpr float a = 3.0F, bv = 2.0F, c = 5.0F;
        const float mr = b.max_radius();
        std::vector<SampleResult> out;
        out.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) / static_cast<float>(count)) * glm::two_pi<float>() * 2.0F;
            const glm::vec3 pos = b.center() + mr * glm::vec3(std::sin(a * t), std::sin(bv * t), std::sin(c * t));
            out.push_back({ .position = pos,
                .color = { 0.5F + pos.x, 0.5F, 0.8F },
                .scalar = 1.0F });
        }

        return out;
    }

    std::vector<SampleResult> sample_torus(const SamplerBounds& b, size_t count)
    {
        const float mr = b.max_radius();
        const float main_r = mr * 0.7F;
        const float tube_r = mr * 0.3F;
        std::vector<SampleResult> out;
        out.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const float u = (static_cast<float>(i) / static_cast<float>(count)) * glm::two_pi<float>();
            const float v = (static_cast<float>(i * 7 % count) / static_cast<float>(count)) * glm::two_pi<float>();
            const glm::vec3 pos = b.center() + glm::vec3((main_r + tube_r * std::cos(v)) * std::cos(u), (main_r + tube_r * std::cos(v)) * std::sin(u), tube_r * std::sin(v));
            const glm::vec3 color { (std::cos(u) + 1.0F) * 0.5F, (std::cos(v) + 1.0F) * 0.5F, (std::sin(u) + 1.0F) * 0.5F };
            out.push_back({ .position = pos, .color = color, .scalar = (std::cos(v) + 1.0F) * 0.5F });
        }

        return out;
    }

} // anonymous namespace

//-----------------------------------------------------------------------------

std::vector<SampleResult> generate_samples(
    SpatialDistribution dist,
    size_t count,
    const SamplerBounds& bounds,
    Stochastic::Stochastic& rng)
{
    if (count == 0 || dist == SpatialDistribution::EMPTY) {
        return {};
    }

    switch (dist) {
    case SpatialDistribution::PERLIN_FIELD:
        return sample_perlin_field(bounds, count, rng);
    case SpatialDistribution::BROWNIAN_PATH:
        return sample_brownian_path(bounds, count, rng);
    case SpatialDistribution::STRATIFIED_CUBE:
        return sample_stratified_cube(bounds, count, rng);
    case SpatialDistribution::SPLINE_PATH:
        return sample_spline_path(bounds, count, rng);
    case SpatialDistribution::FIBONACCI_SPHERE:
        return sample_fibonacci_sphere(bounds, count);
    case SpatialDistribution::FIBONACCI_SPIRAL:
        return sample_fibonacci_spiral(bounds, count);
    case SpatialDistribution::TORUS:
        return sample_torus(bounds, count);
    case SpatialDistribution::LISSAJOUS:
        return sample_lissajous(bounds, count);

    default: {
        std::vector<SampleResult> out;
        out.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            out.push_back(generate_sample_at(dist, i, count, bounds, rng));
        }
        return out;
    }
    }
}

SampleResult generate_sample_at(
    SpatialDistribution dist,
    size_t index,
    size_t total,
    const SamplerBounds& bounds,
    Stochastic::Stochastic& rng)
{
    switch (dist) {
    case SpatialDistribution::RANDOM_VOLUME:
        return sample_random_volume(bounds, rng);
    case SpatialDistribution::RANDOM_SURFACE:
        return sample_random_surface(bounds, rng);
    case SpatialDistribution::GRID:
        return sample_grid(bounds, index, total);
    case SpatialDistribution::SPHERE_VOLUME:
        return sample_sphere_volume(bounds, rng);
    case SpatialDistribution::SPHERE_SURFACE:
        return sample_sphere_surface(bounds, rng);
    case SpatialDistribution::UNIFORM_GRID:
        return sample_uniform_grid(bounds, index, total);
    case SpatialDistribution::RANDOM_SPHERE:
        return sample_random_sphere(bounds, rng);
    case SpatialDistribution::RANDOM_CUBE:
        return sample_random_cube(bounds, rng);
    default:
        return { .position = glm::vec3(0.0F), .color = glm::vec3(0.5F), .scalar = 0.5F };
    }
}

std::vector<Nodes::PointVertex> to_point_vertices(
    std::span<const SampleResult> samples,
    glm::vec2 size_range)
{
    std::vector<Nodes::PointVertex> out;
    out.reserve(samples.size());
    for (const auto& s : samples) {
        out.push_back(to_point_vertex(s, size_range));
    }
    return out;
}

std::vector<Nodes::LineVertex> to_line_vertices(
    std::span<const SampleResult> samples,
    glm::vec2 thickness_range)
{
    std::vector<Nodes::LineVertex> out;
    out.reserve(samples.size());
    for (const auto& s : samples) {
        out.push_back(to_line_vertex(s, thickness_range));
    }
    return out;
}

} // namespace MayaFlux::Kinesis
