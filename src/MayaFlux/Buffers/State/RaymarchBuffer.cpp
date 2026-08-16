#include "RaymarchBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Buffers {

RaymarchBuffer::RaymarchBuffer(
    Kinesis::Lattice3D lattice,
    RaymarchProcessor::FieldSource source,
    size_t field_bytes)
    : MeshBuffer(Kinesis::generate_box(
          (lattice.bounds.min + lattice.bounds.max) * 0.5F,
          (lattice.bounds.max - lattice.bounds.min) * 0.5F))
    , m_lattice(lattice)
    , m_source(std::move(source))
    , m_field_bytes(field_bytes)
{
}

void RaymarchBuffer::setup_processors(ProcessingToken token)
{
    MeshBuffer::setup_processors(token);

    m_march_processor = std::make_shared<RaymarchProcessor>(
        m_source, m_field_bytes, m_lattice);
    m_march_processor->set_processing_token(token);

    get_processing_chain()->add_processor(m_march_processor, shared_from_this());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "RaymarchBuffer: proxy box over {}x{}x{} lattice",
        m_lattice.resolution.x, m_lattice.resolution.y, m_lattice.resolution.z);
}

void RaymarchBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;

    if (resolved.vertex_shader.empty())
        resolved.vertex_shader = "volume_raymarch.vert.spv";
    if (resolved.fragment_shader.empty())
        resolved.fragment_shader = "volume_raymarch.frag.spv";

    MeshBuffer::setup_rendering(resolved);

    m_render_processor->set_cull_mode(Portal::Graphics::CullMode::FRONT);
    m_render_processor->enable_alpha_blending();
    m_render_processor->disable_depth_test();
}

} // namespace MayaFlux::Buffers
