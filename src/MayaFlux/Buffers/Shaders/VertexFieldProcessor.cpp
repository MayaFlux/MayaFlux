#include "VertexFieldProcessor.hpp"

#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Buffers {

namespace {

    /**
     * @brief Extract a spec from the operator or fail construction.
     *
     * The shader is compiled by ShaderConfig's ShaderSpec constructor, which
     * runs in the member initialiser list, so an operator that yields nothing
     * cannot produce a usable processor. Failing here beats constructing one
     * that logs on every cycle.
     */
    [[nodiscard]] Portal::Graphics::ShaderSpec require_spec(
        const std::shared_ptr<Nodes::Network::GpuFieldOperator>& op)
    {
        if (!op) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "VertexFieldProcessor: null field operator");
        }

        auto spec = op->build_spec();
        if (!spec.has_value()) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "VertexFieldProcessor: operator produced no spec. Bind at least "
                "one field, and supply a layout carrying a position attribute, "
                "before constructing the processor.");
        }

        return *spec;
    }

} // namespace

VertexFieldProcessor::VertexFieldProcessor(
    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_operator)
    : ComputeProcessor(require_spec(field_operator))
    , m_operator(std::move(field_operator))
    , m_built_revision(m_operator->revision())
    , m_needs_cluster_id(m_operator->needs_cluster_id())
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;

    add_binding("vertices",
        ShaderBinding(0, m_operator->get_vertex_binding(),
            Portal::Graphics::DescriptorRole::STORAGE));

    if (m_needs_cluster_id) {
        add_binding("cluster_id",
            ShaderBinding(0, m_operator->get_vertex_binding() + 1,
                Portal::Graphics::DescriptorRole::STORAGE));
    }

    m_params.stride_words = m_operator->get_layout().stride_bytes / 4;

    set_push_constant_data(m_params);
}

void VertexFieldProcessor::set_vertex_range(uint32_t first_vertex, uint32_t vertex_count)
{
    m_explicit_first = first_vertex;
    m_explicit_count = vertex_count;
    m_range_set = true;
}

void VertexFieldProcessor::clear_vertex_range()
{
    m_explicit_first = 0;
    m_explicit_count = 0;
    m_range_set = false;
}

void VertexFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VertexFieldProcessor requires a VKBuffer");
        return;
    }

    bind_buffer("vertices", vk_buf);

    m_network_buffer = std::dynamic_pointer_cast<NetworkGeometryBuffer>(buffer);
    ensure_cluster_binding();

    ComputeProcessor::on_attach(buffer);

    m_epoch = std::chrono::steady_clock::now();

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "VertexFieldProcessor attached: {} bytes, stride {} words, {} bound fields",
        vk_buf->get_size_bytes(),
        m_params.stride_words,
        m_operator->binding_count());
}

float VertexFieldProcessor::elapsed() const
{
    return std::chrono::duration<float>(
        std::chrono::steady_clock::now() - m_epoch)
        .count();
}

uint32_t VertexFieldProcessor::buffer_capacity(
    const std::shared_ptr<VKBuffer>& buffer) const
{
    const uint32_t stride_bytes = m_params.stride_words * 4;
    if (stride_bytes == 0)
        return 0;

    return static_cast<uint32_t>(buffer->get_size_bytes() / stride_bytes);
}

bool VertexFieldProcessor::sync_revision()
{
    if (m_operator->revision() == m_built_revision)
        return true;

    auto spec = m_operator->build_spec();
    if (!spec.has_value()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VertexFieldProcessor: operator no longer yields a spec after rebind. "
            "Dispatch suppressed; the previous pipeline is not reused because it "
            "no longer describes the bindings.");
        m_built_revision = m_operator->revision();
        return false;
    }

    set_config(ShaderConfig(*spec));

    add_binding("vertices",
        ShaderBinding(0, m_operator->get_vertex_binding(),
            Portal::Graphics::DescriptorRole::STORAGE));

    m_needs_cluster_id = m_operator->needs_cluster_id();
    if (m_needs_cluster_id) {
        add_binding("cluster_id",
            ShaderBinding(0, m_operator->get_vertex_binding() + 1,
                Portal::Graphics::DescriptorRole::STORAGE));
    }

    m_params.stride_words = m_operator->get_layout().stride_bytes / 4;
    m_built_revision = m_operator->revision();

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "VertexFieldProcessor: rebuilt for revision {} ({} bound fields)",
        m_built_revision, m_operator->binding_count());

    return ensure_cluster_binding();
}

bool VertexFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer)
        return false;

    if (!sync_revision())
        return false;

    const uint32_t capacity = buffer_capacity(buffer);
    if (capacity == 0)
        return false;

    if (m_range_set) {
        if (m_explicit_first >= capacity) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "VertexFieldProcessor: range starts at {} but the buffer holds {} "
                "records. Dispatch suppressed.",
                m_explicit_first, capacity);
            return false;
        }

        m_params.first_vertex = m_explicit_first;
        m_params.vertex_count = std::min(m_explicit_count, capacity - m_explicit_first);
    } else {
        m_params.first_vertex = 0;
        m_params.vertex_count = capacity;
    }

    if (m_params.vertex_count == 0)
        return false;

    m_params.time = elapsed();

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "VertexFieldProcessor dispatch: [{}, {}) of {} records, stride {} words, t={:.3f}",
        m_params.first_vertex,
        m_params.first_vertex + m_params.vertex_count,
        capacity,
        m_params.stride_words,
        m_params.time);

    set_push_constant_data(m_params);
    return true;
}

std::array<uint32_t, 3> VertexFieldProcessor::calculate_dispatch_size(
    const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    const auto& dispatch = get_dispatch_config();
    const uint32_t local_x = std::max(1U, dispatch.workgroup_x);
    const uint32_t groups = (m_params.vertex_count + local_x - 1) / local_x;

    return { std::max(1U, groups), 1U, 1U };
}

bool VertexFieldProcessor::ensure_cluster_binding()
{
    if (!m_needs_cluster_id) {
        return true;
    }

    if (!m_network_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VertexFieldProcessor: operator has a cluster-scoped binding but the "
            "attached buffer is not a NetworkGeometryBuffer, so hash_cluster_id "
            "has nowhere to live. Dispatch suppressed.");
        return false;
    }

    if (!m_network_buffer->ensure_cluster_ids()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VertexFieldProcessor: operator has a cluster-scoped binding but "
            "hash_cluster_id could not be derived (no primary GraphicsOperator, "
            "or it reports zero points). Dispatch suppressed.");
        return false;
    }

    return true;
}

void VertexFieldProcessor::on_descriptors_created()
{
    if (!m_needs_cluster_id || !m_network_buffer || m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const auto handle = m_network_buffer->read_state_handle("hash_cluster_id");
    const auto bytes = m_network_buffer->get_state_bytes("hash_cluster_id");

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], m_operator->get_vertex_binding() + 1,
        vk::DescriptorType::eStorageBuffer, handle, 0, bytes);
}

} // namespace MayaFlux::Buffers
