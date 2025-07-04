#include "SubsystemManager.hpp"

namespace MayaFlux::Core {

SubsystemManager::SubsystemManager(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager)
    : m_node_graph_manager(node_graph_manager)
    , m_buffer_manager(buffer_manager)
{
}

void SubsystemManager::Initialize()
{
    for (const auto& [token, subsystem] : m_subsystems) {
        if (subsystem) {
            subsystem->initialize(m_node_graph_manager, m_buffer_manager);
        }
    }
}

void SubsystemManager::start_all_subsystems()
{
    for (auto& [token, subsystem] : m_subsystems) {
        if (subsystem->is_ready()) {
            subsystem->start();
        }
    }
}

void SubsystemManager::add_subsystem(SubsystemTokens tokens, std::unique_ptr<ISubsystem> subsystem)
{
    subsystem->initialize(m_node_graph_manager, m_buffer_manager);
    subsystem->register_callbacks();
    m_subsystems[tokens] = std::move(subsystem);
}

void SubsystemManager::remove_subsystem(SubsystemTokens tokens)
{
    if (auto it = m_subsystems.find(tokens); it != m_subsystems.end()) {
        it->second->shutdown();
        m_subsystems.erase(it);
    }
}

std::unordered_map<SubsystemTokens, bool> SubsystemManager::query_subsystem_status() const
{
    std::unordered_map<SubsystemTokens, bool> statuses;
    for (const auto& [token, subsystem] : m_subsystems) {
        statuses[token] = subsystem->is_ready();
    }
    return statuses;
}

void SubsystemManager::shutdown()
{
    for (auto& [token, subsystem] : m_subsystems) {
        subsystem->shutdown();
    }
    m_subsystems.clear();
}

}
