#include "SkinProcessor.hpp"

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Buffers {

namespace {

    constexpr uint32_t k_rest_words = 10;
    constexpr uint32_t k_mesh_vertex_words = sizeof(Kakshya::MeshVertex) / sizeof(float);

    constexpr std::array<std::string_view, 7> k_names { "rest", "joints", "weights", "palette", "vertices", "deltas", "morph_weights" };

} // namespace

std::vector<float> shape_offsets(
    const Kakshya::MeshMorphTargets& targets, std::span<const Kakshya::MeshVertex> base)
{
    std::vector<float> offsets(std::max<size_t>(1, targets.size() * base.size() * 9), 0.0F);
    for (size_t t = 0; t < targets.size(); ++t) {
        const auto& shape = targets[t].second;
        for (size_t v = 0; shape.size() == base.size() && v < base.size(); ++v) {
            float* out = offsets.data() + (t * base.size() + v) * 9;
            for (glm::length_t c = 0; c < 3; ++c) {
                out[c] = shape[v].position[c] - base[v].position[c];
                out[3 + c] = shape[v].normal[c] - base[v].normal[c];
                out[6 + c] = shape[v].tangent[c] - base[v].tangent[c];
            }
        }
    }
    return offsets;
}

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
        .ssbo("deltas", BindingDirection::Input, GpuDataFormat::FLOAT32)
        .ssbo("morph_weights", BindingDirection::Input, GpuDataFormat::FLOAT32)
        .pc("first_vertex", GpuDataFormat::UINT32)
        .pc("vertex_count", GpuDataFormat::UINT32)
        .pc("stride_words", GpuDataFormat::UINT32)
        .pc("palette_base", GpuDataFormat::UINT32)
        .pc("target_count", GpuDataFormat::UINT32)
        .workgroup(workgroup)
        .build();
}

SkinProcessor::SkinProcessor(
    std::shared_ptr<const Kakshya::MeshSkin> skin,
    std::span<const uint8_t> rest_vertices,
    std::string tag,
    std::shared_ptr<const Kakshya::MeshMorphTargets> morphs)
    : ComputeProcessor(skin_spec())
    , m_skin(std::move(skin))
    , m_morphs(std::move(morphs))
    , m_tag(std::move(tag))
{
    const size_t vertices = rest_vertices.size() / sizeof(Kakshya::MeshVertex);
    const bool targets_fit = !m_morphs
        || std::ranges::all_of(*m_morphs, [vertices](const auto& target) { return target.second.size() == vertices; });
    if ((!m_skin && target_count() == 0) || vertices == 0 || rest_vertices.size() % sizeof(Kakshya::MeshVertex) != 0
        || (m_skin && !m_skin->valid(vertices)) || !targets_fit) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SkinProcessor: there is no skin or morph target, or they do not cover the {} vertices given",
            vertices);
    }

    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    m_params.vertex_count = static_cast<uint32_t>(vertices);
    m_params.stride_words = k_mesh_vertex_words;
    m_params.target_count = static_cast<uint32_t>(target_count());
    pack_rest(rest_vertices);

    std::vector<Kakshya::MeshVertex> base(vertices);
    std::memcpy(base.data(), rest_vertices.data(), rest_vertices.size());
    m_deltas = shape_offsets(m_morphs ? *m_morphs : Kakshya::MeshMorphTargets {}, base);

    if (target_count() > 0) {
        std::vector<float> initial;
        initial.reserve(target_count());
        for (const auto& [name, shape] : *m_morphs) {
            initial.push_back(shape.front().weight);
        }
        m_morph_weights.publish(std::move(initial));
        m_weight_revision.store(1, std::memory_order_release);

        m_palette.publish(std::vector<glm::mat4>(joint_count(), glm::mat4(1.0F)));
        m_palette_revision.store(1, std::memory_order_release);
    }

    const auto spec = skin_spec();
    for (size_t i = 0; i < k_names.size(); ++i) {
        m_binding_index[i] = spec.bindings[i].binding_index;
        add_binding(std::string(k_names[i]),
            ShaderBinding(0, m_binding_index[i], Portal::Graphics::DescriptorRole::STORAGE));
    }

    set_push_constant_data(m_params);
}

std::string SkinProcessor::field_name(std::string_view role) const
{
    return m_tag.empty() ? "skin_" + std::string(role) : "skin_" + m_tag + "_" + std::string(role);
}

size_t SkinProcessor::joint_count() const noexcept
{
    return m_skin ? m_skin->joint_count() : 1;
}

size_t SkinProcessor::target_count() const noexcept
{
    return m_morphs ? m_morphs->size() : 0;
}

void SkinProcessor::pack_rest(std::span<const uint8_t> rest_vertices)
{
    const size_t vertices = rest_vertices.size() / sizeof(Kakshya::MeshVertex);
    m_rest.resize(vertices * k_rest_words);
    for (size_t v = 0; v < vertices; ++v) {
        Kakshya::MeshVertex vertex {};
        std::memcpy(&vertex, rest_vertices.data() + v * sizeof(Kakshya::MeshVertex), sizeof(vertex));
        const std::array<float, k_rest_words> record {
            vertex.position.x, vertex.position.y, vertex.position.z,
            vertex.normal.x, vertex.normal.y, vertex.normal.z,
            vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, vertex.tangent.w
        };
        std::ranges::copy(record, m_rest.begin() + static_cast<std::ptrdiff_t>(v * k_rest_words));
    }
}

void SkinProcessor::set_rest(std::span<const uint8_t> rest_vertices)
{
    const bool fits = rest_vertices.size() == static_cast<size_t>(m_params.vertex_count) * sizeof(Kakshya::MeshVertex);
    if (!fits) {
        if (m_rest_fits) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "SkinProcessor: new geometry of {} vertices where the skin or targets cover {}, so it is shown undeformed",
                rest_vertices.size() / sizeof(Kakshya::MeshVertex), m_params.vertex_count);
        }
        m_rest_fits = false;
        return;
    }
    m_rest_fits = true;
    const auto previous = std::exchange(m_rest, {});
    pack_rest(rest_vertices);
    m_rest_pending = m_rest_pending || m_rest != previous;
}

bool SkinProcessor::set_palette(std::vector<glm::mat4> palette)
{
    if (!m_skin || palette.size() != joint_count()) {
        return false;
    }
    m_palette.publish(std::move(palette));
    m_palette_revision.fetch_add(1, std::memory_order_release);
    return true;
}

std::vector<glm::mat4> SkinProcessor::get_palette() const
{
    const auto held = m_palette.read();
    return held ? *held : std::vector<glm::mat4> {};
}

bool SkinProcessor::set_morph_weights(std::vector<float> weights)
{
    if (target_count() == 0 || weights.size() != target_count()) {
        return false;
    }
    m_morph_weights.publish(std::move(weights));
    m_weight_revision.fetch_add(1, std::memory_order_release);
    return true;
}

std::vector<float> SkinProcessor::get_morph_weights() const
{
    const auto held = m_morph_weights.read();
    return held ? *held : std::vector<float> {};
}

void SkinProcessor::set_palette_source(std::function<std::vector<glm::mat4>()> source)
{
    m_palette_source = std::move(source);
    m_sourced_palette.clear();
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

    const size_t vertices = m_params.vertex_count;
    const bool declared = m_state_buffer->declare_state(field_name("rest"), vertices * k_rest_words, sizeof(float), false)
        && m_state_buffer->declare_state(field_name("joints"), vertices * 4, sizeof(uint32_t), false)
        && m_state_buffer->declare_state(field_name("weights"), vertices * 4, sizeof(float), false)
        && m_state_buffer->declare_state(field_name("palette"), joint_count() * 16, sizeof(float), false)
        && m_state_buffer->declare_state(field_name("deltas"), m_deltas.size(), sizeof(float), false)
        && m_state_buffer->declare_state(field_name("morph_weights"), std::max<size_t>(1, target_count()), sizeof(float), false);
    if (!declared) {
        m_state_buffer.reset();
        return;
    }
    m_declared = true;

    const std::vector<glm::uvec4> no_joints(m_skin ? 0 : vertices, glm::uvec4(0));
    const std::vector<glm::vec4> no_weights(m_skin ? 0 : vertices, glm::vec4(0.0F));

    upload_back_buffer(m_state_buffer->write_state_slot(field_name("rest")),
        m_rest.data(), m_rest.size() * sizeof(float), m_staging);
    upload_back_buffer(m_state_buffer->write_state_slot(field_name("joints")),
        m_skin ? m_skin->influence_joints.data() : no_joints.data(), vertices * sizeof(glm::uvec4), m_staging);
    upload_back_buffer(m_state_buffer->write_state_slot(field_name("weights")),
        m_skin ? m_skin->influence_weights.data() : no_weights.data(), vertices * sizeof(glm::vec4), m_staging);
    upload_back_buffer(m_state_buffer->write_state_slot(field_name("deltas")),
        m_deltas.data(), m_deltas.size() * sizeof(float), m_staging);
    m_rest_pending = false;
    m_uploaded_revision = 0;
    m_uploaded_weight_revision = 0;

    if (const auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        bind_buffer("vertices", vk_buffer);
    }

    ComputeProcessor::on_attach(buffer);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "SkinProcessor attached: {} vertices, {} joints, {} morph targets",
        vertices, m_skin ? m_skin->joint_count() : 0, target_count());
}

bool SkinProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    m_active = false;
    if (!buffer || !m_declared || !m_rest_fits) {
        return false;
    }

    if (m_palette_source) {
        auto sourced = m_palette_source();
        if (sourced != m_sourced_palette && set_palette(sourced)) {
            m_sourced_palette = std::move(sourced);
        }
    }

    const uint64_t revision = m_palette_revision.load(std::memory_order_acquire);
    if (revision == 0) {
        return false;
    }

    if (m_rest_pending) {
        upload_back_buffer(m_state_buffer->write_state_slot(field_name("rest")),
            m_rest.data(), m_rest.size() * sizeof(float), m_staging);
        m_rest_pending = false;
    }

    if (revision != m_uploaded_revision) {
        const auto held = m_palette.read();
        if (!held || held->size() != joint_count()) {
            return false;
        }
        upload_back_buffer(m_state_buffer->write_state_slot(field_name("palette")),
            held->data(), held->size() * sizeof(glm::mat4), m_staging);
        m_uploaded_revision = revision;
    }

    const uint64_t weight_revision = m_weight_revision.load(std::memory_order_acquire);
    if (weight_revision != m_uploaded_weight_revision) {
        const auto held = m_morph_weights.read();
        if (!held || held->size() != target_count()) {
            return false;
        }
        upload_back_buffer(m_state_buffer->write_state_slot(field_name("morph_weights")),
            held->data(), held->size() * sizeof(float), m_staging);
        m_uploaded_weight_revision = weight_revision;
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
            "SkinProcessor: the pass covers records [{}, {}) but the buffer holds {}. Dispatch suppressed.",
            m_params.first_vertex, m_params.first_vertex + m_params.vertex_count, capacity);
        return false;
    }

    set_push_constant_data(m_params);
    m_active = true;
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

    for (size_t i = 0; i < k_names.size(); ++i) {
        if (k_names[i] == "vertices") {
            continue;
        }
        const std::string name = field_name(k_names[i]);
        writes.push_back({
            .binding = m_binding_index[i],
            .type = Portal::Graphics::to_vk_descriptor_type(Portal::Graphics::DescriptorRole::STORAGE),
            .buffer = vk::DescriptorBufferInfo(
                m_state_buffer->read_state_handle(name), 0, m_state_buffer->get_state_bytes(name)),
        });
    }
}

} // namespace MayaFlux::Buffers
