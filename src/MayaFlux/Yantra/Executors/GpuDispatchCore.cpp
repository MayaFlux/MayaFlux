#include "GpuDispatchCore.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Yantra {

GpuDispatchCore::GpuDispatchCore(GpuShaderConfig config)
    : m_gpu_config(std::move(config))
{
}

//==============================================================================
// Public interface
//==============================================================================

void GpuDispatchCore::set_push_constants(const void* data, size_t bytes)
{
    m_push_constants.resize(bytes);
    std::memcpy(m_push_constants.data(), data, bytes);
}

void GpuDispatchCore::set_output_size(size_t index, size_t byte_size)
{
    if (index >= m_output_size_overrides.size())
        m_output_size_overrides.resize(index + 1, 0);
    m_output_size_overrides[index] = byte_size;
}

bool GpuDispatchCore::ensure_gpu_ready()
{
    if (m_resources.is_ready())
        return true;
    m_bindings = declare_buffer_bindings();
    return m_resources.initialise(m_gpu_config, m_bindings);
}

bool GpuDispatchCore::is_gpu_ready() const
{
    return m_resources.is_ready();
}

std::shared_ptr<Core::VKImage> GpuDispatchCore::get_output_image(size_t binding_index) const
{
    if (binding_index >= m_image_bindings.size())
        return nullptr;
    return m_image_bindings[binding_index].image;
}

const GpuShaderConfig& GpuDispatchCore::gpu_config() const
{
    return m_gpu_config;
}

//==============================================================================
// Protected staging helpers
//==============================================================================

void GpuDispatchCore::stage_passthrough(size_t binding_index, const void* data, size_t byte_size)
{
    if (binding_index >= m_passthrough_bytes.size())
        m_passthrough_bytes.resize(binding_index + 1);
    auto& slot = m_passthrough_bytes[binding_index];
    slot.resize(byte_size);
    std::memcpy(slot.data(), data, byte_size);
}

void GpuDispatchCore::stage_image_storage(size_t binding_index, std::shared_ptr<Core::VKImage> image)
{
    if (binding_index >= m_image_bindings.size())
        m_image_bindings.resize(binding_index + 1);
    m_image_bindings[binding_index] = { .image = std::move(image), .sampler = nullptr, .kind = GpuBufferBinding::ElementType::IMAGE_STORAGE };
}

void GpuDispatchCore::stage_image_sampled(size_t binding_index,
    std::shared_ptr<Core::VKImage> image,
    vk::Sampler sampler)
{
    if (binding_index >= m_image_bindings.size())
        m_image_bindings.resize(binding_index + 1);
    m_image_bindings[binding_index] = { .image = std::move(image), .sampler = sampler, .kind = GpuBufferBinding::ElementType::IMAGE_SAMPLED };
}

//==============================================================================
// Virtual override points
//==============================================================================

std::vector<GpuBufferBinding> GpuDispatchCore::declare_buffer_bindings() const
{
    return {
        { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
    };
}

void GpuDispatchCore::on_before_gpu_dispatch(
    const std::vector<std::vector<double>>&,
    const DataStructureInfo&)
{
}

void GpuDispatchCore::prepare_gpu_inputs(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info)
{
    flatten_channels_to_staging(channels, structure_info);
    const size_t float_byte_size = m_staging_floats.size() * sizeof(float);

    const size_t fallback_bytes = float_byte_size > 0
        ? float_byte_size
        : Kakshya::ContainerDataStructure::get_total_elements(structure_info.dimensions) * sizeof(float);

    for (size_t i = 0; i < m_bindings.size(); ++i) {
        const auto& b = m_bindings[i];

        if (i < m_binding_data.size() && !m_binding_data[i].empty()) {
            m_resources.ensure_buffer(i, m_binding_data[i].size());
            m_resources.upload_raw(i, m_binding_data[i].data(), m_binding_data[i].size());
            continue;
        }

        if (b.direction == GpuBufferBinding::Direction::OUTPUT) {
            const size_t sz = (i < m_output_size_overrides.size() && m_output_size_overrides[i] > 0)
                ? m_output_size_overrides[i]
                : fallback_bytes;
            m_resources.ensure_buffer(i, sz);
            if (i < m_output_size_overrides.size() && m_output_size_overrides[i] > 0) {
                std::vector<uint8_t> zeros(sz, 0);
                m_resources.upload_raw(i, zeros.data(), sz);
            }
            continue;
        }

        switch (b.element_type) {
        case GpuBufferBinding::ElementType::PASSTHROUGH:
            if (i < m_passthrough_bytes.size() && !m_passthrough_bytes[i].empty()) {
                m_resources.ensure_buffer(i, m_passthrough_bytes[i].size());
                m_resources.upload_raw(i, m_passthrough_bytes[i].data(),
                    m_passthrough_bytes[i].size());
            }
            break;

        case GpuBufferBinding::ElementType::IMAGE_SAMPLED: {
            if (i >= m_image_bindings.size() || !m_image_bindings[i].image)
                continue;
            auto& img = m_image_bindings[i].image;
            if (img->get_current_layout() != vk::ImageLayout::eGeneral) {
                m_resources.transition_image(img, img->get_current_layout(),
                    vk::ImageLayout::eGeneral);
            }
            m_resources.bind_image_storage(i, img, b);
        } break;

        case GpuBufferBinding::ElementType::IMAGE_STORAGE: {
            if (i >= m_image_bindings.size() || !m_image_bindings[i].image)
                continue;
            auto& img = m_image_bindings[i].image;
            auto sampler = m_image_bindings[i].sampler;
            if (img->get_current_layout() != vk::ImageLayout::eShaderReadOnlyOptimal) {
                m_resources.transition_image(img, img->get_current_layout(),
                    vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            m_resources.bind_image_sampled(i, img, sampler, b);
        } break;

        case GpuBufferBinding::ElementType::UINT32:
        case GpuBufferBinding::ElementType::INT32:
            if (!channels.empty()) {
                const size_t raw_bytes = channels[0].size()
                    * (b.element_type == GpuBufferBinding::ElementType::UINT32
                            ? sizeof(uint32_t)
                            : sizeof(int32_t));
                m_resources.ensure_buffer(i, raw_bytes);
                m_resources.upload_raw(i,
                    reinterpret_cast<const uint8_t*>(channels[0].data()),
                    raw_bytes);
            }
            break;

        case GpuBufferBinding::ElementType::FLOAT32:
        default:
            m_resources.ensure_buffer(i, float_byte_size);
            m_resources.upload(i, m_staging_floats.data(), float_byte_size);
            break;
        }
    }
}

std::array<uint32_t, 3> GpuDispatchCore::calculate_dispatch_size(
    size_t total_elements, const DataStructureInfo& structure_info) const
{
    uint64_t sz_x = 0, sz_y = 0, sz_z = 0;
    for (const auto& dim : structure_info.dimensions) {
        switch (dim.role) {
        case Kakshya::DataDimension::Role::SPATIAL_X:
            sz_x = dim.size;
            break;
        case Kakshya::DataDimension::Role::SPATIAL_Y:
            sz_y = dim.size;
            break;
        case Kakshya::DataDimension::Role::SPATIAL_Z:
            sz_z = dim.size;
            break;
        default:
            break;
        }
    }

    const auto& ws = m_gpu_config.workgroup_size;
    if (sz_x > 0) {
        return {
            static_cast<uint32_t>((sz_x + ws[0] - 1) / ws[0]),
            sz_y > 0 ? static_cast<uint32_t>((sz_y + ws[1] - 1) / ws[1]) : 1U,
            sz_z > 0 ? static_cast<uint32_t>((sz_z + ws[2] - 1) / ws[2]) : 1U,
        };
    }

    return { static_cast<uint32_t>((total_elements + ws[0] - 1) / ws[0]), 1U, 1U };
}

//==============================================================================
// Dispatch
//==============================================================================

GpuChannelResult GpuDispatchCore::dispatch_core(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info)
{
    prepare_gpu_inputs(channels, structure_info);

    for (size_t i = 0; i < m_bindings.size(); ++i)
        m_resources.bind_descriptor(i, m_bindings[i]);

    on_before_gpu_dispatch(channels, structure_info);

    const size_t effective = m_staging_floats.empty()
        ? largest_binding_data_element_count()
        : m_staging_floats.size();
    const auto groups = calculate_dispatch_size(effective, structure_info);

    m_resources.dispatch(
        groups, m_bindings,
        m_push_constants.empty() ? nullptr : m_push_constants.data(),
        m_push_constants.size());

    GpuChannelResult result;
    result.primary = readback_primary(effective);
    readback_aux(result);
    return result;
}

GpuChannelResult GpuDispatchCore::dispatch_core_chained(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info,
    const ExecutionContext& ctx)
{
    prepare_gpu_inputs(channels, structure_info);

    for (size_t i = 0; i < m_bindings.size(); ++i)
        m_resources.bind_descriptor(i, m_bindings[i]);

    const size_t effective = m_staging_floats.empty()
        ? largest_binding_data_element_count()
        : m_staging_floats.size();
    const auto groups = calculate_dispatch_size(effective, structure_info);

    if (!ctx.execution_metadata.contains("pass_count") || !ctx.execution_metadata.contains("pc_updater")) {
        error<std::runtime_error>(Journal::Component::Yantra,
            Journal::Context::Runtime,
            std::source_location::current(),
            "GpuDispatchCore: dispatch_core_chained requires 'pass_count' and 'pc_updater' in execution_metadata");
    }

    const auto pass_count = safe_any_cast_or_throw<uint32_t>(ctx.execution_metadata.at("pass_count"));
    const auto& pc_updater = safe_any_cast_or_throw<std::function<void(uint32_t, void*)>>(ctx.execution_metadata.at("pc_updater"));

    m_resources.dispatch_batched(
        pass_count, groups, m_bindings,
        [&](uint32_t pass, std::vector<uint8_t>& pc_data) { pc_updater(pass, pc_data.data()); },
        m_gpu_config.push_constant_size,
        ctx.execution_metadata);

    GpuChannelResult result;
    result.primary = readback_primary(effective);
    readback_aux(result);
    return result;
}

//==============================================================================
// Readback helpers
//==============================================================================

std::vector<float> GpuDispatchCore::readback_primary(size_t float_count)
{
    const size_t idx = find_first_output_index();
    const size_t allocated = m_resources.buffer_allocated_bytes(idx);
    const size_t byte_size = std::min(float_count * sizeof(float), allocated);
    std::vector<float> out(byte_size / sizeof(float));
    m_resources.download(idx, out.data(), byte_size);
    return out;
}

void GpuDispatchCore::readback_aux(GpuChannelResult& result)
{
    for (size_t i = 0; i < m_bindings.size(); ++i) {
        const auto dir = m_bindings[i].direction;
        if ((dir == GpuBufferBinding::Direction::OUTPUT || dir == GpuBufferBinding::Direction::INPUT_OUTPUT)
            && i < m_output_size_overrides.size()
            && m_output_size_overrides[i] > 0) {
            const size_t sz = m_output_size_overrides[i];
            std::vector<uint8_t> raw(sz);
            m_resources.download(i, reinterpret_cast<float*>(raw.data()), sz);
            result.aux[i] = std::move(raw);
        }
    }
}

//==============================================================================
// Internal helpers
//==============================================================================

void GpuDispatchCore::flatten_channels_to_staging(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info)
{
    m_staging_floats.clear();

    if (Kakshya::is_structured_modality(structure_info.modality))
        return;

    bool all_inputs_staged = !m_bindings.empty();
    for (size_t i = 0; i < m_bindings.size(); ++i) {
        if (m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT)
            continue;
        if (i >= m_binding_data.size() || m_binding_data[i].empty()) {
            all_inputs_staged = false;
            break;
        }
    }
    if (all_inputs_staged)
        return;

    size_t total = 0;

    for (const auto& ch : channels)
        total += ch.size();
    m_staging_floats.reserve(total);

    for (const auto& ch : channels) {
        for (double v : ch)
            m_staging_floats.push_back(static_cast<float>(v));
    }
}

size_t GpuDispatchCore::find_first_output_index() const
{
    size_t first_inout = SIZE_MAX;
    for (size_t i = 0; i < m_bindings.size(); ++i) {
        if (m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT)
            return i;
        if (m_bindings[i].direction == GpuBufferBinding::Direction::INPUT_OUTPUT
            && first_inout == SIZE_MAX)
            first_inout = i;
    }
    if (first_inout != SIZE_MAX)
        return first_inout;

    error<std::runtime_error>(Journal::Component::Yantra,
        Journal::Context::BufferProcessing,
        std::source_location::current(),
        "GpuDispatchCore: no output buffer declared");
}

size_t GpuDispatchCore::largest_binding_data_element_count() const
{
    size_t max_bytes = 0;

    for (size_t i = 0; i < m_bindings.size(); ++i) {
        if (m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT)
            continue;
        if (i < m_binding_data.size() && !m_binding_data[i].empty())
            max_bytes = std::max(max_bytes, m_binding_data[i].size());
    }

    return max_bytes / sizeof(float);
}

} // namespace MayaFlux::Yantra
