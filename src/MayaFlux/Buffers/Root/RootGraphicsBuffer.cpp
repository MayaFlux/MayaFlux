#include "RootGraphicsBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

GraphicsBatchProcessor::GraphicsBatchProcessor(std::shared_ptr<Buffer> root_buffer)
    : m_root_buffer(std::dynamic_pointer_cast<RootGraphicsBuffer>(std::move(root_buffer)))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void GraphicsBatchProcessor::processing_function(std::shared_ptr<Buffer> /* buffer */)
{
    /* auto root_graphics_buffer = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_graphics_buffer || root_graphics_buffer != m_root_buffer) {
        return;
    }
    root_graphics_buffer->process_children(1); */
}

void GraphicsBatchProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    auto root_graphics_buffer = std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer);
    if (!root_graphics_buffer) {
        throw std::runtime_error("GraphicsBatchProcessor can only be attached to RootGraphicsBuffer");
    }

    if (!are_tokens_compatible(ProcessingToken::GRAPHICS_BACKEND, m_processing_token)) {
        throw std::runtime_error("GraphicsBatchProcessor token incompatible with RootGraphicsBuffer requirements");
    }
}

bool GraphicsBatchProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<RootGraphicsBuffer>(buffer) != nullptr;
}

RootGraphicsBuffer::RootGraphicsBuffer()
    : m_final_processor(nullptr)
{
    m_preferred_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    m_token_enforcement_strategy = TokenEnforcementStrategy::STRICT;
}

RootGraphicsBuffer::~RootGraphicsBuffer()
{
    cleanup_marked_buffers();
    m_child_buffers.clear();
    m_pending_removal.clear();
}

void RootGraphicsBuffer::initialize()
{
    auto batch_processor = create_default_processor();
    if (batch_processor) {
        set_default_processor(batch_processor);
    }
}

void RootGraphicsBuffer::process_children(uint32_t processing_units)
{
    cleanup_marked_buffers();

    for (auto& buffer : m_child_buffers) {
        if (!buffer)
            continue;

        if (buffer->needs_removal()) {
            continue;
        }

        if (!buffer->has_data_for_cycle()) {
            continue;
        }

        process_buffer(buffer, processing_units);

        if (auto processing_chain = buffer->get_processing_chain()) {
            processing_chain->process(buffer);
        }
    }
}

void RootGraphicsBuffer::process_default()
{
    if (this->has_pending_operations()) {
        this->process_pending_buffer_operations();
    }

    process_children(1);

    if (m_final_processor) {
        m_final_processor->process(shared_from_this());
    }
}

void RootGraphicsBuffer::process_buffer(const std::shared_ptr<VKBuffer>& buffer, uint32_t /*processing_units*/)
{
    try {
        if (buffer->needs_default_processing() && buffer->get_default_processor()) {
            buffer->process_default();
        }

        if (auto chain = buffer->get_processing_chain()) {
            chain->process(buffer);
        }

    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::BufferProcessing,
            "Error processing graphics buffer: {}", e.what());
    }
}

void RootGraphicsBuffer::cleanup_marked_buffers()
{
    if (m_pending_removal.empty()) {
        return;
    }

    auto it = std::remove_if(
        m_child_buffers.begin(),
        m_child_buffers.end(),
        [](const std::shared_ptr<Buffer>& buf) {
            return buf && buf->needs_removal();
        });

    size_t removed_count = std::distance(it, m_child_buffers.end());

    if (removed_count > 0) {
        m_child_buffers.erase(it, m_child_buffers.end());

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Cleaned up {} graphics buffers (remaining: {})",
            removed_count, m_child_buffers.size());
    }

    m_pending_removal.clear();
}

void RootGraphicsBuffer::set_final_processor(std::shared_ptr<BufferProcessor> processor)
{
    m_final_processor = std::move(processor);
}

std::shared_ptr<BufferProcessor> RootGraphicsBuffer::get_final_processor() const
{
    return m_final_processor;
}

bool RootGraphicsBuffer::has_buffer(const std::shared_ptr<VKBuffer>& buffer) const
{
    return std::ranges::find(m_child_buffers, buffer) != m_child_buffers.end();
}

std::vector<std::shared_ptr<VKBuffer>> RootGraphicsBuffer::get_buffers_by_usage(VKBuffer::Usage usage) const
{
    std::vector<std::shared_ptr<VKBuffer>> filtered_buffers;

    for (const auto& buffer : m_child_buffers) {
        if (buffer && buffer->get_usage() == usage) {
            filtered_buffers.push_back(buffer);
        }
    }

    return filtered_buffers;
}

std::shared_ptr<BufferProcessor> RootGraphicsBuffer::create_default_processor()
{
    return std::make_shared<GraphicsBatchProcessor>(shared_from_this());
}

} // namespace MayaFlux::Buffers
