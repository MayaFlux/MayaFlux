#include "Locus.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

namespace MayaFlux::Nexus {

Locus::Locus(const Kinesis::NavigationConfig& config,
    float query_radius,
    PerceptionFn perception,
    InfluenceFn influence)
    : Agent(query_radius, std::move(perception), std::move(influence))
    , m_nav(Kinesis::make_navigation_state(config))
{
    Agent::set_position(m_nav.eye);
    m_view = Kinesis::compute_view_transform(m_nav, m_aspect);
}

Locus::Locus(const Kinesis::NavigationConfig& config,
    float query_radius,
    std::string perception_fn_name, PerceptionFn perception,
    std::string influence_fn_name, InfluenceFn influence)
    : Agent(query_radius,
          std::move(perception_fn_name), std::move(perception),
          std::move(influence_fn_name), std::move(influence))
    , m_nav(Kinesis::make_navigation_state(config))
{
    Agent::set_position(m_nav.eye);
    m_view = Kinesis::compute_view_transform(m_nav, m_aspect);
}

void Locus::add_view_target(std::shared_ptr<Buffers::RenderProcessor> proc)
{
    if (!proc) {
        return;
    }
    if (std::ranges::find(m_view_targets, proc) != m_view_targets.end()) {
        return;
    }
    m_view_targets.push_back(std::move(proc));
}

void Locus::remove_view_target(const std::shared_ptr<Buffers::RenderProcessor>& proc)
{
    std::erase(m_view_targets, proc);
}

void Locus::invoke_perception(const PerceptionContext& ctx)
{
    Agent::invoke_perception(ctx);

    m_view = Kinesis::compute_view_transform(m_nav, m_aspect);
    Agent::set_position(m_nav.eye);
}

void Locus::invoke_influence(const InfluenceContext& ctx) const
{
    Agent::invoke_influence(ctx);

    for (const auto& proc : m_view_targets) {
        if (proc) {
            proc->set_view_transform(m_view);
        }
    }
}

} // namespace MayaFlux::Nexus
