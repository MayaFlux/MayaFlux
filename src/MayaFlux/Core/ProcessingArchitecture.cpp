#include "ProcessingArchitecture.hpp"

#include "MayaFlux/Core/Windowing/WindowManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Core {

BufferProcessingHandle::BufferProcessingHandle(
    std::shared_ptr<Buffers::BufferManager> manager,
    Buffers::ProcessingToken token)
    : m_manager(std::move(manager))
    , m_token(token)
    , m_locked(false)
{
}

void BufferProcessingHandle::ensure_valid() const
{
    if (!m_manager) {
        fatal(Journal::Component::Core, Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Invalid buffer processing handle: BufferManager is null");
    }
}

void BufferProcessingHandle::acquire_write_lock()
{
    if (!m_locked) {
        m_locked = true;
    }
}

void BufferProcessingHandle::process(uint32_t processing_units)
{
    ensure_valid();
    m_manager->process_token(m_token, processing_units);
}

void BufferProcessingHandle::process_channel(uint32_t channel, uint32_t processing_units)
{
    ensure_valid();
    m_manager->process_channel(m_token, channel, processing_units);
}

void BufferProcessingHandle::process_channel_with_node_data(
    uint32_t channel,
    uint32_t processing_units,
    const std::vector<double>& node_data)
{
    ensure_valid();
    m_manager->process_channel(m_token, channel, processing_units, node_data);
}

void BufferProcessingHandle::process_input(double* input_data, uint32_t num_channels, uint32_t num_frames)
{
    m_manager->process_input(input_data, num_channels, num_frames);
}

std::span<const double> BufferProcessingHandle::read_channel_data(uint32_t channel) const
{
    ensure_valid();
    return m_manager->get_buffer_data(m_token, channel);
}

std::span<double> BufferProcessingHandle::write_channel_data(uint32_t channel)
{
    ensure_valid();
    acquire_write_lock();
    return m_manager->get_buffer_data(m_token, channel);
}

void BufferProcessingHandle::setup_channels(uint32_t num_channels, uint32_t buffer_size)
{
    ensure_valid();
    m_manager->validate_num_channels(m_token, num_channels, buffer_size);
}

NodeProcessingHandle::NodeProcessingHandle(
    std::shared_ptr<Nodes::NodeGraphManager> manager,
    Nodes::ProcessingToken token)
    : m_manager(std::move(manager))
    , m_token(token)
{
}

void NodeProcessingHandle::process(uint32_t num_samples)
{
    m_manager->process_token(m_token, num_samples);
}

std::vector<double> NodeProcessingHandle::process_channel(uint32_t channel, uint32_t num_samples)
{
    return m_manager->process_channel(m_token, channel, num_samples);
}

double NodeProcessingHandle::process_sample(uint32_t channel)
{
    return m_manager->process_sample(m_token, channel);
}

std::vector<std::vector<double>> NodeProcessingHandle::process_audio_networks(uint32_t num_samples, uint32_t channel)
{
    return m_manager->process_audio_networks(m_token, num_samples, channel);
}

TaskSchedulerHandle::TaskSchedulerHandle(
    std::shared_ptr<Vruta::TaskScheduler> task_manager,
    Vruta::ProcessingToken token)
    : m_scheduler(std::move(task_manager))
    , m_token(token)
{
    if (!m_scheduler) {
        fatal(Journal::Component::Core, Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "TaskSchedulerHandle requires valid TaskScheduler");
    }
}

void TaskSchedulerHandle::register_token_processor(Vruta::token_processing_func_t processor)
{
    m_scheduler->register_token_processor(m_token, std::move(processor));
}

void TaskSchedulerHandle::process(uint64_t processing_units)
{
    m_scheduler->process_token(m_token, processing_units);
}

void TaskSchedulerHandle::process_buffer_cycle()
{
    m_scheduler->process_buffer_cycle_tasks();
}

WindowManagerHandle::WindowManagerHandle(std::shared_ptr<Core::WindowManager> window_manager)
    : m_window_manager(std::move(window_manager))
{
}

void WindowManagerHandle::process()
{
    if (m_window_manager) {
        m_window_manager->process();
    }
}

std::vector<std::shared_ptr<Core::Window>> WindowManagerHandle::get_processing_windows() const
{
    if (m_window_manager) {
        return m_window_manager->get_processing_windows();
    }
    return {};
}

SubsystemProcessingHandle::SubsystemProcessingHandle(
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Nodes::NodeGraphManager> node_manager,
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
    SubsystemTokens tokens)
    : buffers(std::move(buffer_manager), tokens.Buffer)
    , nodes(std::move(node_manager), tokens.Node)
    , tasks(std::move(task_scheduler), tokens.Task)
    , windows(nullptr)
    , m_tokens(tokens)
{
}

SubsystemProcessingHandle::SubsystemProcessingHandle(
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Nodes::NodeGraphManager> node_manager,
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
    std::shared_ptr<Core::WindowManager> window_manager,
    SubsystemTokens tokens)
    : buffers(std::move(buffer_manager), tokens.Buffer)
    , nodes(std::move(node_manager), tokens.Node)
    , tasks(std::move(task_scheduler), tokens.Task)
    , windows(std::move(window_manager))
    , m_tokens(tokens)
{
}

}
