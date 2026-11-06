#include "DispatchRecording.hpp"

#include "GpuResourceManager.hpp"

namespace MayaFlux::Yantra {

DispatchRecording::~DispatchRecording()
{
    if (!Portal::Graphics::get_shader_foundry().is_initialized())
        return;

    wait();
}

Portal::Graphics::CommandBufferID DispatchRecording::command()
{
    if (!is_open())
        m_cmd = GpuResourceManager::begin_dispatch_commands();
    return m_cmd;
}

void DispatchRecording::release_after(std::function<void()> release)
{
    if (!release)
        return;

    if (is_open()) {
        m_releases.push_back(std::move(release));
        return;
    }

    Portal::Graphics::get_shader_foundry().release_after_submit(std::move(release));
}

Portal::Graphics::FenceID DispatchRecording::submit()
{
    if (!is_open())
        return m_last;

    auto& foundry = Portal::Graphics::get_shader_foundry();

    record_closing_barrier(m_cmd);
    const auto fence = foundry.submit_async(m_cmd);
    m_cmd = Portal::Graphics::INVALID_COMMAND_BUFFER;

    for (auto& release : m_releases)
        foundry.release_after_submit(std::move(release));
    m_releases.clear();

    if (fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "DispatchRecording: failed to submit recorded dispatches");
        return m_last;
    }

    foundry.release_fence(m_last);
    m_last = fence;
    return m_last;
}

bool DispatchRecording::is_complete(Portal::Graphics::FenceID fence)
{
    if (fence == Portal::Graphics::INVALID_FENCE || m_last == Portal::Graphics::INVALID_FENCE)
        return true;
    return Portal::Graphics::get_shader_foundry().is_fence_signaled(m_last);
}

void DispatchRecording::wait()
{
    submit();

    if (m_last == Portal::Graphics::INVALID_FENCE)
        return;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    foundry.wait_for_fence(m_last);
    foundry.release_fence(m_last);
    m_last = Portal::Graphics::INVALID_FENCE;
}

void DispatchRecording::record_closing_barrier(Portal::Graphics::CommandBufferID cmd_id)
{
    Portal::Graphics::get_shader_foundry().memory_barrier(
        cmd_id,
        vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eTransferWrite,
        vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eHostRead,
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eTransfer,
        vk::PipelineStageFlagBits2::eAllCommands | vk::PipelineStageFlagBits2::eHost);
}

} // namespace MayaFlux::Yantra
