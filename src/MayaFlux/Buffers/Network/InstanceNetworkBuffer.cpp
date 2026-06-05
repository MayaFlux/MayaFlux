#include "InstanceNetworkBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// =============================================================================
// Construction
// =============================================================================

InstanceNetworkBuffer::InstanceNetworkBuffer(
    std::shared_ptr<Nodes::Network::InstanceNetwork> network,
    float over_allocate_factor)
    : VKBuffer(
          estimate_vertex_bytes(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(std::move(network))
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "InstanceNetworkBuffer: null InstanceNetwork");
    }

    const auto& slots = m_network->slots();
    const Portal::Graphics::PrimitiveTopology topo
        = (!slots.empty() && slots[0].node)
        ? slots[0].node->get_primitive_topology()
        : Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;

    RenderConfig defaults;
    defaults.topology = topo;
    set_default_render_config(defaults);
    m_render_config = defaults;

    set_needs_depth_attachment(true);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "InstanceNetworkBuffer: {} slots, {} bytes estimated",
        m_network->slot_count(), get_size_bytes());
}

// =============================================================================
// setup_processors
// =============================================================================

void InstanceNetworkBuffer::setup_processors(ProcessingToken token)
{
    m_ssbo_processor = std::make_shared<InstanceSSBOProcessor>(m_network);
    m_ssbo_processor->set_processing_token(token);
    set_default_processor(m_ssbo_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "InstanceNetworkBuffer::setup_processors with token {}",
        static_cast<int>(token));
}

// =============================================================================
// setup_rendering
// =============================================================================

void InstanceNetworkBuffer::setup_rendering(const RenderConfig& config)
{
    if (!config.vertex_shader.empty())
        m_render_config.vertex_shader = config.vertex_shader;
    if (!config.fragment_shader.empty())
        m_render_config.fragment_shader = config.fragment_shader;

    m_render_config.target_window = config.target_window;
    m_render_config.polygon_mode = config.polygon_mode;
    m_render_config.cull_mode = config.cull_mode;

    for (const auto& [name, tex] : config.additional_textures)
        m_render_config.additional_textures.emplace_back(name, tex);

    if (m_render_config.vertex_shader.empty())
        m_render_config.vertex_shader = "instance.vert.spv";
    if (m_render_config.fragment_shader.empty())
        m_render_config.fragment_shader = "triangle.frag.spv";

    ShaderConfig sc { m_render_config.vertex_shader };
    sc.bindings["instanceTransforms"] = ShaderBinding(
        0, InstanceSSBOProcessor::k_transform_ssbo_binding,
        vk::DescriptorType::eStorageBuffer);

    apply_render_config(m_render_config, sc);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "InstanceNetworkBuffer::setup_rendering: vert={} frag={}",
        m_render_config.vertex_shader, m_render_config.fragment_shader);
}

// =============================================================================
// Private
// =============================================================================

size_t InstanceNetworkBuffer::estimate_vertex_bytes(
    const std::shared_ptr<Nodes::Network::InstanceNetwork>& network,
    float factor)
{
    constexpr size_t k_min = 64L * 1024;
    if (!network || network->slot_count() == 0)
        return k_min;

    const auto& slots = network->slots();
    if (!slots[0].node)
        return k_min;

    const size_t capacity = slots[0].node->get_vertex_buffer_size_bytes();
    if (capacity == 0)
        return k_min;

    return static_cast<size_t>(static_cast<float>(capacity) * factor);
}

} // namespace MayaFlux::Buffers
