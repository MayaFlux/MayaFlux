#include "SkinProcessor.hpp"

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Buffers {

namespace {

    constexpr uint32_t k_rest_words = 10;
    constexpr uint32_t k_mesh_vertex_words = sizeof(Kakshya::MeshVertex) / sizeof(float);

    constexpr std::array<std::string_view, 5> k_names { "rest", "joints", "weights", "palette", "vertices" };

} // namespace

Portal::Graphics::ShaderSpec skin_spec(uint32_t workgroup)
{
    using Portal::Graphics::BindingDirection;
    using Kakshya::GpuDataFormat;

    return Portal::Graphics::ShaderSpec::Assemble {}
        .tmpl(Portal::Graphics::KernelTemplate::Skin)
        .ssbo("rest", BindingDirection::Input, GpuDataFormat::FLOAT32)
        .ssbo("joints", BindingDirection::Input, GpuDataFormat::UINT32)
        .ssbo("weights", BindingDirection::Input, GpuDataFormat::FLOAT32)
        .ssbo("palette", BindingDirection::Input, GpuDataFormat::FLOAT32)
        .ssbo("vertices", BindingDirection::InOut, GpuDataFormat::FLOAT32)
        .pc("first_vertex", GpuDataFormat::UINT32)
        .pc("vertex_count", GpuDataFormat::UINT32)
        .pc("stride_words", GpuDataFormat::UINT32)
        .pc("palette_base", GpuDataFormat::UINT32)
        .workgroup(workgroup)
        .build();
}

SkinProcessor::SkinProcessor(
    std::shared_ptr<const Kakshya::MeshSkin> skin,
    std::span<const uint8_t> rest_vertices,
    std::string tag)
    : ComputeProcessor(skin_spec())
    , m_skin(std::move(skin))
    , m_tag(std::move(tag))
{
    const size_t vertices = rest_vertices.size() / sizeof(Kakshya::MeshVertex);
    if (!m_skin || rest_vertices.size() % sizeof(Kakshya::MeshVertex) != 0 || !m_skin->valid(vertices)) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SkinProcessor: the skin is missing or invalid, or does not cover the {} vertices given",
            vertices);
    }

    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;

    m_rest.reserve(vertices * k_rest_words);
    for (size_t v = 0; v < vertices; ++v) {
        Kakshya::MeshVertex vertex {};
        std::memcpy(&vertex, rest_vertices.data() + v * sizeof(Kakshya::MeshVertex), sizeof(vertex));
        for (const float value : { vertex.position.x, vertex.position.y, vertex.position.z,
                 vertex.normal.x, vertex.normal.y, vertex.normal.z,
                 vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, vertex.tangent.w }) {
            m_rest.push_back(value);
        }
    }

    const auto spec = skin_spec();
    for (size_t i = 0; i < k_names.size(); ++i) {
        m_binding_index[i] = spec.bindings[i].binding_index;
        add_binding(std::string(k_names[i]),
            ShaderBinding(0, m_binding_index[i], Portal::Graphics::DescriptorRole::STORAGE));
    }

    m_params.vertex_count = static_cast<uint32_t>(vertices);
    m_params.stride_words = k_mesh_vertex_words;
    set_push_constant_data(m_params);
}

std::string SkinProcessor::field_name(std::string_view role) const
{
    return m_tag.empty() ? "skin_" + std::string(role) : "skin_" + m_tag + "_" + std::string(role);
}

bool SkinProcessor::set_palette(std::vector<glm::mat4> palette)
{
    if (palette.size() != m_skin->joints.size()) {
        return false;
    }
    m_palette.publish(std::move(palette));
    m_palette_revision.fetch_add(1, std::memory_order_release);
    return true;
}

void SkinProcessor::set_first_vertex(uint32_t first_vertex)
{
    m_params.first_vertex = first_vertex;
}

void SkinProcessor::set_first_vertex_source(std::function<uint32_t()> source)
{
    m_first_source = std::move(source);
}

void SkinProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    m_state_buffer = std::dynamic_pointer_cast<StateFieldBuffer>(buffer);
    if (!m_state_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SkinProcessor requires a StateFieldBuffer, which keeps its rest records, joints and palette as state fields");
        return;
    }

    const size_t vertices = m_skin->vertex_count();
    const bool declared = m_state_buffer->declare_state(field_name("rest"), vertices * k_rest_words, sizeof(float), false)
        && m_state_buffer->declare_state(field_name("joints"), vertices * 4, sizeof(uint32_t), false)
        && m_state_buffer->declare_state(field_name("weights"), vertices * 4, sizeof(float), false)
        && m_state_buffer->declare_state(field_name("palette"), m_skin->joints.size() * 16, sizeof(float), false);
    if (!declared) {
        m_state_buffer.reset();
        return;
    }
    m_declared = true;

    upload_back_buffer(m_state_buffer->write_state_slot(field_name("rest")),
        m_rest.data(), m_rest.size() * sizeof(float), m_staging);
    upload_back_buffer(m_state_buffer->write_state_slot(field_name("joints")),
        m_skin->influence_joints.data(), vertices * 4 * sizeof(uint32_t), m_staging);
    upload_back_buffer(m_state_buffer->write_state_slot(field_name("weights")),
        m_skin->influence_weights.data(), vertices * 4 * sizeof(float), m_staging);

    if (const auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        bind_buffer("vertices", vk_buffer);
    }

    ComputeProcessor::on_attach(buffer);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "SkinProcessor attached: {} vertices, {} joints", vertices, m_skin->joints.size());
}

bool SkinProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer || !m_declared) {
        return false;
    }

    const uint64_t revision = m_palette_revision.load(std::memory_order_acquire);
    if (revision == 0) {
        return false;
    }

    if (revision != m_uploaded_revision) {
        const auto held = m_palette.read();
        if (!held || held->size() != m_skin->joints.size()) {
            return false;
        }
        upload_back_buffer(m_state_buffer->write_state_slot(field_name("palette")),
            held->data(), held->size() * sizeof(glm::mat4), m_staging);
        m_uploaded_revision = revision;
    }

    if (const auto layout = buffer->get_vertex_layout(); layout && layout->stride_bytes >= sizeof(Kakshya::MeshVertex)) {
        m_params.stride_words = layout->stride_bytes / sizeof(float);
    } else {
        m_params.stride_words = k_mesh_vertex_words;
    }

    if (m_first_source) {
        m_params.first_vertex = m_first_source();
    }

    const size_t capacity = buffer->get_size_bytes() / (static_cast<size_t>(m_params.stride_words) * sizeof(float));
    if (static_cast<size_t>(m_params.first_vertex) + m_params.vertex_count > capacity) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SkinProcessor: the skin covers records [{}, {}) but the buffer holds {}. Dispatch suppressed.",
            m_params.first_vertex, m_params.first_vertex + m_params.vertex_count, capacity);
        return false;
    }

    set_push_constant_data(m_params);
    return true;
}

std::array<uint32_t, 3> SkinProcessor::calculate_dispatch_size(const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    const uint32_t local = std::max(1U, get_dispatch_config().workgroup_x);
    return { std::max(1U, (m_params.vertex_count + local - 1) / local), 1U, 1U };
}

void SkinProcessor::collect_push_writes(
    const std::shared_ptr<VKBuffer>& buffer,
    std::vector<Portal::Graphics::PushDescriptorWrite>& writes)
{
    ComputeProcessor::collect_push_writes(buffer, writes);

    if (!m_declared || !m_state_buffer) {
        return;
    }

    constexpr std::array<std::string_view, 4> roles { "rest", "joints", "weights", "palette" };
    for (size_t i = 0; i < roles.size(); ++i) {
        const std::string name = field_name(roles[i]);
        writes.push_back({
            .binding = m_binding_index[i],
            .type = Portal::Graphics::to_vk_descriptor_type(Portal::Graphics::DescriptorRole::STORAGE),
            .buffer = vk::DescriptorBufferInfo(
                m_state_buffer->read_state_handle(name), 0, m_state_buffer->get_state_bytes(name)),
        });
    }
}

} // namespace MayaFlux::Buffers
