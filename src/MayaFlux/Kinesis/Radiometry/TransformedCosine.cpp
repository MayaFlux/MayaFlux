#include "TransformedCosine.hpp"

#include "Microfacet.hpp"

#include "MayaFlux/Kinesis/ForEachIndexed.hpp"
#include "MayaFlux/Kinesis/Stochastic/Directions.hpp"

namespace MayaFlux::Kinesis {

namespace {

    /** The lobe being fitted: a frame about its mean direction and three free terms. */
    struct Lobe {
        glm::mat3 frame { 1.0F };
        glm::vec3 terms { 1.0F, 1.0F, 0.0F };

        [[nodiscard]] glm::mat3 matrix() const
        {
            return frame
                * glm::mat3(glm::vec3(terms.x, 0.0F, 0.0F), glm::vec3(0.0F, terms.y, 0.0F), glm::vec3(terms.z, 0.0F, 1.0F));
        }
    };

    struct Target {
        glm::vec3 view;
        float alpha {};
        float albedo {};
        uint32_t samples {};
        bool isotropic {};

        [[nodiscard]] glm::vec2 grid(uint32_t i, uint32_t j) const
        {
            return { (static_cast<float>(i) + 0.5F) / static_cast<float>(samples),
                (static_cast<float>(j) + 0.5F) / static_cast<float>(samples) };
        }

        /** Specular value times cosine at unit Fresnel, and its sampling density. */
        [[nodiscard]] std::pair<float, float> specular(const glm::vec3& source) const
        {
            if (!(source.z > 0.0F)) {
                return { 0.0F, 0.0F };
            }
            const glm::vec3 half = glm::normalize(view + source);
            return {
                ggx_density(half.z, alpha) * ggx_visibility(view.z, source.z, alpha) * source.z,
                ggx_reflected_density(view.z, half.z, alpha)
            };
        }

        [[nodiscard]] glm::vec3 specular_sample(const glm::vec2& u) const
        {
            return glm::reflect(-view, ggx_visible_normal(view, alpha, u));
        }
    };

    Lobe shaped(const Lobe& lobe, const Target& target, const glm::vec3& terms)
    {
        Lobe out = lobe;
        out.terms.x = std::max(terms.x, 1.0e-7F);
        out.terms.y = target.isotropic ? out.terms.x : std::max(terms.y, 1.0e-7F);
        out.terms.z = target.isotropic ? 0.0F : terms.z;
        return out;
    }

    double error_of(const Lobe& lobe, const Target& target)
    {
        const glm::mat3 matrix = lobe.matrix();
        const glm::mat3 inverse = glm::inverse(matrix);

        double error = 0.0;
        const auto add = [&](const glm::vec3& source) {
            const auto [value, density] = target.specular(source);
            const float lobe_density = transformed_cosine_density(inverse, source);
            const double apart = std::abs(static_cast<double>(value) - static_cast<double>(target.albedo * lobe_density));
            const double both = static_cast<double>(lobe_density) + static_cast<double>(density);
            if (both > 0.0) {
                error += apart * apart * apart / both;
            }
        };
        for (uint32_t j = 0; j < target.samples; ++j) {
            for (uint32_t i = 0; i < target.samples; ++i) {
                const glm::vec2 u = target.grid(i, j);
                add(glm::normalize(matrix * Stochastic::square_to_cosine_hemisphere(u)));
                add(target.specular_sample(u));
            }
        }
        return std::isfinite(error) ? error / (target.samples * target.samples)
                                    : std::numeric_limits<double>::max();
    }

    /** Nelder and Mead over the three terms. */
    Lobe refined(const Lobe& start, const Target& target)
    {
        constexpr float reach = 0.05F;
        std::array<std::pair<glm::vec3, double>, 4> simplex {};
        const auto measure = [&](const glm::vec3& terms) {
            return std::pair { terms, error_of(shaped(start, target, terms), target) };
        };
        simplex[0] = measure(start.terms);
        for (int i = 0; i < 3; ++i) {
            glm::vec3 moved = start.terms;
            moved[i] += reach;
            simplex[static_cast<size_t>(i) + 1] = measure(moved);
        }

        for (int iteration = 0; iteration < 100; ++iteration) {
            std::ranges::sort(simplex, {}, &std::pair<glm::vec3, double>::second);
            if (2.0 * std::abs(simplex[0].second - simplex[3].second)
                <= (std::abs(simplex[0].second) + std::abs(simplex[3].second)) * 1.0e-5) {
                break;
            }

            const glm::vec3 centre = (simplex[0].first + simplex[1].first + simplex[2].first) / 3.0F;
            const auto reflected = measure(centre + (centre - simplex[3].first));
            if (reflected.second < simplex[0].second) {
                const auto expanded = measure(centre + 2.0F * (centre - simplex[3].first));
                simplex[3] = expanded.second < reflected.second ? expanded : reflected;
            } else if (reflected.second < simplex[2].second) {
                simplex[3] = reflected;
            } else {
                const auto contracted = measure(centre + 0.5F * (simplex[3].first - centre));
                if (contracted.second < simplex[3].second) {
                    simplex[3] = contracted;
                } else {
                    for (size_t i = 1; i < simplex.size(); ++i) {
                        simplex[i] = measure(simplex[0].first + 0.5F * (simplex[i].first - simplex[0].first));
                    }
                }
            }
        }
        std::ranges::sort(simplex, {}, &std::pair<glm::vec3, double>::second);
        return shaped(start, target, simplex[0].first);
    }

    /** Fit one entry, starting from the lobe of a neighbouring one. */
    Lobe fitted(uint32_t column, uint32_t row, uint32_t size, uint32_t samples, Lobe lobe)
    {
        const float x = static_cast<float>(column) / static_cast<float>(size - 1);
        const float theta = std::min(std::acos(1.0F - x * x), 1.57F);
        const float roughness = static_cast<float>(row) / static_cast<float>(size - 1);

        Target target {
            .view = { std::sin(theta), 0.0F, std::cos(theta) },
            .alpha = roughness_to_alpha(roughness),
            .samples = samples,
            .isotropic = column == 0
        };

        glm::vec3 mean(0.0F);
        double albedo = 0.0;
        for (uint32_t j = 0; j < samples; ++j) {
            for (uint32_t i = 0; i < samples; ++i) {
                const glm::vec3 source = target.specular_sample(target.grid(i, j));
                const auto [value, density] = target.specular(source);
                if (density > 0.0F) {
                    albedo += value / density;
                    mean += source * (value / density);
                }
            }
        }
        target.albedo = static_cast<float>(albedo / (samples * samples));

        mean.y = 0.0F;
        if (target.isotropic || !(glm::length(mean) > 1.0e-6F)) {
            lobe.frame = glm::mat3(1.0F);
        } else {
            const glm::vec3 z = glm::normalize(mean);
            lobe.frame = glm::mat3(glm::vec3(z.z, 0.0F, -z.x), glm::vec3(0.0F, 1.0F, 0.0F), z);
        }
        return refined(lobe, target);
    }

    glm::vec4 packed(const Lobe& lobe)
    {
        const glm::mat3 inverse = glm::inverse(lobe.matrix());
        const float middle = inverse[1][1];
        return glm::vec4(inverse[0][0], inverse[0][2], inverse[2][0], inverse[2][2]) / middle;
    }

}

std::vector<glm::vec4> fit_transformed_cosines(uint32_t size, uint32_t samples)
{
    if (size < 2 || samples == 0) {
        return {};
    }

    std::vector<Lobe> first(size);
    Lobe previous;
    for (uint32_t row = size; row-- > 0;) {
        previous = fitted(0, row, size, samples, previous);
        first[row] = previous;
    }

    std::vector<glm::vec4> table(static_cast<size_t>(size) * size);
    detail::for_each_indexed(
        std::execution::par, std::span(first),
        [&table, size, samples](size_t row, const Lobe& start) {
            Lobe lobe = start;
            table[row * size] = packed(lobe);
            for (uint32_t column = 1; column < size; ++column) {
                lobe = fitted(column, static_cast<uint32_t>(row), size, samples, lobe);
                table[row * size + column] = packed(lobe);
            }
        },
        2);
    return table;
}

} // namespace MayaFlux::Kinesis
