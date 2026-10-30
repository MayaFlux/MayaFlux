#include "Stagecraft.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/Nexus/Tapestry.hpp"

namespace MayaFlux {

std::shared_ptr<Nexus::Fabric> create_fabric(const std::string& name, float cell_size)
{
    auto fabric = std::make_shared<Nexus::Fabric>(
        *get_scheduler(), *get_event_manager(), *get_buffer_manager(), cell_size);
    fabric->set_name(name);
    return fabric;
}

std::shared_ptr<Nexus::Tapestry> create_tapestry()
{
    return std::make_shared<Nexus::Tapestry>(get_scheduler(), get_event_manager(), get_buffer_manager());
}

} // namespace MayaFlux
