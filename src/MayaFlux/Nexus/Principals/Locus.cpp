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
    open_view();
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
    open_view();
}

LocusControls& Locus::open_locus()
{
    auto& controls = extend<LocusControls>();
    controls.aspect.observe(1.0F);
    return controls;
}

Locus::~Locus()
{
    m_held->leave(m_driver);
}

void Locus::open_view()
{
    join_view(std::make_shared<Memory::OwnedCell<Kinesis::Viewpoint>>(Kinesis::viewpoint_of(m_nav)));
    m_held->hand_to(m_driver);
}

void Locus::join_view(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view)
{
    m_held = view;
    m_driver = m_held->join([this](const Kinesis::Viewpoint& current) {
        Kinesis::set_viewpoint(m_nav, current);
    });
}

void Locus::set_view(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view)
{
    if (!view || view == m_held) {
        return;
    }
    m_held->leave(m_driver);
    join_view(view);
    for (const auto& proc : m_view_targets) {
        proc->set_view_cell(m_held);
    }
}

void Locus::add_view_target(std::shared_ptr<Buffers::RenderProcessor> proc)
{
    if (!proc) {
        return;
    }
    if (std::ranges::find(m_view_targets, proc) != m_view_targets.end()) {
        return;
    }
    proc->set_view_cell(m_held);
    m_view_targets.push_back(std::move(proc));
}

void Locus::remove_view_target(const std::shared_ptr<Buffers::RenderProcessor>& proc)
{
    if (std::erase(m_view_targets, proc) > 0 && proc->get_view_cell() == m_held) {
        proc->set_view_cell(nullptr);
    }
}

void Locus::invoke_perception(const PerceptionContext& ctx)
{
    Agent::invoke_perception(ctx);

    if (!m_held->drives(m_driver)) {
        const auto [placement, frustum] = m_held->read();
        m_view = Kinesis::view_from_frustum(placement, frustum, aspect());
        m_controls.position.observe(glm::vec3(placement[3]));
        m_controls.orientation.observe(glm::normalize(glm::quat_cast(glm::mat3(placement))));
        return;
    }

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
    const auto viewpoint = Kinesis::viewpoint_of(m_nav);
    m_held->write(m_driver, viewpoint);
    m_controls.position.observe(m_nav.eye);
    m_controls.orientation.observe(glm::normalize(glm::quat_cast(glm::mat3(viewpoint.first))));
}

void Locus::invoke_influence(const InfluenceContext& ctx) const
{
    Agent::invoke_influence(ctx);
}

} // namespace MayaFlux::Nexus
