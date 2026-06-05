#include "GpuComputeNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Nodes::GpuSync {

GpuComputeNode::GpuComputeNode(
    std::shared_ptr<Yantra::ShaderExecutionContext<>> executor,
    bool continuous)
    : m_executor(std::move(executor))
    , m_continuous(continuous)
{
    if (!m_executor) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "GpuComputeNode: null ShaderExecutionContext");
    }
}

// =============================================================================
// GpuSync interface
// =============================================================================

void GpuComputeNode::compute_frame()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (m_pending_fence != Portal::Graphics::INVALID_FENCE) {
        if (!foundry.is_fence_signaled(m_pending_fence))
            return;

        const auto result = m_executor->collect_result();

        m_pending_fence = Portal::Graphics::INVALID_FENCE;
        m_ready = true;

        m_last_result = result;
        notify_tick(0.0);

        if (m_continuous)
            m_dirty = true;

        return;
    }

    if (!m_dirty)
        return;

    m_dirty = false;
    m_ready = false;

    Yantra::Datum<std::vector<Kakshya::DataVariant>> input;
    m_pending_fence = m_executor->dispatch_async(input);

    if (m_pending_fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuComputeNode: dispatch_async returned INVALID_FENCE");
        m_dirty = true;
    }
}

void GpuComputeNode::on_complete(const TypedHook<GpuComputeContext>& callback)
{
    safe_add_callback(m_complete_callbacks, callback);
}

bool GpuComputeNode::remove_complete_hook(const TypedHook<GpuComputeContext>& callback)
{
    return safe_remove_callback(m_complete_callbacks, callback);
}

void GpuComputeNode::notify_tick(double /*value*/)
{
    update_context(0.0);

    for (auto& cb : m_complete_callbacks)
        cb(*m_context_storage);

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GpuComputeNode: dispatch complete, {} primary floats, {} aux bindings",
        m_last_result.primary.size(), m_last_result.aux.size());
}

void GpuComputeNode::update_context(double /*value*/)
{
    m_context_storage = std::make_unique<GpuComputeContext>(m_last_result);

    if (!m_context_storage) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuComputeNode: failed to create GpuComputeContext");
    }
}

} // namespace MayaFlux::Nodes::GpuSync
