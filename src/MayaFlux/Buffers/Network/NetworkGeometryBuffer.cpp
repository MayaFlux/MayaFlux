#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {
    Portal::Graphics::RenderConfig resolve_config(const Portal::Graphics::RenderConfig& config)
    {
        VKBuffer::RenderConfig resolved_config = config;

        switch (config.topology) {
        case Portal::Graphics::PrimitiveTopology::POINT_LIST:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "point.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "point.frag.spv";
            break;
        case Portal::Graphics::PrimitiveTopology::LINE_LIST:
        case Portal::Graphics::PrimitiveTopology::LINE_STRIP:

            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "line.vert.spv";

            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "line.frag.spv";

#ifndef MAYAFLUX_PLATFORM_MACOS
            if (config.geometry_shader.empty())
                resolved_config.geometry_shader = "line.geom.spv";
#else
            resolved_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif // !MAYAFLUX_PLATFORM_MACOS
            break;
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "triangle.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "triangle.frag.spv";
            break;
        default:
            if (config.vertex_shader.empty())
                resolved_config.vertex_shader = "point.vert.spv";
            if (config.fragment_shader.empty())
                resolved_config.fragment_shader = "point.frag.spv";
        }

        return resolved_config;
    }

}

NetworkGeometryBuffer::NetworkGeometryBuffer(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    const std::string& binding_name,
    float over_allocate_factor)
    : VKBuffer(
          calculate_buffer_size(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(std::move(network))
    , m_binding_name(binding_name)
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create NetworkGeometryBuffer with null NodeNetwork");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created NetworkGeometryBuffer '{}' for {} nodes ({} bytes estimated)",
        m_binding_name,
        m_network->get_node_count(),
        get_size_bytes());
}

void NetworkGeometryBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<NetworkGeometryBuffer>(shared_from_this());

    m_processor = std::make_shared<NetworkGeometryProcessor>();
    m_processor->set_processing_token(token);
    m_processor->bind_network(
        m_binding_name,
        m_network,
        self);

    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Setup NetworkGeometryProcessor for '{}' with token {}",
        m_binding_name, static_cast<int>(token));

    wire_field_operators();
}

void NetworkGeometryBuffer::setup_rendering(const RenderConfig& config)
{
    auto resolved_config = resolve_config(config);

    apply_render_config(resolved_config, ShaderConfig { resolved_config.vertex_shader });

    get_processing_chain()->add_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
}

uint32_t NetworkGeometryBuffer::get_vertex_count() const
{
    if (!m_network) {
        return 0;
    }

    auto* operator_ptr = m_network->get_operator();
    if (operator_ptr) {
        auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(operator_ptr);
        if (graphics_op) {
            return static_cast<uint32_t>(graphics_op->get_vertex_count());
        }
    }

    return static_cast<uint32_t>(m_network->get_node_count());
}

size_t NetworkGeometryBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    float over_allocate_factor)
{
    if (!network) {
        return 0;
    }

    size_t node_count = network->get_node_count();
    if (node_count == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NodeNetwork has zero nodes. Buffer will be created with minimum size.");
        return 4096;
    }

    size_t base_size = 0;

    if (auto* operator_ptr = network->get_operator()) {
        if (auto graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(operator_ptr)) {
            size_t vertex_count = graphics_op->get_vertex_count();
            auto layout = graphics_op->get_vertex_layout();

            if (vertex_count > 0 && layout.stride_bytes > 0) {
                base_size = vertex_count * layout.stride_bytes;

                MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
                    "Network geometry buffer sizing: {} vertices × {} bytes = {} bytes (operator: {})",
                    vertex_count, layout.stride_bytes, base_size, operator_ptr->get_type_name());
            }
        }
    }

    if (base_size == 0) {
        size_t vertex_size = sizeof(Kakshya::PointVertex);
        base_size = node_count * vertex_size;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Network geometry buffer fallback sizing: {} nodes × {} bytes = {} bytes",
            node_count, vertex_size, base_size);
    }

    auto allocated_size = static_cast<size_t>(
        static_cast<float>(base_size) * over_allocate_factor);

    if (over_allocate_factor > 1.0F) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Over-allocated by {}x: {} → {} bytes",
            over_allocate_factor, base_size, allocated_size);
    }

    return allocated_size;
}

void NetworkGeometryBuffer::add_chain_operator_rendering(const RenderConfig& config)
{
    const auto resolved_config = resolve_config(config);
    auto self = std::dynamic_pointer_cast<VKBuffer>(shared_from_this());

    std::shared_ptr<RenderProcessor> render;
    apply_render_config(render, resolved_config, ShaderConfig { resolved_config.vertex_shader });

    get_processing_chain()->add_processor(render, shared_from_this());
    render->set_buffer_vertex_layout(self, Kakshya::VertexLayout::for_lines());
    render->set_vertex_range(0, 0);

    m_chain_render_processors.push_back({ .render_processor = render });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Added chain render processor #{} to NetworkGeometryBuffer",
        m_chain_render_processors.size());
}

std::shared_ptr<RenderProcessor> NetworkGeometryBuffer::get_chain_render_processor(size_t index) const
{
    if (index >= m_chain_render_processors.size())
        return nullptr;
    return m_chain_render_processors[index].render_processor;
}

void NetworkGeometryBuffer::update_chain_render_range(
    size_t index,
    uint32_t vertex_offset,
    uint32_t vertex_count,
    const std::optional<Kakshya::VertexLayout>& layout)
{
    if (index == 0) {
        if (m_render_processor)
            m_render_processor->set_vertex_range(vertex_offset, vertex_count);
        return;
    }

    const size_t ci = index - 1;
    if (ci >= m_chain_render_processors.size())
        return;

    auto& entry = m_chain_render_processors[ci];
    entry.vertex_offset = vertex_offset;
    entry.vertex_count = vertex_count;
    entry.render_processor->set_vertex_range(vertex_offset, vertex_count);

    if (layout) {
        auto self = std::dynamic_pointer_cast<VKBuffer>(shared_from_this());
        entry.render_processor->set_buffer_vertex_layout(self, *layout);
    }
}

//-----------------------------------------------------------------------------
// Auxiliary state
//-----------------------------------------------------------------------------

bool NetworkGeometryBuffer::declare_state(
    const std::string& name,
    size_t element_count,
    size_t stride_bytes,
    bool double_buffered)
{
    if (name.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "NetworkGeometryBuffer: state field declared with empty name, skipped");
        return false;
    }

    if (element_count == 0 || stride_bytes == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "NetworkGeometryBuffer: state field '{}' declares zero element count "
            "or stride, skipped",
            name);
        return false;
    }

    if (m_state_fields.contains(name)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "NetworkGeometryBuffer: duplicate state field '{}', later declaration "
            "discarded",
            name);
        return false;
    }

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service || !buffer_service->allocate_raw_buffer) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "NetworkGeometryBuffer requires a valid buffer service");
    }

    const auto usage_flags = static_cast<uint32_t>(
        static_cast<VkBufferUsageFlags>(
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst));

    const auto memory_flags = static_cast<uint32_t>(
        static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal));

    auto& resources = get_buffer_resources();
    const size_t field_bytes = element_count * stride_bytes;
    const uint32_t slot_count = double_buffered ? 2 : 1;

    StateField field {
        .element_count = element_count,
        .stride_bytes = stride_bytes,
        .slot_a = static_cast<uint32_t>(resources.back_buffers.size()),
        .slot_b = 0,
        .read_is_a = true,
    };

    for (uint32_t i = 0; i < slot_count; ++i) {
        void* out_buffer = nullptr;
        void* out_memory = nullptr;
        void* out_mapped = nullptr;

        buffer_service->allocate_raw_buffer(
            field_bytes, usage_flags, memory_flags, false,
            out_buffer, out_memory, out_mapped);

        VKBufferResources::GenerationSlot slot;
        slot.buffer = static_cast<vk::Buffer>(static_cast<VkBuffer>(out_buffer));
        slot.memory = static_cast<vk::DeviceMemory>(static_cast<VkDeviceMemory>(out_memory));
        slot.mapped_ptr = out_mapped;

        resources.back_buffers.push_back(slot);
    }

    field.slot_b = double_buffered ? field.slot_a + 1 : field.slot_a;

    m_state_fields.emplace(name, field);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "NetworkGeometryBuffer: state field '{}', {} elements, stride {}, {} slot(s), {} bytes",
        name, element_count, stride_bytes, slot_count, field_bytes * slot_count);

    return true;
}

const NetworkGeometryBuffer::StateField* NetworkGeometryBuffer::find_state_field(
    const std::string& name, const char* context) const
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NetworkGeometryBuffer::{}: no state field named '{}'", context, name);
        return nullptr;
    }
    return &it->second;
}

bool NetworkGeometryBuffer::has_state(const std::string& name) const
{
    return m_state_fields.contains(name);
}

size_t NetworkGeometryBuffer::get_state_bytes(const std::string& name) const
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        return 0;
    }
    return it->second.element_count * it->second.stride_bytes;
}

vk::Buffer NetworkGeometryBuffer::read_state_handle(const std::string& name) const
{
    const auto* field = find_state_field(name, "read_state_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot].buffer;
}

VKBufferResources::GenerationSlot NetworkGeometryBuffer::read_state_slot(const std::string& name) const
{
    const auto* field = find_state_field(name, "read_state_slot");
    if (!field) {
        return {};
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot];
}

vk::Buffer NetworkGeometryBuffer::write_state_handle(const std::string& name) const
{
    const auto* field = find_state_field(name, "write_state_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot].buffer;
}

VKBufferResources::GenerationSlot NetworkGeometryBuffer::write_state_slot(const std::string& name) const
{
    const auto* field = find_state_field(name, "write_state_slot");
    if (!field) {
        return {};
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot];
}

void NetworkGeometryBuffer::swap_state(const std::string& name)
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NetworkGeometryBuffer::swap_state: no state field named '{}'", name);
        return;
    }

    if (it->second.slot_a == it->second.slot_b) {
        return;
    }

    it->second.read_is_a = !it->second.read_is_a;
}

bool NetworkGeometryBuffer::ensure_cluster_ids()
{
    if (has_state("hash_cluster_id")) {
        return true;
    }

    auto* graphics_op = m_network
        ? dynamic_cast<Nodes::Network::GraphicsOperator*>(m_network->get_operator())
        : nullptr;
    if (!graphics_op) {
        return false;
    }

    const size_t vertex_count = graphics_op->get_vertex_count();
    if (vertex_count == 0) {
        return false;
    }

    if (!declare_state("hash_cluster_id", vertex_count, sizeof(uint32_t), false)) {
        return false;
    }

    auto cluster_ids = graphics_op->build_cluster_ids();
    if (cluster_ids.size() != vertex_count) {
        cluster_ids.assign(vertex_count, 0U);
    }

    std::shared_ptr<VKBuffer> staging;
    upload_back_buffer(
        write_state_slot("hash_cluster_id"),
        cluster_ids.data(),
        cluster_ids.size() * sizeof(uint32_t),
        staging);

    return true;
}

} // namespace MayaFlux::Buffers
