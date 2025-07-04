#include "SubsystemManager.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"

namespace MayaFlux::Core {

SubsystemManager::SubsystemManager(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager)
    : m_node_graph_manager(node_graph_manager)
    , m_buffer_manager(buffer_manager)
{
}

void SubsystemManager::add_subsystem(SubsystemTokens tokens, std::unique_ptr<ISubsystem> subsystem)
{
    auto handle = std::make_unique<SubsystemProcessingHandle>(
        m_buffer_manager, m_node_graph_manager, tokens);

    subsystem->initialize(*handle);
    subsystem->register_callbacks();

    m_handles[tokens] = std::move(handle);
    m_subsystems[tokens] = std::move(subsystem);
}

void SubsystemManager::start_all_subsystems()
{
    for (auto& [token, subsystem] : m_subsystems) {
        if (subsystem->is_ready()) {
            subsystem->start();
        }
    }
}

void SubsystemManager::remove_subsystem(SubsystemTokens tokens)
{
    if (auto it = m_subsystems.find(tokens); it != m_subsystems.end()) {
        it->second->shutdown();
        m_subsystems.erase(it);
        m_handles.erase(tokens);
        m_cross_access_permissions.erase(tokens);
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

bool SubsystemManager::is_cross_access_allowed(SubsystemTokens from, SubsystemTokens to) const
{
    auto it = m_cross_access_permissions.find(from);
    return it != m_cross_access_permissions.end() && it->second.count(to) > 0;
}

void SubsystemManager::allow_cross_access(SubsystemTokens from, SubsystemTokens to)
{
    std::unique_lock lock(m_mutex);
    m_cross_access_permissions[from].insert(to);
}

std::optional<std::span<const double>> SubsystemManager::read_cross_subsystem_buffer(
    SubsystemTokens requesting_tokens,
    SubsystemTokens target_tokens,
    u_int32_t channel)
{

    std::shared_lock lock(m_mutex);

    if (m_subsystems.find(requesting_tokens) == m_subsystems.end()) {
        return std::nullopt;
    }

    if (!is_cross_access_allowed(requesting_tokens, target_tokens)) {
        return std::nullopt;
    }

    try {
        return m_buffer_manager->get_buffer_data(target_tokens.Buffer, channel);
    } catch (...) {
        return std::nullopt;
    }
}
}
