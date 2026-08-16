#include "ShaderProcessor.hpp"

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// Construction
//==============================================================================

ShaderProcessor::ShaderProcessor(const std::string& shader_path)
    : m_config({ shader_path })
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
    initialize_compute_service();
}

ShaderProcessor::ShaderProcessor(ShaderConfig config)
    : m_config(std::move(config))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
    initialize_compute_service();
}

ShaderProcessor::~ShaderProcessor()
{
    cleanup();
}

//==============================================================================
// BufferProcessor Interface
//==============================================================================

void ShaderProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "ShaderProcessor can only process VKBuffers");
        return;
    }

    if (!m_feeds.empty()) {
        pump_feeds();
    }

    if (m_deferred_submission) {
        resolve_pending_dispatch(false);
        if (is_dispatch_pending()) {
            return;
        }
    }

    if (!on_before_execute(m_last_command_buffer, vk_buffer)) {
        MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "on_before_execute() reported failure, skipping shader execution");
        return;
    }

    if (!m_initialized) {
        initialize_shader();
        initialize_pipeline(vk_buffer);
        m_initialized = true;
    }

    if (m_needs_pipeline_rebuild) {
        initialize_pipeline(vk_buffer);
        m_needs_pipeline_rebuild = false;
        m_needs_descriptor_rebuild = true;
    }

    if (m_needs_descriptor_rebuild) {
        initialize_descriptors(vk_buffer);
        m_needs_descriptor_rebuild = false;
    } else {
        update_descriptors(vk_buffer);
    }

    execute_shader(vk_buffer);
    on_after_execute(m_last_command_buffer, vk_buffer);
}

void ShaderProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    if (m_config.bindings.empty()) {
        auto_bind_buffer(vk_buffer);
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "ShaderProcessor attached to VKBuffer (size: {} bytes, modality: {})",
        vk_buffer->get_size_bytes(),
        static_cast<int>(vk_buffer->get_modality()));
}

void ShaderProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    for (auto it = m_bound_buffers.begin(); it != m_bound_buffers.end();) {
        if (it->second == vk_buffer) {
            it = m_bound_buffers.erase(it);
        } else {
            ++it;
        }
    }
    m_needs_descriptor_rebuild = true;
}

bool ShaderProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
}

//==============================================================================
// Buffer Binding
//==============================================================================

void ShaderProcessor::bind_buffer(const std::string& descriptor_name, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null buffer to descriptor '{}'", descriptor_name);
        return;
    }

    ensure_initialized(buffer);

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        auto user_binding_count = static_cast<uint32_t>(
            std::ranges::count_if(m_config.bindings, [](const auto& pair) {
                return pair.second.set == 1;
            }));

        ShaderBinding default_binding;
        default_binding.set = 1;
        default_binding.binding = user_binding_count;
        default_binding.type = vk::DescriptorType::eStorageBuffer;
        m_config.bindings[descriptor_name] = default_binding;

        MF_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created default binding for '{}': set={}, binding={}",
            descriptor_name, default_binding.set, default_binding.binding);
    }

    m_bound_buffers[descriptor_name] = buffer;
    m_needs_descriptor_rebuild = true;

    MF_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound buffer to descriptor '{}' (size: {} bytes)",
        descriptor_name, buffer->get_size_bytes());
}

void ShaderProcessor::unbind_buffer(const std::string& descriptor_name)
{
    auto it = m_bound_buffers.find(descriptor_name);
    if (it != m_bound_buffers.end()) {
        m_bound_buffers.erase(it);
        m_needs_descriptor_rebuild = true;
    }
}

std::shared_ptr<VKBuffer> ShaderProcessor::get_bound_buffer(const std::string& descriptor_name) const
{
    auto it = m_bound_buffers.find(descriptor_name);
    return it != m_bound_buffers.end() ? it->second : nullptr;
}

void ShaderProcessor::auto_bind_buffer(const std::shared_ptr<VKBuffer>& buffer)
{
    std::string descriptor_name;
    if (m_auto_bind_index == 0) {
        descriptor_name = "input";
    } else if (m_auto_bind_index == 1) {
        descriptor_name = "output";
    } else {
        descriptor_name = "buffer_" + std::to_string(m_auto_bind_index);
    }

    bind_buffer(descriptor_name, buffer);
    m_auto_bind_index++;
}

bool ShaderProcessor::download_bound(
    const std::string& descriptor_name,
    void* data,
    size_t size,
    const std::shared_ptr<VKBuffer>& staging) const
{
    auto buffer = get_bound_buffer(descriptor_name);
    if (!buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "download_bound: no buffer bound to descriptor '{}'", descriptor_name);
        return false;
    }

    download_from_gpu(buffer, data, size, staging);
    return true;
}

//==============================================================================
// Feeds
//==============================================================================

void ShaderProcessor::feed(const std::string& name, FeedSource source)
{
    if (!source) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "feed: null source for '{}'", name);
        return;
    }

    if (m_config.bindings.contains(name)) {
        auto& entry = m_feeds[name];
        entry.source = std::move(source);
        entry.is_storage = true;
        entry.offset = 0;
        entry.size = 0;
        entry.descriptor_name = name;
        entry.buffer.reset();
        entry.mismatch_logged = false;
        return;
    }

    uint32_t offset = 0;
    for (const auto& field : m_config.pc_fields) {
        const size_t width = Kakshya::gpu_data_format_bytes(field.format);
        if (field.name == name) {
            auto& entry = m_feeds[name];
            entry.source = std::move(source);
            entry.is_storage = false;
            entry.offset = offset;
            entry.size = width;
            entry.descriptor_name.clear();
            entry.buffer.reset();
            entry.mismatch_logged = false;
            return;
        }
        offset += static_cast<uint32_t>(width);
    }

    MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "feed: '{}' matches no descriptor and no push constant field. "
        "File-shader processors must supply an explicit offset.",
        name);
}

void ShaderProcessor::feed(const std::string& name, FeedSource source, uint32_t offset, size_t size)
{
    if (!source) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "feed: null source for '{}'", name);
        return;
    }

    if (size != sizeof(float) && size != sizeof(double)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "feed: '{}' requests {} bytes, only 4 or 8 are written", name, size);
        return;
    }

    auto& entry = m_feeds[name];
    entry.source = std::move(source);
    entry.is_storage = false;
    entry.offset = offset;
    entry.size = size;
    entry.descriptor_name.clear();
    entry.buffer.reset();
    entry.mismatch_logged = false;
}

void ShaderProcessor::remove_feed(const std::string& name)
{
    auto it = m_feeds.find(name);
    if (it == m_feeds.end()) {
        return;
    }

    if (it->second.is_storage) {
        unbind_buffer(it->second.descriptor_name);
    }

    m_feeds.erase(it);
}

bool ShaderProcessor::has_feed(const std::string& name) const
{
    return m_feeds.contains(name);
}

std::vector<std::string> ShaderProcessor::get_feed_names() const
{
    std::vector<std::string> names;
    names.reserve(m_feeds.size());
    for (const auto& [name, _] : m_feeds) {
        names.push_back(name);
    }
    return names;
}

void ShaderProcessor::pump_feeds()
{
    for (auto& [name, entry] : m_feeds) {
        FeedValue value = entry.source();

        if (!entry.is_storage) {
            const auto* scalar = std::get_if<double>(&value);
            if (!scalar) {
                if (!entry.mismatch_logged) {
                    MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                        "feed '{}' writes a push constant but returned a DataVariant", name);
                    entry.mismatch_logged = true;
                }
                continue;
            }

            auto& data = get_push_constant_data();
            const size_t required = entry.offset + entry.size;
            if (data.size() < required) {
                data.resize(required);
            }

            if (entry.size == sizeof(float)) {
                const auto narrowed = static_cast<float>(*scalar);
                std::memcpy(data.data() + entry.offset, &narrowed, sizeof(float));
            } else {
                std::memcpy(data.data() + entry.offset, scalar, sizeof(double));
            }
            continue;
        }

        auto* variant = std::get_if<Kakshya::DataVariant>(&value);
        if (!variant) {
            if (!entry.mismatch_logged) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "feed '{}' writes a storage descriptor but returned a double", name);
                entry.mismatch_logged = true;
            }
            continue;
        }

        Kakshya::DataAccess accessor(*variant, {}, Kakshya::DataModality::UNKNOWN);
        auto [ptr, bytes, format] = accessor.gpu_buffer();

        if (!ptr || bytes == 0) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "feed '{}' produced no bytes", name);
            continue;
        }

        if (!entry.buffer) {
            entry.buffer = std::make_shared<VKBuffer>(
                static_cast<size_t>(static_cast<float>(bytes) * 1.5F),
                m_config.bindings.at(entry.descriptor_name).type == vk::DescriptorType::eUniformBuffer
                    ? VKBuffer::Usage::UNIFORM
                    : VKBuffer::Usage::HOST_STORAGE,
                Kakshya::DataModality::UNKNOWN);

            bind_buffer(entry.descriptor_name, entry.buffer);

            MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "feed '{}' backing buffer created at {} bytes",
                name, entry.buffer->get_size_bytes());
        } else if (entry.buffer->get_size_bytes() < bytes) {
            const size_t grown = bytes * 3 / 2;

            MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "feed '{}' backing buffer resized {} to {} bytes",
                name, entry.buffer->get_size_bytes(), grown);

            entry.buffer->resize(grown, false);
            m_needs_descriptor_rebuild = true;
        }

        upload_host_visible(entry.buffer, *variant);
    }
}

//==============================================================================
// Shader Management
//==============================================================================

bool ShaderProcessor::hot_reload_shader()
{
    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Hot-reloading shader: {}", m_config.shader_path);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto new_shader_id = foundry.reload_shader(m_config.shader_path);

    if (new_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Hot-reload failed for shader: {}", m_config.shader_path);
        return false;
    }

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
    }

    m_shader_id = new_shader_id;
    m_needs_pipeline_rebuild = true;
    on_shader_loaded(m_shader_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Shader hot-reloaded successfully (ID: {})", m_shader_id);
    return true;
}

void ShaderProcessor::set_shader(const std::string& shader_path)
{
    m_config.shader_path = shader_path;
    m_needs_pipeline_rebuild = true;
    initialize_shader();
}

//==============================================================================
// Push Constants
//==============================================================================

void ShaderProcessor::set_push_constant_size(size_t size)
{
    m_config.push_constant_size = size;
    m_push_constant_data.resize(size);
    m_needs_pipeline_rebuild = true;
}

void ShaderProcessor::set_push_constant_data_raw(const void* data, size_t size)
{
    if (size > m_config.push_constant_size) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Push constant data size {} exceeds configured size {}",
            size, m_config.push_constant_size);
        return;
    }

    m_push_constant_data.resize(size);
    std::memcpy(m_push_constant_data.data(), data, size);
}

size_t ShaderProcessor::resolve_push_constant_size(const std::shared_ptr<VKBuffer>& buffer) const
{
    size_t size = std::max(m_config.push_constant_size, m_push_constant_data.size());

    for (const auto& entry : buffer->get_pipeline_context().push_constant_bindings) {
        size = std::max(size, static_cast<size_t>(entry.offset) + entry.data.size());
    }

    return size;
}

std::vector<uint8_t> ShaderProcessor::resolve_push_constants(const std::shared_ptr<VKBuffer>& buffer) const
{
    std::vector<uint8_t> merged = m_push_constant_data;
    merged.resize(resolve_push_constant_size(buffer));

    for (const auto& entry : buffer->get_pipeline_context().push_constant_bindings) {
        std::memcpy(merged.data() + entry.offset, entry.data.data(), entry.data.size());
    }

    return merged;
}

//==============================================================================
// Submission
//==============================================================================

void ShaderProcessor::set_deferred_submission(bool deferred)
{
    if (!deferred && is_dispatch_pending()) {
        resolve_pending_dispatch(true);
    }
    m_deferred_submission = deferred;
}

bool ShaderProcessor::resolve_pending_dispatch(bool block)
{
    if (!is_dispatch_pending()) {
        return false;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (block) {
        foundry.wait_for_fence(m_pending_fence);
    } else if (!foundry.is_fence_signaled(m_pending_fence)) {
        return false;
    }

    const auto fence = m_pending_fence;
    auto buffer = m_pending_buffer;

    m_pending_fence = Portal::Graphics::INVALID_FENCE;
    m_pending_buffer.reset();

    on_dispatch_complete(buffer);
    foundry.release_fence(fence);

    return true;
}

void ShaderProcessor::submit_recorded(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (!m_deferred_submission) {
        foundry.submit_and_wait(cmd_id);
        return;
    }

    m_pending_fence = foundry.submit_async(cmd_id);

    if (m_pending_fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Deferred submission failed, no fence returned");
        return;
    }

    m_pending_buffer = buffer;
}

//==============================================================================
// Specialization Constants
//==============================================================================

void ShaderProcessor::set_specialization_constant(uint32_t constant_id, uint32_t value)
{
    m_config.specialization_constants[constant_id] = value;
    m_needs_pipeline_rebuild = true;
}

void ShaderProcessor::clear_specialization_constants()
{
    m_config.specialization_constants.clear();
    m_needs_pipeline_rebuild = true;
}

//==============================================================================
// Configuration
//==============================================================================

void ShaderProcessor::set_config(const ShaderConfig& config)
{
    m_config = config;
    m_needs_pipeline_rebuild = true;
    m_needs_descriptor_rebuild = true;
    initialize_shader();
}

void ShaderProcessor::add_binding(const std::string& descriptor_name, const ShaderBinding& binding)
{
    m_config.bindings[descriptor_name] = binding;
    m_needs_descriptor_rebuild = true;
}

//==========================================================================
// Data movement Queries
//==========================================================================

[[nodiscard]] ShaderProcessor::BufferUsageHint ShaderProcessor::get_buffer_usage_hint(const std::string& descriptor_name) const
{
    if (descriptor_name == "input")
        return BufferUsageHint::INPUT_READ;
    if (descriptor_name == "output")
        return BufferUsageHint::OUTPUT_WRITE;
    return BufferUsageHint::NONE;
}

bool ShaderProcessor::is_in_place_operation(const std::string& descriptor_name) const
{
    auto hint = get_buffer_usage_hint(descriptor_name);
    return hint == BufferUsageHint::BIDIRECTIONAL;
}

bool ShaderProcessor::has_binding(const std::string& descriptor_name) const
{
    return m_config.bindings.find(descriptor_name) != m_config.bindings.end();
}

std::vector<std::string> ShaderProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_config.bindings.size());
    for (const auto& [name, _] : m_config.bindings) {
        names.push_back(name);
    }
    return names;
}

bool ShaderProcessor::are_bindings_complete() const
{
    return std::ranges::all_of(
        m_config.bindings,
        [this](const auto& pair) {
            return m_bound_buffers.find(pair.first) != m_bound_buffers.end();
        });
}

//==============================================================================
// Protected Hooks
//==============================================================================

void ShaderProcessor::on_before_compile(const std::string&) { }
void ShaderProcessor::on_shader_loaded(Portal::Graphics::ShaderID) { }
void ShaderProcessor::on_pipeline_created(Portal::Graphics::ComputePipelineID) { }
void ShaderProcessor::on_before_descriptors_create() { }
void ShaderProcessor::on_descriptors_created() { }
bool ShaderProcessor::on_before_execute(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { return true; }
void ShaderProcessor::on_after_execute(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { }
void ShaderProcessor::on_dispatch_complete(const std::shared_ptr<VKBuffer>&) { }

//==============================================================================
// Private Implementation
//==============================================================================

void ShaderProcessor::initialize_shader()
{
    on_before_compile(m_config.shader_path);

    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (m_config.shader_id != Portal::Graphics::INVALID_SHADER) {
        m_shader_id = m_config.shader_id;
    } else {
        m_shader_id = foundry.load_shader(m_config.shader_path, m_config.stage, m_config.entry_point);
    }

    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to load shader: {}",
            m_config.shader_path.empty() ? "<from spec>" : m_config.shader_path);
        return;
    }

    on_shader_loaded(m_shader_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Shader loaded: {} (ID: {})",
        m_config.shader_path.empty() ? "<from spec>" : m_config.shader_path, m_shader_id);
}

std::optional<uint32_t> ShaderProcessor::resolve_ds_index(uint32_t set) const
{
    if (m_engine_owns_set_zero) {
        if (set == 0)
            return std::nullopt;
        const uint32_t idx = set - 1;
        if (idx >= m_descriptor_set_ids.size())
            return std::nullopt;
        return idx;
    }
    if (set >= m_descriptor_set_ids.size())
        return std::nullopt;
    return set;
}

void ShaderProcessor::update_descriptors(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;

    std::set<std::pair<uint32_t, uint32_t>> updated_pairs;

    for (const auto& binding : descriptor_bindings) {
        auto ds_index = resolve_ds_index(binding.set);
        if (!ds_index) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Descriptor set index {} out of range or reserved", binding.set);
            continue;
        }

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[*ds_index],
            binding.binding,
            binding.type,
            binding.buffer_info.buffer,
            binding.buffer_info.offset,
            binding.buffer_info.range);

        updated_pairs.emplace(binding.set, binding.binding);
    }

    for (const auto& [descriptor_name, buf] : m_bound_buffers) {
        auto binding_it = m_config.bindings.find(descriptor_name);
        if (binding_it == m_config.bindings.end()) {
            continue;
        }

        const auto& binding = binding_it->second;
        auto key = std::make_pair(binding.set, binding.binding);

        if (updated_pairs.count(key)) {
            continue;
        }

        auto ds_index = resolve_ds_index(binding.set);
        if (!ds_index) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Invalid descriptor set index {} for binding '{}'",
                binding.set, descriptor_name);
            continue;
        }

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[*ds_index],
            binding.binding,
            binding.type,
            buf->get_buffer(),
            0,
            buf->get_size_bytes());
    }
}

void ShaderProcessor::cleanup()
{
    resolve_pending_dispatch(true);
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    m_descriptor_set_ids.clear();
    m_bound_buffers.clear();
    m_feeds.clear();
    m_initialized = false;
}

} // namespace MayaFlux::Buffers
