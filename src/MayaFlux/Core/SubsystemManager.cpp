#include "SubsystemManager.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"

#include "Subsystems/AudioSubsystem.hpp"

#include "Subsystems/GraphicsSubsystem.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

SubsystemManager::SubsystemManager(
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
    std::shared_ptr<Core::WindowManager> window_manager)
    : m_node_graph_manager(std::move(node_graph_manager))
    , m_buffer_manager(std::move(buffer_manager))
    , m_task_scheduler(std::move(task_scheduler))
    , m_window_manager(std::move(window_manager))
{
    if (!m_node_graph_manager) {
        fatal(Journal::Component::Core, Journal::Context::Init,
            std::source_location::current(),
            "SubsystemManager requires valid NodeGraphManager");
    }
    if (!m_buffer_manager) {
        fatal(Journal::Component::Core, Journal::Context::Init,
            std::source_location::current(),
            "SubsystemManager requires valid BufferManager");
    }
    if (!m_task_scheduler) {
        fatal(Journal::Component::Core, Journal::Context::Init,
            std::source_location::current(),
            "SubsystemManager requires valid TaskScheduler");
    }
    if (!m_window_manager) {
        MF_WARN(Journal::Component::Core, Journal::Context::Init,
            "No WindowManager provided - Graphics subsystems will be unavailable");
    }
}

void SubsystemManager::create_audio_subsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type)
{
    create_subsystem_internal<AudioSubsystem>(SubsystemType::AUDIO, stream_info, backend_type);
}

void SubsystemManager::create_graphics_subsystem(const GlobalGraphicsConfig& graphics_config)
{
    if (!m_window_manager) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create GraphicsSubsystem without a valid WindowManager");
    }

    create_subsystem_internal<GraphicsSubsystem>(SubsystemType::GRAPHICS, graphics_config);
}

void SubsystemManager::add_subsystem(SubsystemType type, const std::shared_ptr<ISubsystem>& subsystem)
{
    auto tokens = subsystem->get_tokens();
    auto handle = std::make_unique<SubsystemProcessingHandle>(
        m_buffer_manager,
        m_node_graph_manager,
        m_task_scheduler,
        m_window_manager,
        tokens);

    subsystem->initialize(*handle);
    subsystem->register_callbacks();

    m_subsystems[type] = subsystem;
    m_handles[type] = std::move(handle);
}

std::shared_ptr<AudioSubsystem> SubsystemManager::get_audio_subsystem()
{
    if (auto subsystem = std::dynamic_pointer_cast<AudioSubsystem>(get_subsystem(SubsystemType::AUDIO))) {
        return subsystem;
    }
    return nullptr;
}

std::shared_ptr<GraphicsSubsystem> SubsystemManager::get_graphics_subsystem()
{
    if (auto subsystem = std::dynamic_pointer_cast<GraphicsSubsystem>(get_subsystem(SubsystemType::GRAPHICS))) {
        return subsystem;
    }
    return nullptr;
}

void SubsystemManager::start_all_subsystems()
{
    for (auto& [token, subsystem] : m_subsystems) {
        if (subsystem->is_ready()) {
            subsystem->start();
        }
    }
}

void SubsystemManager::pause_all_subsystems()
{
    for (auto& [type, subsystem] : m_subsystems) {
        if (subsystem->is_running()) {
            subsystem->pause();
        }
    }
}

void SubsystemManager::resume_all_subsystems()
{
    for (auto& [type, subsystem] : m_subsystems) {
        if (subsystem->is_ready()) {
            subsystem->resume();
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

std::unordered_map<SubsystemType, std::pair<bool, bool>> SubsystemManager::query_subsystem_status() const
{
    std::unordered_map<SubsystemType, std::pair<bool, bool>> statuses;
    for (const auto& [type, subsystem] : m_subsystems) {
        statuses[type] = { subsystem->is_ready(), subsystem->is_running() };
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
    m_cross_access_permissions[from].insert(to);
}

std::optional<std::span<const double>> SubsystemManager::read_cross_subsystem_buffer(
    SubsystemType requesting_type,
    SubsystemType target_type,
    uint32_t channel)
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

SubsystemProcessingHandle* SubsystemManager::get_validated_handle(SubsystemType type) const
{
    auto subsystem_it = m_subsystems.find(type);
    if (subsystem_it == m_subsystems.end()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Runtime,
            "Invalid subsystem type: Subsystem not found.");
        return nullptr;
    }

    auto handle = subsystem_it->second->get_processing_context_handle();
    if (!handle) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Runtime,
            "Invalid processing context handle: Handle is null.");
        return nullptr;
    }

    return handle;
}

void SubsystemManager::register_process_hook(SubsystemType type, const std::string& name, ProcessHook hook, HookPosition position)
{
    auto handle = get_validated_handle(type);
    if (!handle)
        return;

    if (!hook) {
        MF_ERROR(Journal::Component::Core, Journal::Context::Runtime,
            "Invalid process hook: Hook cannot be null or invalid.");
        return;
    }

    if (position == HookPosition::PRE_PROCESS) {
        handle->pre_process_hooks[name] = std::move(hook);
    } else {
        handle->post_process_hooks[name] = std::move(hook);
    }
}

void SubsystemManager::unregister_process_hook(SubsystemType type, const std::string& name)
{
    auto handle = get_validated_handle(type);
    if (!handle)
        return;

    if (handle->pre_process_hooks.erase(name) == 0) {
        handle->post_process_hooks.erase(name);
    }
}

bool SubsystemManager::has_process_hook(SubsystemType type, const std::string& name)
{
    auto handle = get_validated_handle(type);
    if (!handle)
        return false;

    return handle->pre_process_hooks.contains(name) || handle->post_process_hooks.contains(name);
}
}
