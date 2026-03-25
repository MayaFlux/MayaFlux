#include "NetworkTextureBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkTextureBuffer::NetworkTextureBuffer(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    std::shared_ptr<Core::VKImage> texture,
    const std::string& binding_name,
    float over_allocate_factor)
    : NetworkGeometryBuffer(std::move(network), binding_name, over_allocate_factor)
{
    m_uv_processor = std::make_shared<UVFieldProcessor>();

    if (texture)
        m_uv_processor->set_texture(std::move(texture));
}

void NetworkTextureBuffer::setup_processors(ProcessingToken token)
{
    NetworkGeometryBuffer::setup_processors(token);

    auto self = std::dynamic_pointer_cast<NetworkTextureBuffer>(shared_from_this());
    get_processing_chain()->add_postprocessor(m_uv_processor, self);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "NetworkTextureBuffer: UV field postprocessor added");
}

void NetworkTextureBuffer::set_texture(
    std::shared_ptr<Core::VKImage> image,
    const Portal::Graphics::SamplerConfig& config)
{
    m_uv_processor->set_texture(std::move(image), config);
}

} // namespace MayaFlux::Buffers
