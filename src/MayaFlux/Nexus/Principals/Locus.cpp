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
    m_placement.position.observe(m_nav.eye);
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
    m_placement.position.observe(m_nav.eye);
    m_view = Kinesis::compute_view_transform(m_nav, aspect());
    open_view();
}

LocusControls& Locus::open_locus()
{
    auto& controls = extend<LocusControls>();
    controls.aspect.observe(1.0F);
    return controls;
}

bool Locus::follow(std::function<Kinesis::Viewpoint()> source)
{
    if (!source) {
        return false;
    }
    unfollow();

    const auto mark = std::make_shared<char>();
    auto guarded = [source = std::move(source), mark, stood = m_held->read()]() -> Kinesis::Viewpoint {
        thread_local std::vector<const void*> reading;
        if (std::ranges::find(reading, mark.get()) != reading.end()) {
            return stood;
        }
        reading.push_back(mark.get());
        auto view = source();
        reading.pop_back();
        return view;
    };

    m_follower = m_held->join({}, std::move(guarded));
    return m_held->hand_to(m_follower);
}

bool Locus::follow(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& other, const glm::mat4& offset)
{
    if (!other || other == m_held) {
        return false;
    }
    const bool followed = follow(
        [held = std::weak_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>(other), offset, stood = m_held->read()]() {
            const auto cell = held.lock();
            if (!cell) {
                return stood;
            }
            const auto [placement, frustum] = cell->read();
            return Kinesis::Viewpoint { placement * offset, frustum };
        });
    if (followed) {
        m_followed = other;
        m_follow_offset = offset;
    }
    return followed;
}

void Locus::unfollow()
{
    if (m_follower == 0) {
        return;
    }
    if (m_held->drives(m_follower)) {
        m_held->hand_to(m_driver);
    }
    m_held->leave(m_follower);
    m_follower = 0;
    m_followed.reset();
}

std::optional<std::pair<std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>, glm::mat4>> Locus::following() const
{
    auto cell = m_followed.lock();
    if (m_follower == 0 || !cell) {
        return std::nullopt;
    }
    return std::pair { std::move(cell), m_follow_offset };
}

Locus::~Locus()
{
    if (m_follower != 0) {
        m_held->leave(m_follower);
    }
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
    unfollow();
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
        m_placement.position.observe(glm::vec3(placement[3]));
        m_placement.orientation.observe(glm::normalize(glm::quat_cast(glm::mat3(placement))));
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
    m_placement.position.observe(m_nav.eye);
    m_placement.orientation.observe(glm::normalize(glm::quat_cast(glm::mat3(viewpoint.first))));
}

void Locus::invoke_influence(const InfluenceContext& ctx) const
{
    Agent::invoke_influence(ctx);
}

} // namespace MayaFlux::Nexus
