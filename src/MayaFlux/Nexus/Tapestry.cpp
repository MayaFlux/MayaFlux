#include "Tapestry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Nexus {

Tapestry::Tapestry(
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    std::shared_ptr<Vruta::EventManager> event_manager)
    : m_scheduler(std::move(scheduler))
    , m_event_manager(std::move(event_manager))
{
}

Tapestry::~Tapestry() = default;

std::shared_ptr<Fabric> Tapestry::create_fabric(float cell_size)
{
    auto fabric = std::make_shared<Fabric>(*m_scheduler, *m_event_manager, cell_size);
    m_fabrics.push_back(fabric);
    return fabric;
}

std::shared_ptr<Fabric> Tapestry::create_fabric(std::string name, float cell_size)
{
    if (m_named_fabrics.contains(name)) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Tapestry::create_fabric: name '{}' already registered, returning existing", name);
        return m_named_fabrics[name].lock();
    }

    auto fabric = std::make_shared<Fabric>(*m_scheduler, *m_event_manager, cell_size);
    m_fabrics.push_back(fabric);
    m_named_fabrics.emplace(std::move(name), fabric);
    return fabric;
}

void Tapestry::remove_fabric(const std::shared_ptr<Fabric>& fabric)
{
    auto it = std::ranges::find(m_fabrics, fabric);
    if (it == m_fabrics.end()) {
        return;
    }
    m_fabrics.erase(it);

    for (auto nit = m_named_fabrics.begin(); nit != m_named_fabrics.end();) {
        if (nit->second.lock() == fabric) {
            nit = m_named_fabrics.erase(nit);
        } else {
            ++nit;
        }
    }
}

void Tapestry::remove_fabric(std::string_view name)
{
    auto nit = m_named_fabrics.find(std::string(name));
    if (nit == m_named_fabrics.end()) {
        return;
    }
    auto fabric = nit->second.lock();
    m_named_fabrics.erase(nit);
    if (fabric) {
        std::erase(m_fabrics, fabric);
    }
}

std::shared_ptr<Fabric> Tapestry::get_fabric(std::string_view name) const
{
    auto it = m_named_fabrics.find(std::string(name));
    return it != m_named_fabrics.end() ? it->second.lock() : nullptr;
}

const std::vector<std::shared_ptr<Fabric>>& Tapestry::all_fabrics() const
{
    return m_fabrics;
}

void Tapestry::commit_all()
{
    for (auto& fabric : m_fabrics) {
        fabric->commit();
    }
}

} // namespace MayaFlux::Nexus
