#include "Presence.hpp"

#include <glm/gtc/constants.hpp>

namespace MayaFlux::Nexus {

namespace {

    float linear_falloff(float t)
    {
        return 1.0F - std::clamp(t, 0.0F, 1.0F);
    }

} // namespace

Presence::Presence(float query_radius,
    PerceptionFn perception,
    InfluenceFn influence,
    RadiateFn radiate)
    : Agent(query_radius, std::move(perception), std::move(influence))
    , m_falloff(linear_falloff)
    , m_radiate(std::move(radiate))
{
}

Presence::Presence(float query_radius,
    std::string perception_fn_name, PerceptionFn perception,
    std::string influence_fn_name, InfluenceFn influence,
    std::string radiate_fn_name, RadiateFn radiate)
    : Agent(query_radius,
          std::move(perception_fn_name), std::move(perception),
          std::move(influence_fn_name), std::move(influence))
    , m_falloff(linear_falloff)
    , m_radiate_fn_name(std::move(radiate_fn_name))
    , m_radiate(std::move(radiate))
{
}

void Presence::set_falloff_curve(FalloffCurve curve)
{
    m_falloff_curve = curve;
    switch (curve) {
    case FalloffCurve::Linear:
        m_falloff = [](float t) { return 1.0F - std::clamp(t, 0.0F, 1.0F); };
        break;
    case FalloffCurve::Cosine:
        m_falloff = [](float t) { return 0.5F * (1.0F + std::cos(std::clamp(t, 0.0F, 1.0F) * glm::pi<float>())); };
        break;
    case FalloffCurve::Exponential:
        m_falloff = [](float t) { return std::exp(-4.0F * std::clamp(t, 0.0F, 1.0F)); };
        break;
    case FalloffCurve::InverseSquare:
        m_falloff = [](float t) { const float c = std::clamp(t, 0.0F, 1.0F); return 1.0F / (1.0F + 16.0F * c * c); };
        break;
    }
}

void Presence::invoke_perception(const PerceptionContext& ctx)
{
    Agent::invoke_perception(ctx);

    m_neighbors.clear();

    const float fr = falloff_radius();
    const float fr_sq = fr * fr;

    for (const auto& result : ctx.spatial_results) {
        if (result.distance_sq > fr_sq) {
            continue;
        }
        const float t = fr > 0.0F
            ? std::sqrt(result.distance_sq) / fr
            : 0.0F;
        const float weight = m_falloff ? m_falloff(t) : linear_falloff(t);
        m_neighbors.emplace_back(result.id, weight);
    }
}

void Presence::invoke_influence(const InfluenceContext& ctx) const
{
    Agent::invoke_influence(ctx);

    if (!m_radiate) {
        return;
    }

    for (const auto& [id, weight] : m_neighbors) {
        m_radiate(id, weight);
    }
}

} // namespace MayaFlux::Nexus
