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

namespace {

    /**
     * @brief Format a stage declares, or the one its ShaderSpec binding carries.
     * @param stage Stage to resolve.
     * @param binding Binding whose format a spec stage is read from.
     * @return The format, or nullopt when a path stage declares none.
     */
    std::optional<Kakshya::GpuDataFormat> resolve_state(
        const RelaxationGridBuffer::GridConfig::Stage& stage,
        std::string_view binding)
    {
        if (stage.state) {
            return stage.state;
        }

        const auto* spec = std::get_if<Portal::Graphics::ShaderSpec>(&stage.shader);
        if (!spec) {
            return std::nullopt;
        }

        const auto it = std::ranges::find_if(
            spec->bindings,
            [binding](const auto& slot) { return slot.name == binding; });

        if (it == spec->bindings.end()) {
            return std::nullopt;
        }

        return it->format;
    }

    /**
     * @brief Cell stride in bytes for a GridConfig, after checking its stages.
     * @param config Grid arrangement.
     * @return gpu_data_format_bytes of the shared cell format.
     */
    size_t resolve_stride(const RelaxationGridBuffer::GridConfig& config)
    {
        const auto rule = resolve_state(config.rule, "state_in");
        const auto emit = resolve_state(config.emit, "cell_state");

        if (!rule || !emit) {
            error<std::invalid_argument>(
                Journal::Component::Buffers, Journal::Context::Init,
                std::source_location::current(),
                "GridConfig: a stage whose shader is a path must declare its state format");
        }

        if (*rule != *emit) {
            error<std::invalid_argument>(
                Journal::Component::Buffers, Journal::Context::Init,
                std::source_location::current(),
                "GridConfig: rule and emit disagree on the cell format");
        }

        if (*rule == Kakshya::GpuDataFormat::VEC3_F32 || *rule == Kakshya::GpuDataFormat::VEC3_F64) {
            error<std::invalid_argument>(
                Journal::Component::Buffers, Journal::Context::Init,
                std::source_location::current(),
                "GridConfig: a three component cell strides 16 bytes in std430, declare a four component format");
        }

        return Kakshya::gpu_data_format_bytes(*rule);
    }

    /**
     * @brief Write constants after a stage's fixed prefix in its push constant block.
     * @param processor Stage processor, attached and not yet dispatched.
     * @param offset Byte offset of the first constant.
     * @param constants Words to write, in shader declaration order.
     *
     * Grows the block through set_push_constant_size so the pipeline range is
     * created at the final width on the first cycle.
     */
    template <typename Processor>
    void apply_constants(
        const std::shared_ptr<Processor>& processor,
        size_t offset,
        const std::vector<RelaxationGridBuffer::GridConfig::Constant>& constants)
    {
        if (constants.empty()) {
            return;
        }

        const size_t end = offset + constants.size() * sizeof(uint32_t);
        if (processor->get_push_constant_data().size() < end) {
            processor->set_push_constant_size(end);
        }

        auto& data = processor->get_push_constant_data();
        if (data.size() < end) {
            data.resize(end);
        }

        size_t at = offset;
        for (const auto& constant : constants) {
            std::visit([&](auto value) { std::memcpy(data.data() + at, &value, sizeof(value)); }, constant);
            at += sizeof(uint32_t);
        }
    }

} // namespace

RelaxationGridBuffer::RelaxationGridBuffer(const GridConfig& config)
    : RelaxationGridBuffer(
          config.width, config.height, resolve_stride(config),
          config.rule.shader, config.emit.shader)
{
    m_rule_constants = config.rule.constants;
    m_emit_constants = config.emit.constants;
    m_extent = config.extent;
    m_point_size = config.point_size;
}

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

    m_step_processor = std::visit(
        [](const auto& src) { return std::make_shared<RelaxationStepProcessor>(src); },
        m_rule_source);

    m_step_processor->set_processing_token(token);
    set_default_processor(m_step_processor);
    apply_constants(m_step_processor, sizeof(RelaxationStepProcessor::GridExtent), m_rule_constants);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    m_emit_processor = std::visit(
        [](const auto& src) { return std::make_shared<RelaxationEmitProcessor>(src); },
        m_emit_source);
    m_emit_processor->set_processing_token(token);
    chain->add_processor(m_emit_processor, shared_from_this());
    apply_constants(m_emit_processor, sizeof(RelaxationEmitProcessor::EmitParams), m_emit_constants);
    m_emit_processor->set_extent(m_extent);
    m_emit_processor->set_point_size(m_point_size);

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
