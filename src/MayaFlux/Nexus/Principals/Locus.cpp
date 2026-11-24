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
    m_controls.position.observe(m_nav.eye);
    m_view = Kinesis::compute_view_transform(m_nav, aspect());
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
    m_controls.position.observe(m_nav.eye);
    m_view = Kinesis::compute_view_transform(m_nav, aspect());
}

LocusControls& Locus::open_locus()
{
    auto& controls = extend<LocusControls>();
    controls.aspect.observe(1.0F);
    return controls;
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

    if (attached()) {
        if (const auto placed = position()) {
            m_nav.eye = *placed;
        }
    }

    if (const auto step = timestep()) {
        Kinesis::advance_navigation(m_nav, *step);
    } else {
        Kinesis::advance_navigation(m_nav);
    }
    m_view = Kinesis::build_view_transform(m_nav, aspect());
    m_controls.position.observe(m_nav.eye);
    m_controls.orientation.observe(
        glm::angleAxis(m_nav.yaw, glm::vec3(0.0F, 1.0F, 0.0F))
        * glm::angleAxis(-m_nav.pitch, glm::vec3(1.0F, 0.0F, 0.0F)));
}

void Locus::invoke_influence(const InfluenceContext& ctx) const
{
    for (const auto& proc : m_view_targets) {
        if (proc) {
            proc->set_view_transform(m_view);
        }
    }

    Agent::invoke_influence(ctx);
}

} // namespace MayaFlux::Nexus
