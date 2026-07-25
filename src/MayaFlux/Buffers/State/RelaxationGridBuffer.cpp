#include "RelaxationGridBuffer.hpp"
#include "RelaxationEmitProcessor.hpp"
#include "RelaxationStepProcessor.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

RelaxationGridBuffer::RelaxationGridBuffer(
    uint32_t width,
    uint32_t height,
    size_t cell_stride_bytes,
    ShaderSource rule_source,
    ShaderSource emit_source)
    : VKBuffer(
          static_cast<size_t>(width) * height * sizeof(Kakshya::Vertex),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_width(width)
    , m_height(height)
    , m_cell_stride_bytes(cell_stride_bytes)
    , m_rule_source(std::move(rule_source))
    , m_emit_source(std::move(emit_source))
{
    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferManagement,
            std::source_location::current(),
            "RelaxationGridBuffer requires a valid BufferService");
    }

    const size_t state_bytes = get_state_bytes();
    const auto usage_flags = static_cast<uint32_t>(
        VkBufferUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer));
    const auto memory_flags = static_cast<uint32_t>(
        VkMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

    auto& resources = get_buffer_resources();
    resources.back_buffers.resize(2);

    for (uint32_t i = 0; i < 2; ++i) {
        void* out_buffer = nullptr;
        void* out_memory = nullptr;
        void* out_mapped = nullptr;

        buffer_service->allocate_raw_buffer(
            state_bytes, usage_flags, memory_flags, true,
            out_buffer, out_memory, out_mapped);

        resources.back_buffers[i].buffer = static_cast<vk::Buffer>(static_cast<VkBuffer>(out_buffer));
        resources.back_buffers[i].memory = static_cast<vk::DeviceMemory>(static_cast<VkDeviceMemory>(out_memory));
        resources.back_buffers[i].mapped_ptr = out_mapped;
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "RelaxationGridBuffer: {}x{} grid, {} bytes/cell, {} bytes/generation",
        m_width, m_height, m_cell_stride_bytes, state_bytes);
}

void RelaxationGridBuffer::setup_processors(ProcessingToken token)
{
    auto layout = Kakshya::VertexLayout::for_raw(sizeof(Kakshya::Vertex));
    layout.vertex_count = get_cell_count();
    set_vertex_layout(layout);

    auto step_processor = std::visit(
        [](const auto& src) { return std::make_shared<RelaxationStepProcessor>(src); },
        m_rule_source);

    step_processor->set_processing_token(token);
    set_default_processor(step_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    auto emit_processor = std::visit(
        [](const auto& src) { return std::make_shared<RelaxationEmitProcessor>(src); },
        m_emit_source);
    emit_processor->set_processing_token(token);
    chain->add_processor(emit_processor, shared_from_this());

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "RelaxationGridBuffer setup_processors: step + emit attached");
}

void RelaxationGridBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;
    resolved.topology = Portal::Graphics::PrimitiveTopology::POINT_LIST;

    if (resolved.vertex_shader.empty())
        resolved.vertex_shader = "point.vert.spv";
    if (resolved.fragment_shader.empty())
        resolved.fragment_shader = "point.frag.spv";

    ShaderConfig sc { resolved.vertex_shader };
    apply_render_config(resolved, sc);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "RelaxationGridBuffer: rendering configured");
}

void RelaxationGridBuffer::seed_state(const void* data, size_t size)
{
    if (size != get_state_bytes()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "seed_state size {} does not match expected {}", size, get_state_bytes());
        return;
    }

    auto& resources = get_buffer_resources();
    auto& front = resources.back_buffers[front_index()];

    if (!front.mapped_ptr) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "seed_state: front generation buffer has no mapped pointer");
        return;
    }

    std::memcpy(front.mapped_ptr, data, size);
}

} // namespace MayaFlux::Buffers
