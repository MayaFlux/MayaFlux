#include "RaymarchProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr const char* k_binding_name = "volume_field";
    constexpr const char* k_fragment_name = "march_params";
}

RaymarchProcessor::RaymarchProcessor(
    FieldSource source,
    size_t field_bytes,
    Kinesis::Lattice3D lattice,
    uint32_t set,
    uint32_t binding)
    : m_source(std::move(source))
    , m_field_bytes(field_bytes)
    , m_lattice(lattice)
    , m_set(set)
    , m_binding(binding)
{
    if (m_set == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "RaymarchProcessor: set 0 is the ViewTransform reservation, "
            "falling back to set 1");
        m_set = 1;
    }

    m_params.max_steps = 128;
    m_params.step_scale = 0.5F;
    m_params.density_scale = 1.0F;
    m_params.absorption = 12.0F;
    m_params.cool_r = 0.35F;
    m_params.cool_g = 0.10F;
    m_params.cool_b = 0.05F;
    m_params.hot_r = 1.0F;
    m_params.hot_g = 0.82F;
    m_params.hot_b = 0.45F;
    m_params.emission = 2.2F;
    m_params.threshold = 0.02F;

    write_lattice_params();
}

void RaymarchProcessor::write_lattice_params()
{
    const glm::vec3 cell = m_lattice.cell_size();

    m_params.width = m_lattice.resolution.x;
    m_params.height = m_lattice.resolution.y;
    m_params.depth = m_lattice.resolution.z;
    m_params.bounds_min_x = m_lattice.bounds.min.x;
    m_params.bounds_min_y = m_lattice.bounds.min.y;
    m_params.bounds_min_z = m_lattice.bounds.min.z;
    m_params.cell_size_x = cell.x;
    m_params.cell_size_y = cell.y;
    m_params.cell_size_z = cell.z;
}

void RaymarchProcessor::set_max_steps(uint32_t steps) { m_params.max_steps = steps; }
void RaymarchProcessor::set_step_scale(float scale) { m_params.step_scale = scale; }
void RaymarchProcessor::set_density_scale(float scale) { m_params.density_scale = scale; }
void RaymarchProcessor::set_absorption(float absorption) { m_params.absorption = absorption; }
void RaymarchProcessor::set_emission(float emission) { m_params.emission = emission; }
void RaymarchProcessor::set_threshold(float threshold) { m_params.threshold = threshold; }

void RaymarchProcessor::set_emission_ramp(const glm::vec3& cool, const glm::vec3& hot)
{
    m_params.cool_r = cool.r;
    m_params.cool_g = cool.g;
    m_params.cool_b = cool.b;
    m_params.hot_r = hot.r;
    m_params.hot_g = hot.g;
    m_params.hot_b = hot.b;
}

void RaymarchProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "RaymarchProcessor requires a VKBuffer");
        return;
    }

    stage_params(vk_buffer);

    if (m_source) {
        stage_descriptor(vk_buffer, m_source());
    }
}

void RaymarchProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer || !m_source) {
        return;
    }

    const vk::Buffer handle = m_source();
    if (!handle) {
        return;
    }

    stage_descriptor(vk_buffer, handle);
    stage_params(vk_buffer);
}

void RaymarchProcessor::stage_descriptor(
    const std::shared_ptr<VKBuffer>& buffer, vk::Buffer handle)
{
    auto& bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;

    auto it = std::ranges::find_if(bindings, [this](const auto& entry) {
        return entry.set == m_set && entry.binding == m_binding;
    });

    if (it == bindings.end()) {
        bindings.push_back(Portal::Graphics::DescriptorBindingInfo {
            .set = m_set,
            .binding = m_binding,
            .type = vk::DescriptorType::eStorageBuffer,
            .buffer_info = vk::DescriptorBufferInfo(handle, 0, m_field_bytes),
            .name = k_binding_name,
            .count = 1,
        });
        return;
    }

    it->buffer_info.buffer = handle;
    it->buffer_info.offset = 0;
    it->buffer_info.range = m_field_bytes;
}

void RaymarchProcessor::stage_params(const std::shared_ptr<VKBuffer>& buffer)
{
    auto& fragments = buffer->get_pipeline_context().push_constant_bindings;

    const auto* bytes = reinterpret_cast<const uint8_t*>(&m_params);

    auto it = std::ranges::find_if(fragments, [](const auto& entry) {
        return entry.name == k_fragment_name;
    });

    if (it == fragments.end()) {
        fragments.push_back(Portal::Graphics::PushConstantBindingInfo {
            .offset = 0,
            .data = std::vector<uint8_t>(bytes, bytes + sizeof(MarchParams)),
            .name = k_fragment_name,
        });
        return;
    }

    it->offset = 0;
    it->data.assign(bytes, bytes + sizeof(MarchParams));
}

} // namespace MayaFlux::Buffers
