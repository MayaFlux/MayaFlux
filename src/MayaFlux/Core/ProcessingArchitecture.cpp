#include "ProcessingArchitecture.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Core {

BufferProcessingHandle::BufferProcessingHandle(
    std::shared_ptr<Buffers::BufferManager> manager,
    Buffers::ProcessingToken token)
    : m_manager(manager)
    , m_token(token)
    , m_locked(false)
{
}

void BufferProcessingHandle::ensure_valid() const
{
    if (!m_manager) {
        throw std::runtime_error("Invalid buffer processing handle");
    }
}

void BufferProcessingHandle::acquire_write_lock()
{
    if (!m_locked) {
        m_locked = true;
    }
}

void BufferProcessingHandle::process(u_int32_t processing_units)
{
    ensure_valid();
    m_manager->process_token(m_token, processing_units);
}

void BufferProcessingHandle::process_channel(u_int32_t channel, u_int32_t processing_units)
{
    ensure_valid();
    m_manager->process_channel(m_token, channel, processing_units);
}

void BufferProcessingHandle::process_channel_with_node_data(
    u_int32_t channel,
    u_int32_t processing_units,
    const std::vector<double>& node_data)
{
    ensure_valid();
    m_manager->process_channel(m_token, channel, processing_units, node_data);
}

void BufferProcessingHandle::process_input(double* input_data, u_int32_t num_channels, u_int32_t num_frames)
{
    m_manager->process_input(input_data, num_channels, num_frames);
}

std::span<const double> BufferProcessingHandle::read_channel_data(u_int32_t channel) const
{
    ensure_valid();
    return m_manager->get_buffer_data(m_token, channel);
}

std::span<double> BufferProcessingHandle::write_channel_data(u_int32_t channel)
{
    ensure_valid();
    acquire_write_lock();
    return m_manager->get_buffer_data(m_token, channel);
}

void BufferProcessingHandle::setup_channels(u_int32_t num_channels, u_int32_t buffer_size)
{
    ensure_valid();
    m_manager->validate_num_channels(m_token, num_channels, buffer_size);
}

NodeProcessingHandle::NodeProcessingHandle(
    std::shared_ptr<Nodes::NodeGraphManager> manager,
    Nodes::ProcessingToken token)
    : m_manager(manager)
    , m_token(token)
{
}

void NodeProcessingHandle::process(u_int32_t num_samples)
{
    m_manager->process_token(m_token, num_samples);
}

std::vector<double> NodeProcessingHandle::process_channel(u_int32_t channel, u_int32_t num_samples)
{
    return m_manager->process_channel(m_token, channel, num_samples);
}

double NodeProcessingHandle::process_sample(u_int32_t channel)
{
    return m_manager->process_sample(m_token, channel);
}

TaskSchedulerHandle::TaskSchedulerHandle(
    std::shared_ptr<Vruta::TaskScheduler> scheduler,
    Vruta::ProcessingToken token)
    : m_scheduler(scheduler)
    , m_token(token)
{
    if (!m_scheduler) {
        throw std::runtime_error("TaskProcessingHandle requires valid TaskScheduler");
    }
}

void TaskSchedulerHandle::process(u_int64_t processing_units)
{
    m_scheduler->process_token(m_token, processing_units);
}

SubsystemProcessingHandle::SubsystemProcessingHandle(
    std::shared_ptr<Buffers::BufferManager> buffer_manager,
    std::shared_ptr<Nodes::NodeGraphManager> node_manager,
    std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
    SubsystemTokens tokens)
    : buffers(buffer_manager, tokens.Buffer)
    , nodes(node_manager, tokens.Node)
    , tasks(task_scheduler, tokens.Task)
    , m_tokens(tokens)
{
}

}
