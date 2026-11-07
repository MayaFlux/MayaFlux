#include "GpuDispatchCore.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Yantra {

GpuDispatchCore::GpuDispatchCore(GpuComputeConfig config, bool compute_isolated)
    : m_resources(compute_isolated)
    , m_gpu_config(std::move(config))
{
    update_dispatch_key_cache();
}

//==============================================================================
// Public interface
//==============================================================================

namespace {

    constexpr bool native_accepts(GpuBufferBinding::ElementType binding, Kakshya::GpuDataFormat native)
    {
        return (binding == GpuBufferBinding::ElementType::FLOAT32 && native == Kakshya::GpuDataFormat::FLOAT32)
            || (binding == GpuBufferBinding::ElementType::UINT32 && native == Kakshya::GpuDataFormat::UINT32);
    }

} // namespace

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
    if (m_resources.is_ready(dispatch_key()))
        return true;
    m_bindings = declare_buffer_bindings();
    return m_resources.initialise(dispatch_key(), m_gpu_config, m_bindings);
}

bool GpuDispatchCore::is_gpu_ready() const
{
    return m_resources.is_ready(dispatch_key());
}

std::shared_ptr<Core::VKImage> GpuDispatchCore::get_output_image(size_t binding_index) const
{
    if (binding_index >= m_image_bindings.size())
        return nullptr;
    return m_image_bindings[binding_index].image;
}

const GpuComputeConfig& GpuDispatchCore::gpu_config() const
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

void GpuDispatchCore::stage_image_at(size_t binding_index,
    std::shared_ptr<Core::VKImage> image,
    GpuBufferBinding::ElementType kind,
    vk::Sampler sampler)
{
    if (binding_index >= m_image_bindings.size())
        m_image_bindings.resize(binding_index + 1);
    m_image_bindings[binding_index] = {
        .image = std::move(image),
        .sampler = (kind == GpuBufferBinding::ElementType::IMAGE_SAMPLED) ? sampler : nullptr,
        .kind = kind,
    };
}

void GpuDispatchCore::stage_native_bytes(Kakshya::GpuDataFormat format, const void* data, size_t byte_size)
{
    m_native_format = format;
    m_native_staging_bytes.resize(byte_size);
    if (byte_size > 0)
        std::memcpy(m_native_staging_bytes.data(), data, byte_size);
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
    const auto& key = dispatch_key();

    const auto native_format = std::exchange(m_native_format, std::nullopt);
    const std::vector<uint8_t> native_bytes = std::exchange(m_native_staging_bytes, {});
    const bool native_staged = native_format.has_value();
    const Kakshya::GpuDataFormat native_element = native_format.value_or(Kakshya::GpuDataFormat::FLOAT32);

    flatten_channels_to_staging(channels, structure_info);
    const size_t float_byte_size = m_staging_floats.size() * sizeof(float);

    const size_t fallback_bytes = float_byte_size > 0
        ? float_byte_size
        : Kakshya::ContainerDataStructure::get_total_elements(structure_info.dimensions) * sizeof(float);

    std::vector<GpuResourceManager::ImageBind> image_binds;

    for (auto b : m_bindings) {
        const size_t idx = b.binding;

        if (idx < m_binding_data.size() && !m_binding_data[idx].empty()) {
            m_resources.ensure_buffer(key, idx, m_binding_data[idx].size());
            m_resources.upload_raw(key, idx, m_binding_data[idx].data(), m_binding_data[idx].size());
            continue;
        }

        if (b.direction == GpuBufferBinding::Direction::OUTPUT) {
            const auto et = b.element_type;
            if (et == GpuBufferBinding::ElementType::IMAGE_STORAGE
                || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED) {
            } else {
                const size_t sz = (idx < m_output_size_overrides.size() && m_output_size_overrides[idx] > 0)
                    ? m_output_size_overrides[idx]
                    : fallback_bytes;
                m_resources.ensure_buffer(key, idx, sz);
                if (idx < m_output_size_overrides.size() && m_output_size_overrides[idx] > 0) {
                    std::vector<uint8_t> zeros(sz, 0);
                    m_resources.upload_raw(key, idx, zeros.data(), sz);
                }
                continue;
            }
        }

        switch (b.element_type) {
        case GpuBufferBinding::ElementType::PASSTHROUGH:
            if (idx < m_passthrough_bytes.size() && !m_passthrough_bytes[idx].empty()) {
                m_resources.ensure_buffer(key, idx, m_passthrough_bytes[idx].size());
                m_resources.upload_raw(key, idx, m_passthrough_bytes[idx].data(),
                    m_passthrough_bytes[idx].size());
            }
            break;

        case GpuBufferBinding::ElementType::IMAGE_STORAGE: {
            if (idx >= m_image_bindings.size() || !m_image_bindings[idx].image)
                continue;
            auto& img = m_image_bindings[idx].image;
            if (img->get_current_layout() != vk::ImageLayout::eGeneral) {
                m_resources.transition_image(img, vk::ImageLayout::eGeneral);
            }
            image_binds.push_back({ .index = idx, .image = img, .sampler = nullptr, .spec = b });
        } break;

        case GpuBufferBinding::ElementType::IMAGE_SAMPLED: {
            if (idx >= m_image_bindings.size() || !m_image_bindings[idx].image)
                continue;
            auto& img = m_image_bindings[idx].image;
            auto sampler = m_image_bindings[idx].sampler;
            if (img->get_current_layout() != vk::ImageLayout::eShaderReadOnlyOptimal) {
                m_resources.transition_image(img, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
            image_binds.push_back({ .index = idx, .image = img, .sampler = sampler, .spec = b });
        } break;

        case GpuBufferBinding::ElementType::UINT32:
        case GpuBufferBinding::ElementType::INT32:
            if (native_staged && native_accepts(b.element_type, native_element)) {
                m_resources.ensure_buffer(key, idx, native_bytes.size());
                m_resources.upload_raw(key, idx, native_bytes.data(), native_bytes.size());
                break;
            }
            if (!channels.empty()) {
                const size_t raw_bytes = channels[0].size()
                    * (b.element_type == GpuBufferBinding::ElementType::UINT32
                            ? sizeof(uint32_t)
                            : sizeof(int32_t));
                m_resources.ensure_buffer(key, idx, raw_bytes);
                m_resources.upload_raw(key, idx,
                    reinterpret_cast<const uint8_t*>(channels[0].data()),
                    raw_bytes);
            }
            break;

        case GpuBufferBinding::ElementType::FLOAT32:
        default:
            if (native_staged && native_accepts(b.element_type, native_element)) {
                m_resources.ensure_buffer(key, idx, native_bytes.size());
                m_resources.upload_raw(key, idx, native_bytes.data(), native_bytes.size());
            } else {
                m_resources.ensure_buffer(key, idx, float_byte_size);
                m_resources.upload(key, idx, m_staging_floats.data(), float_byte_size);
            }
            break;
        }
    }

    if (native_staged) {
        m_prepared_elements = native_bytes.size() / Kakshya::gpu_data_format_bytes(native_element);
    } else if (!m_staging_floats.empty()) {
        m_prepared_elements = m_staging_floats.size();
    } else {
        m_prepared_elements = largest_binding_data_element_count();
    }

    if (!image_binds.empty())
        m_resources.bind_images_batch(key, image_binds);
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

bool GpuDispatchCore::has_unstaged_active_image()
{
    const auto key = dispatch_key();
    return std::ranges::any_of(m_bindings, [&](const auto& b) {
        const bool is_image = b.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || b.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        if (!is_image || !m_resources.uses_binding(key, b.set, b.binding))
            return false;
        const auto idx = static_cast<size_t>(b.binding);
        return idx >= m_image_bindings.size() || !m_image_bindings[idx].image;
    });
}

bool GpuDispatchCore::native_input_unreceivable()
{
    if (!m_native_format)
        return false;

    const bool receivable = std::ranges::any_of(m_bindings, [&](const auto& b) {
        if (b.direction == GpuBufferBinding::Direction::OUTPUT)
            return false;
        const bool float_or_uint = b.element_type == GpuBufferBinding::ElementType::FLOAT32
            || b.element_type == GpuBufferBinding::ElementType::UINT32;
        if (!float_or_uint)
            return false;
        if (b.binding < m_binding_data.size() && !m_binding_data[b.binding].empty())
            return false;
        return native_accepts(b.element_type, *m_native_format);
    });
    return !receivable;
}

bool GpuDispatchCore::inputs_accepted()
{
    const bool rejected = m_native_rejected || native_input_unreceivable() || has_unstaged_active_image();

    if (!rejected) {
        m_input_warned = false;
        return true;
    }

    if (!m_input_warned) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuDispatchCore: key '{}' dispatch skipped; its input cannot be fed to the shader (native input rejected or without a receiving binding, or a used image binding has no staged image)",
            dispatch_key());
        m_input_warned = true;
    }

    m_native_staging_bytes.clear();
    m_native_format.reset();
    m_native_rejected = false;
    return false;
}

GpuChannelResult GpuDispatchCore::dispatch_core(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info)
{
    on_before_gpu_dispatch(channels, structure_info);
    if (!inputs_accepted())
        return {};
    prepare_gpu_inputs(channels, structure_info);
    bind_all_descriptors();

    const size_t effective = m_prepared_elements;
    const auto groups = calculate_dispatch_size(effective, structure_info);

    m_resources.dispatch(
        dispatch_key(), groups,
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
    on_before_gpu_dispatch(channels, structure_info);
    if (!inputs_accepted())
        return {};
    prepare_gpu_inputs(channels, structure_info);
    bind_all_descriptors();

    const size_t effective = m_prepared_elements;
    const auto groups = calculate_dispatch_size(effective, structure_info);

    m_resources.dispatch_batched(
        dispatch_key(), groups,
        m_gpu_config.push_constant_size, ctx);

    if (m_resources.has_recording())
        return {};

    GpuChannelResult result;
    result.primary = readback_primary(effective);
    readback_aux(result);
    return result;
}

GpuChannelResult GpuDispatchCore::dispatch_core_chained_indirect(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info,
    const ExecutionContext& ctx)
{
    on_before_gpu_dispatch(channels, structure_info);

    if (!ensure_gpu_ready()) {
        error<std::runtime_error>(Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuDispatchCore: dispatch_core_chained_indirect GPU initialisation failed");
    }

    m_bindings = declare_buffer_bindings();
    if (!inputs_accepted())
        return {};
    prepare_gpu_inputs(channels, structure_info);
    bind_all_descriptors();

    const size_t effective = m_prepared_elements;
    const auto groups = calculate_dispatch_size(effective, structure_info);

    const auto indirect_it = std::ranges::find_if(m_bindings, [](const auto& b) {
        return b.usage_hint == Portal::Graphics::BufferUsageHint::INDIRECT;
    });
    if (indirect_it == m_bindings.end()) {
        error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(),
            "GpuDispatchCore: dispatch_core_chained_indirect requires a binding with usage_hint INDIRECT");
    }

    m_resources.dispatch_batched_indirect(
        dispatch_key(), indirect_it->set, indirect_it->binding, groups,
        m_gpu_config.push_constant_size, ctx);

    if (m_resources.has_recording())
        return {};

    GpuChannelResult result;
    result.primary = readback_primary(effective);
    readback_aux(result);
    return result;
}

Portal::Graphics::FenceID GpuDispatchCore::dispatch_core_async(
    const std::vector<std::vector<double>>& channels,
    const DataStructureInfo& structure_info)
{
    on_before_gpu_dispatch(channels, structure_info);
    if (!inputs_accepted())
        return Portal::Graphics::INVALID_FENCE;
    prepare_gpu_inputs(channels, structure_info);
    bind_all_descriptors();

    const size_t effective = m_prepared_elements;
    const auto groups = calculate_dispatch_size(effective, structure_info);

    m_last_effective_element_count = effective;

    return m_resources.dispatch_async(
        dispatch_key(), groups,
        m_push_constants.empty() ? nullptr : m_push_constants.data(),
        m_push_constants.size());
}

void GpuDispatchCore::for_each_stage(const std::vector<DependencyStage>& stages,
    const std::function<void(const std::array<uint32_t, 3>&, const Portal::Graphics::IndirectDispatch&)>& on_stage)
{
    const GpuComputeConfig original_config = m_gpu_config;
    const auto original_bindings = m_bindings;
    const auto original_image_bindings = m_image_bindings;
    const auto original_binding_data = m_binding_data;
    const auto original_passthrough_bytes = m_passthrough_bytes;
    const auto original_push_constants = m_push_constants;

    for (const auto& stage : stages) {
        m_gpu_config = stage.config;
        update_dispatch_key_cache();
        m_bindings = declare_buffer_bindings();
        m_image_bindings.clear();
        m_binding_data.clear();
        m_passthrough_bytes.clear();
        m_push_constants.clear();

        stage.stage_fn(*this);

        if (!ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "GpuDispatchCore: failed to initialise key '{}' for a dependency stage",
                stage.config.shader_path);
        }

        prepare_gpu_inputs({}, {});

        bind_all_descriptors();

        Portal::Graphics::IndirectDispatch indirect;
        if (stage.indirect_groups) {
            indirect.buffer = m_resources.shared_buffer_handle(stage.indirect_groups->set, stage.indirect_groups->binding).buffer;
            indirect.offset = stage.indirect_groups->offset_bytes;
        }

        on_stage(stage.explicit_groups ? *stage.explicit_groups : calculate_dispatch_size(m_prepared_elements, {}), indirect);
    }

    m_gpu_config = original_config;
    update_dispatch_key_cache();
    m_bindings = original_bindings;
    m_image_bindings = original_image_bindings;
    m_binding_data = original_binding_data;
    m_passthrough_bytes = original_passthrough_bytes;
    m_push_constants = original_push_constants;
}

void GpuDispatchCore::record_stages(const std::vector<DependencyStage>& stages)
{
    for_each_stage(stages, [this](const std::array<uint32_t, 3>& groups, const Portal::Graphics::IndirectDispatch& indirect) {
        m_resources.record_stage(dispatch_key(), groups, m_push_constants, indirect);
    });
}

//==============================================================================
// Readback helpers
//==============================================================================

std::vector<float> GpuDispatchCore::readback_primary(size_t float_count)
{
    const size_t idx = find_first_output_index();

    for (const auto& b : m_bindings) {
        if (b.binding != idx)
            continue;
        if (b.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || b.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED)
            return {};
        break;
    }

    const size_t allocated = m_resources.buffer_allocated_bytes(dispatch_key(), idx);
    const size_t byte_size = std::min(float_count * sizeof(float), allocated);
    std::vector<float> out(byte_size / sizeof(float));
    m_resources.download(dispatch_key(), idx, out.data(), byte_size);
    return out;
}

void GpuDispatchCore::readback_aux(GpuChannelResult& result)
{
    for (const auto& b : m_bindings) {
        if (b.skip_auto_readback)
            continue;

        const size_t idx = b.binding;
        const bool is_image = b.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || b.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED;

        if ((b.direction == GpuBufferBinding::Direction::OUTPUT
                || b.direction == GpuBufferBinding::Direction::INPUT_OUTPUT)
            && !is_image
            && idx < m_output_size_overrides.size()
            && m_output_size_overrides[idx] > 0) {
            const size_t sz = m_output_size_overrides[idx];
            std::vector<uint8_t> raw(sz);
            m_resources.download(dispatch_key(), idx, reinterpret_cast<float*>(raw.data()), sz);
            result.aux[idx] = std::move(raw);
        }
    }
}

void GpuDispatchCore::download_binding(size_t index, void* dest, size_t byte_size)
{
    m_resources.download(dispatch_key(), index, reinterpret_cast<float*>(dest), byte_size);
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

void GpuDispatchCore::flatten_native_variants_to_staging(
    const std::vector<Kakshya::DataVariant>& variants,
    const DataStructureInfo& structure_info)
{
    m_native_staging_bytes.clear();
    m_native_format.reset();
    m_native_rejected = false;

    if (variants.empty() || Kakshya::is_structured_modality(structure_info.modality))
        return;

    std::optional<Kakshya::GpuDataFormat> element;
    size_t total_bytes = 0;
    for (const auto& v : variants) {
        const auto variant_element = std::visit([](const auto& vec) -> std::optional<Kakshya::GpuDataFormat> {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            if constexpr (requires { Kakshya::gpu_data_format_of<T>::value; }) {
                return Kakshya::gpu_data_format_of<T>::value;
            } else {
                return std::nullopt;
            }
        },
            v);

        if (!variant_element || (element && element != variant_element)) {
            m_native_staging_bytes.clear();
            m_native_rejected = true;
            return;
        }
        element = variant_element;

        total_bytes += std::visit([](const auto& vec) {
            return vec.size() * sizeof(typename std::decay_t<decltype(vec)>::value_type);
        },
            v);
    }

    m_native_staging_bytes.reserve(total_bytes);
    for (const auto& v : variants) {
        std::visit([&](const auto& vec) {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            const auto* bytes = reinterpret_cast<const uint8_t*>(vec.data());
            m_native_staging_bytes.insert(
                m_native_staging_bytes.end(),
                bytes,
                bytes + vec.size() * sizeof(T));
        },
            v);
    }
    m_native_format = element;
}

size_t GpuDispatchCore::find_first_output_index() const
{
    size_t first_inout = SIZE_MAX;
    for (const auto& b : m_bindings) {
        if (b.direction == GpuBufferBinding::Direction::OUTPUT)
            return b.binding;
        if (b.direction == GpuBufferBinding::Direction::INPUT_OUTPUT
            && first_inout == SIZE_MAX)
            first_inout = b.binding;
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
    for (const auto& b : m_bindings) {
        if (b.direction == GpuBufferBinding::Direction::OUTPUT)
            continue;
        const size_t idx = b.binding;
        if (idx < m_binding_data.size() && !m_binding_data[idx].empty())
            max_bytes = std::max(max_bytes, m_binding_data[idx].size());
    }
    return max_bytes / sizeof(float);
}

void GpuDispatchCore::update_dispatch_key_cache()
{
    m_cached_dispatch_key = m_gpu_config.shader_path.empty()
        ? std::to_string(m_gpu_config.shader_id)
        : m_gpu_config.shader_path;
}

void GpuDispatchCore::bind_all_descriptors()
{
    m_resources.bind_descriptors_batch(dispatch_key(), m_bindings);
}

} // namespace MayaFlux::Yantra
