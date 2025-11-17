#include "TextureBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TextureBuffer::TextureBuffer(
    std::shared_ptr<Nodes::TextureNode> node, std::string binding_name)
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
            "Cannot create TextureBuffer with null TextureNode");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created TextureBuffer '{}' for {}x{} texture ({} bytes)",
        m_binding_name,
        m_texture_node->get_width(),
        m_texture_node->get_height(),
        get_size_bytes());
}

void TextureBuffer::initialize()
{
    auto self = std::dynamic_pointer_cast<TextureBuffer>(shared_from_this());

    m_bindings_processor = std::make_shared<TextureBindingsProcessor>();
    m_bindings_processor->bind_texture_node(
        m_binding_name,
        m_texture_node,
        self);

    set_default_processor(m_bindings_processor);
}

size_t TextureBuffer::calculate_buffer_size(const std::shared_ptr<Nodes::TextureNode>& node)
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
