#include "Stagecraft.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/Nexus/Tapestry.hpp"

namespace MayaFlux {

std::shared_ptr<Nexus::Fabric> create_fabric(const std::string& name, float cell_size)
{
    auto fabric = std::make_shared<Nexus::Fabric>(
        get_scheduler(), get_event_manager(), get_buffer_manager(), cell_size);
    fabric->set_name(name);
    return fabric;
}

std::shared_ptr<Nexus::Tapestry> create_tapestry()
{
    return std::make_shared<Nexus::Tapestry>(get_scheduler(), get_event_manager(), get_buffer_manager());
}

std::shared_ptr<Nexus::StateOrchestrator> create_orchestrator(const std::shared_ptr<Nexus::Tapestry>& tapestry)
{
    return std::make_shared<Nexus::StateOrchestrator>(get_scheduler(), tapestry);
}

std::tuple<
    std::shared_ptr<Kakshya::MeshAssemblyContainer>,
    std::shared_ptr<Kriya::MeshAssemblyRealization>,
    Nexus::StateOrchestrator::Rows>
orchestrate_chosen_model(const std::shared_ptr<Nexus::StateOrchestrator>& orchestrator)
{
    if (!orchestrator) {
        return {};
    }
    const std::string path = choose_model_path();
    if (path.empty()) {
        return {};
    }
    return orchestrator->orchestrate(path);
}

} // namespace MayaFlux
