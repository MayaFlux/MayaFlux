#include "TextureBindBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TextureBindBuffer::TextureBindBuffer(
    std::shared_ptr<Nodes::GpuSync::TextureNode> node, std::string binding_name)
    : VKBuffer(
          calculate_buffer_size(node),
          Usage::STAGING,
          Kakshya::DataModality::IMAGE_COLOR)
    , m_texture_node(std::move(node))
    , m_binding_name(std::move(binding_name))
{
    if (!m_texture_node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create TextureBindBuffer with null TextureNode");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created TextureBindBuffer '{}' for {}x{} texture ({} bytes)",
        m_binding_name,
        m_texture_node->get_width(),
        m_texture_node->get_height(),
        get_size_bytes());
}

void TextureBindBuffer::setup_processors(Buffers::ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<TextureBindBuffer>(shared_from_this());

    m_bindings_processor = std::make_shared<TextureBindingsProcessor>();
    m_bindings_processor->bind_texture_node(
        m_binding_name,
        m_texture_node,
        self);

    set_default_processor(m_bindings_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);
}

size_t TextureBindBuffer::calculate_buffer_size(const std::shared_ptr<Nodes::GpuSync::TextureNode>& node)
{
    if (!node) {
        return 0;
    }

    size_t size = static_cast<size_t>(node->get_width()) * static_cast<size_t>(node->get_height()) * 4 * sizeof(float);

    if (size == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "TextureNode has zero dimensions. Using minimum buffer size.");
        return 4096;
    }

    return size;
}

} // namespace MayaFlux::Buffers
