#include "SubsystemManager.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"

#include "Subsystems/AudioSubsystem.hpp"

namespace MayaFlux::Core {

SubsystemManager::SubsystemManager(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager)
    : m_node_graph_manager(node_graph_manager)
    , m_buffer_manager(buffer_manager)
{
}

void SubsystemManager::create_audio_subsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type)
{
    create_subsystem_internal<AudioSubsystem>(SubsystemType::AUDIO, stream_info, backend_type);
}

void SubsystemManager::add_subsystem(SubsystemType type, std::shared_ptr<ISubsystem> subsystem)
{
    auto tokens = subsystem->get_tokens();
    auto handle = std::make_unique<SubsystemProcessingHandle>(
        m_buffer_manager, m_node_graph_manager, tokens);

    subsystem->initialize(*handle);
    subsystem->register_callbacks();

    m_subsystems[type] = subsystem;
    m_handles[type] = std::move(handle);
}

std::shared_ptr<AudioSubsystem> SubsystemManager::get_audio_subsystem()
{
    return std::dynamic_pointer_cast<AudioSubsystem>(get_subsystem(SubsystemType::AUDIO));
}

void SubsystemManager::start_all_subsystems()
{
    for (auto& [token, subsystem] : m_subsystems) {
        if (subsystem->is_ready()) {
            subsystem->start();
        }
    }
}

std::shared_ptr<ISubsystem> SubsystemManager::get_subsystem(SubsystemType type)
{
    return has_subsystem(type) ? m_subsystems[type] : nullptr;
}

void SubsystemManager::remove_subsystem(SubsystemType type)
{
    if (auto it = m_subsystems.find(type); it != m_subsystems.end()) {
        it->second->shutdown();
        m_subsystems.erase(it);
        m_handles.erase(type);
        m_cross_access_permissions.erase(type);
    }
}

std::unordered_map<SubsystemType, bool> SubsystemManager::query_subsystem_status() const
{
    std::unordered_map<SubsystemType, bool> statuses;
    for (const auto& [type, subsystem] : m_subsystems) {
        statuses[type] = subsystem->is_ready();
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

bool SubsystemManager::is_cross_access_allowed(SubsystemType from, SubsystemType to) const
{
    auto it = m_cross_access_permissions.find(from);
    return it != m_cross_access_permissions.end() && it->second.count(to) > 0;
}

void SubsystemManager::allow_cross_access(SubsystemType from, SubsystemType to)
{
    std::unique_lock lock(m_mutex);
    m_cross_access_permissions[from].insert(to);
}

std::optional<std::span<const double>> SubsystemManager::read_cross_subsystem_buffer(
    SubsystemType requesting_type,
    SubsystemType target_type,
    u_int32_t channel)
{

    std::shared_lock lock(m_mutex);

    if (m_subsystems.find(requesting_type) == m_subsystems.end()) {
        return std::nullopt;
    }

    if (!is_cross_access_allowed(requesting_type, target_type)) {
        return std::nullopt;
    }

    auto target_token = m_subsystems[target_type]->get_tokens();

    try {
        return m_buffer_manager->get_buffer_data(target_token.Buffer, channel);
    } catch (...) {
        return std::nullopt;
    }
}

void SubsystemManager::register_process_hook(SubsystemType type, const std::string& name, ProcessHook hook, HookPosition position)
{
    auto handle = m_subsystems[type]->get_processing_context_handle();

    if (position == HookPosition::PRE_PROCESS) {
        handle->pre_process_hooks[name] = std::move(hook);
    } else {
        handle->post_process_hooks[name] = std::move(hook);
    }
}

void SubsystemManager::unregister_process_hook(SubsystemType type, const std::string& name)
{
    auto handle = m_subsystems[type]->get_processing_context_handle();
    auto pre_it = handle->pre_process_hooks.find(name);
    if (pre_it != handle->pre_process_hooks.end()) {
        handle->pre_process_hooks.erase(pre_it);
        return;
    }

    auto post_it = handle->post_process_hooks.find(name);
    if (post_it != handle->post_process_hooks.end()) {
        handle->post_process_hooks.erase(post_it);
    }
}

bool SubsystemManager::has_process_hook(SubsystemType type, const std::string& name)
{
    auto handle = m_subsystems[type]->get_processing_context_handle();

    return handle->pre_process_hooks.find(name) != handle->pre_process_hooks.end() || handle->post_process_hooks.find(name) != handle->post_process_hooks.end();
}
}
