#include "Prefilter.hpp"

#include "Microfacet.hpp"

#include "MayaFlux/Kinesis/ForEachIndexed.hpp"
#include "MayaFlux/Kinesis/Stochastic/Directions.hpp"
#include "MayaFlux/Kinesis/Stochastic/Sequences.hpp"

namespace MayaFlux::Kinesis {

namespace {

    /** A grid and its successive halvings, so wide lobes read averaged radiance. */
    struct Pyramid {
        struct Level {
            std::vector<float> pixels;
            uint32_t width {};
            uint32_t height {};

            [[nodiscard]] RadianceGrid grid() const { return { pixels, width, height, 3 }; }
        };

        std::vector<Level> levels;

        explicit Pyramid(const RadianceGrid& source)
        {
            Level base { .width = source.width, .height = source.height };
            base.pixels.resize(static_cast<size_t>(source.width) * source.height * 3);
            for (size_t i = 0; i < static_cast<size_t>(source.width) * source.height; ++i) {
                for (size_t c = 0; c < 3; ++c) {
                    base.pixels[i * 3 + c] = source.pixels[i * source.channels + c];
                }
            }
            levels.push_back(std::move(base));

            while (levels.back().width > 2 && levels.back().height > 1) {
                const Level& above = levels.back();
                Level next { .width = above.width / 2, .height = above.height / 2 };
                next.pixels.resize(static_cast<size_t>(next.width) * next.height * 3);
                for (uint32_t y = 0; y < next.height; ++y) {
                    for (uint32_t x = 0; x < next.width; ++x) {
                        for (size_t c = 0; c < 3; ++c) {
                            const auto at = [&](uint32_t px, uint32_t py) {
                                return above.pixels[(static_cast<size_t>(py) * above.width + px) * 3 + c];
                            };
                            next.pixels[(static_cast<size_t>(y) * next.width + x) * 3 + c] = 0.25F
                                * (at(2 * x, 2 * y) + at(2 * x + 1, 2 * y)
                                    + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1));
                        }
                    }
                }
                levels.push_back(std::move(next));
            }
        }

        [[nodiscard]] glm::vec3 at(const glm::vec3& direction, float level) const
        {
            const float held = std::clamp(level, 0.0F, static_cast<float>(levels.size() - 1));
            const auto low = static_cast<size_t>(held);
            const size_t high = std::min(low + 1, levels.size() - 1);
            return glm::mix(radiance_at(levels[low].grid(), direction),
                radiance_at(levels[high].grid(), direction), held - static_cast<float>(low));
        }
    };

}

glm::vec3 radiance_at(const RadianceGrid& grid, const glm::vec3& direction) noexcept
{
    const glm::vec2 uv = grid_coordinate(direction);
    const float x = uv.x * static_cast<float>(grid.width) - 0.5F;
    const float y = std::clamp(uv.y * static_cast<float>(grid.height) - 0.5F,
        0.0F, static_cast<float>(grid.height - 1));
    const float fx = std::floor(x);
    const float fy = std::floor(y);

    const auto columns = static_cast<int64_t>(grid.width);
    const auto x0 = static_cast<size_t>((static_cast<int64_t>(fx) % columns + columns) % columns);
    const size_t x1 = (x0 + 1) % grid.width;
    const auto y0 = static_cast<size_t>(fy);
    const size_t y1 = std::min<size_t>(y0 + 1, grid.height - 1);

    const auto texel = [&grid](size_t px, size_t py) {
        const size_t at = (py * grid.width + px) * grid.channels;
        return glm::vec3(grid.pixels[at], grid.pixels[at + 1], grid.pixels[at + 2]);
    };
    return glm::mix(
        glm::mix(texel(x0, y0), texel(x1, y0), x - fx),
        glm::mix(texel(x0, y1), texel(x1, y1), x - fx),
        y - fy);
}

HarmonicCoefficients project_harmonics(const RadianceGrid& grid)
{
    HarmonicCoefficients result {};
    if (!usable_grid(grid)) {
        return result;
    }

    std::vector<std::array<glm::dvec3, 9>> rows(grid.height);
    detail::for_each_indexed(
        std::execution::par_unseq, std::span(rows),
        [&grid](size_t row, std::array<glm::dvec3, 9>& sums) {
            sums.fill(glm::dvec3(0.0));
            const double top = std::numbers::pi * static_cast<double>(row) / grid.height;
            const double bottom = std::numbers::pi * static_cast<double>(row + 1) / grid.height;
            const double solid = (std::cos(top) - std::cos(bottom)) * 2.0 * std::numbers::pi / grid.width;
            for (uint32_t x = 0; x < grid.width; ++x) {
                const size_t at = (row * grid.width + x) * grid.channels;
                const glm::dvec3 radiance(grid.pixels[at], grid.pixels[at + 1], grid.pixels[at + 2]);
                const auto basis = harmonic_basis(grid_direction({
                    (static_cast<float>(x) + 0.5F) / static_cast<float>(grid.width),
                    (static_cast<float>(row) + 0.5F) / static_cast<float>(grid.height) }));
                for (size_t i = 0; i < basis.size(); ++i) {
                    sums[i] += radiance * (static_cast<double>(basis[i]) * solid);
                }
            }
        },
        16);

    std::array<glm::dvec3, 9> total {};
    total.fill(glm::dvec3(0.0));
    for (const auto& row : rows) {
        for (size_t i = 0; i < total.size(); ++i) {
            total[i] += row[i];
        }
    }
    for (size_t i = 0; i < total.size(); ++i) {
        result[i] = glm::vec3(total[i]);
    }
    return result;
}

std::vector<glm::vec3> prefilter_radiance(
    const RadianceGrid& grid, float roughness, uint32_t width, uint32_t height, uint32_t samples)
{
    if (!usable_grid(grid) || width == 0 || height == 0 || samples == 0) {
        return {};
    }

    const Pyramid source(grid);
    const float texel_solid = 4.0F * std::numbers::pi_v<float>
        / (static_cast<float>(grid.width) * static_cast<float>(grid.height));
    const float output_solid = 4.0F * std::numbers::pi_v<float>
        / (static_cast<float>(width) * static_cast<float>(height));
    const float alpha = roughness_to_alpha(roughness);

    std::vector<glm::vec3> out(static_cast<size_t>(width) * height);
    detail::for_each_indexed(
        std::execution::par_unseq, std::span(out),
        [&](size_t index, glm::vec3& texel) {
            const glm::vec3 normal = grid_direction({
                (static_cast<float>(index % width) + 0.5F) / static_cast<float>(width),
                (static_cast<float>(index / width) + 0.5F) / static_cast<float>(height) });
            if (!(roughness > 0.0F)) {
                texel = source.at(normal, 0.5F * std::log2(std::max(output_solid / texel_solid, 1.0F)));
                return;
            }

            const glm::mat3 frame = Stochastic::orthonormal_basis(normal);
            const glm::vec3 pole(0.0F, 0.0F, 1.0F);
            glm::dvec3 sum(0.0);
            double weight = 0.0;
            for (uint32_t i = 0; i < samples; ++i) {
                const glm::vec3 half = ggx_visible_normal(pole, alpha, Stochastic::hammersley_point(i, samples));
                const glm::vec3 local = glm::reflect(-pole, half);
                if (!(local.z > 0.0F)) {
                    continue;
                }
                const float density = ggx_reflected_density(1.0F, half.z, alpha);
                const float sample_solid = 1.0F / (static_cast<float>(samples) * std::max(density, 1.0e-6F));
                const float level = 0.5F * std::log2(std::max(sample_solid / texel_solid, 1.0F));
                sum += glm::dvec3(source.at(frame * local, level)) * static_cast<double>(local.z);
                weight += local.z;
            }
            texel = weight > 0.0 ? glm::vec3(sum / weight) : source.at(normal, 0.0F);
        },
        64);
    return out;
}

std::vector<glm::vec2> specular_albedo_table(uint32_t size, uint32_t samples)
{
    if (size == 0 || samples == 0) {
        return {};
    }

    std::vector<glm::vec2> table(static_cast<size_t>(size) * size);
    detail::for_each_indexed(
        std::execution::par_unseq, std::span(table),
        [size, samples](size_t index, glm::vec2& entry) {
            const float cos_view = (static_cast<float>(index % size) + 0.5F) / static_cast<float>(size);
            const float roughness = (static_cast<float>(index / size) + 0.5F) / static_cast<float>(size);
            const float alpha = roughness_to_alpha(roughness);
            const glm::vec3 view(std::sqrt(1.0F - cos_view * cos_view), 0.0F, cos_view);
            const float masking = ggx_masking(cos_view, alpha);

            double scale = 0.0;
            double bias = 0.0;
            for (uint32_t i = 0; i < samples; ++i) {
                const glm::vec3 half = ggx_visible_normal(view, alpha, Stochastic::hammersley_point(i, samples));
                const glm::vec3 source = glm::reflect(-view, half);
                if (!(source.z > 0.0F)) {
                    continue;
                }
                const float weight = 4.0F * ggx_visibility(cos_view, source.z, alpha)
                    * cos_view * source.z / masking;
                const float m = std::clamp(1.0F - glm::dot(view, half), 0.0F, 1.0F);
                const float fresnel = m * m * m * m * m;
                scale += weight * (1.0F - fresnel);
                bias += weight * fresnel;
            }
            entry = { static_cast<float>(scale / samples), static_cast<float>(bias / samples) };
        },
        64);
    return table;
}

SurroundRadiance prefilter_surround(const RadianceGrid& grid, uint32_t samples)
{
    SurroundRadiance surround;
    if (!usable_grid(grid)) {
        return surround;
    }

    surround.harmonics = project_harmonics(grid);
    for (uint32_t level = 0; level < k_surround_levels; ++level) {
        surround.levels[level] = prefilter_radiance(grid,
            static_cast<float>(level) / static_cast<float>(k_surround_levels - 1),
            k_surround_width >> level, k_surround_height >> level, samples);
    }
    surround.albedo = specular_albedo_table(k_albedo_table_size, samples);
    return surround;
}

} // namespace MayaFlux::Kinesis
