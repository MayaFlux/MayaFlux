#include "FormaBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

FormaBindingsProcessor::FormaBindingsProcessor(const std::string& shader_path)
    : ShaderProcessor(shader_path)
{
}

FormaBindingsProcessor::FormaBindingsProcessor(ShaderConfig config)
    : ShaderProcessor(std::move(config))
{
}

// =============================================================================
// Raw reader overloads
// =============================================================================

void FormaBindingsProcessor::bind_push_constant(
    const std::string& name,
    std::function<float()> reader,
    uint32_t offset,
    size_t size)
{
    auto& b = m_bindings[name];
    b.kind = TargetKind::PUSH_CONSTANT;
    b.reader = std::move(reader);
    b.pc = PushConstantTarget { .offset = offset, .size = size };
    b.desc.reset();
}

void FormaBindingsProcessor::bind_descriptor(
    const std::string& name,
    std::function<float()> reader,
    const std::string& descriptor_name,
    uint32_t binding_index,
    uint32_t set,
    Portal::Graphics::DescriptorRole role)
{
    if (!reader) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBindingsProcessor::bind_descriptor: null reader for '{}'", name);
        return;
    }

    auto& b = m_bindings[name];
    b.kind = TargetKind::DESCRIPTOR;
    b.reader = std::move(reader);
    b.pc.reset();

    auto gpu_buf = make_descriptor_buffer(role);

    b.desc = DescriptorTarget {
        .descriptor_name = descriptor_name,
        .set_index = set,
        .binding_index = binding_index,
        .role = role,
        .gpu_buffer = gpu_buf,
        .buffer_size = sizeof(float),
    };

    bind_buffer(descriptor_name, gpu_buf);
    m_needs_descriptor_rebuild = true;
}

// =============================================================================
// Introspection
// =============================================================================

bool FormaBindingsProcessor::has_binding(const std::string& name) const
{
    return m_bindings.contains(name);
}

std::vector<std::string> FormaBindingsProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [k, _] : m_bindings) {
        names.push_back(k);
    }
    return names;
}

bool FormaBindingsProcessor::unbind(const std::string& name)
{
    return m_bindings.erase(name) > 0;
}

// =============================================================================
// ShaderProcessor hook
// =============================================================================

void FormaBindingsProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    for (const auto& [name, b] : m_bindings) {
        if (!b.reader) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "FormaBindingsProcessor: binding '{}' has no reader", name);
            continue;
        }

        const float value = b.reader();
        switch (b.kind) {
        case TargetKind::PUSH_CONSTANT:
            if (b.pc)
                flush_push_constant(value, *b.pc, buffer);
            break;
        case TargetKind::DESCRIPTOR:
            if (b.desc)
                flush_descriptor(value, *b.desc, buffer);
            break;
        }
    }
}

// =============================================================================
// Private
// =============================================================================

void FormaBindingsProcessor::flush_push_constant(
    float value,
    const PushConstantTarget& pc,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto& staging = buffer->get_pipeline_context().push_constant_staging;
    const size_t end = static_cast<size_t>(pc.offset) + pc.size;

    if (staging.size() < end)
        staging.resize(end);

    if (pc.size == sizeof(float)) {
        std::memcpy(staging.data() + pc.offset, &value, sizeof(float));
    } else if (pc.size == sizeof(double)) {
        auto promoted = static_cast<double>(value);
        std::memcpy(staging.data() + pc.offset, &promoted, sizeof(double));
    }

    MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "FormaBindingsProcessor: PC write offset={} value={}", pc.offset, value);
}

void FormaBindingsProcessor::flush_descriptor(
    float value,
    const DescriptorTarget& desc,
    const std::shared_ptr<VKBuffer>& attached)
{
    upload_to_gpu(&value, sizeof(float), desc.gpu_buffer, nullptr);

    auto& bindings_list = attached->get_pipeline_context().descriptor_buffer_bindings;

    for (auto& entry : bindings_list) {
        if (entry.set == desc.set_index && entry.binding == desc.binding_index) {
            entry.buffer_info.buffer = desc.gpu_buffer->get_buffer();
            entry.buffer_info.offset = 0;
            entry.buffer_info.range = sizeof(float);
            return;
        }
    }

    bindings_list.push_back({
        .set = desc.set_index,
        .binding = desc.binding_index,
        .type = (desc.role == Portal::Graphics::DescriptorRole::UNIFORM)
            ? vk::DescriptorType::eUniformBuffer
            : vk::DescriptorType::eStorageBuffer,
        .buffer_info = vk::DescriptorBufferInfo {
            desc.gpu_buffer->get_buffer(), 0, sizeof(float) },
    });
}

std::shared_ptr<VKBuffer> FormaBindingsProcessor::make_descriptor_buffer(
    Portal::Graphics::DescriptorRole role)
{
    const VKBuffer::Usage usage = (role == Portal::Graphics::DescriptorRole::UNIFORM)
        ? VKBuffer::Usage::UNIFORM
        : VKBuffer::Usage::COMPUTE;

    auto buf = std::make_shared<VKBuffer>(
        sizeof(float),
        usage,
        Kakshya::DataModality::UNKNOWN);

    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();

    if (!svc) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "FormaBindingsProcessor: no BufferService available");
    }

    svc->initialize_buffer(buf);
    return buf;
}

} // namespace MayaFlux::Buffers
