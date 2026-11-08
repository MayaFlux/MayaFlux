#include "AudioComputeProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

namespace {

    constexpr const char* k_block_name = "block";
    constexpr size_t k_word_bytes = sizeof(uint64_t);

    /**
     * @brief Writes one sample as eight little-endian bytes.
     *
     * Byte shifts give the device layout on any host, so no byte-order branch is needed.
     */
    void store_device_word(unsigned char* dst, double sample) noexcept
    {
        const auto bits = std::bit_cast<uint64_t>(sample);
        for (size_t k = 0; k < k_word_bytes; ++k) {
            dst[k] = static_cast<unsigned char>((bits >> (8 * k)) & 0xFFU);
        }
    }

    /**
     * @brief Reads one sample from eight little-endian bytes.
     */
    double load_device_word(const unsigned char* src) noexcept
    {
        uint64_t bits = 0;
        for (size_t k = 0; k < k_word_bytes; ++k) {
            bits |= static_cast<uint64_t>(src[k]) << (8 * k);
        }
        return std::bit_cast<double>(bits);
    }

}

//==============================================================================
// Construction
//==============================================================================

AudioComputeProcessor::AudioComputeProcessor(
    const std::string& shader_path,
    bool native_f64,
    uint32_t workgroup_x)
    : ComputeProcessor(shader_path, workgroup_x)
    , m_native_f64(native_f64)
{
    configure();
}

AudioComputeProcessor::AudioComputeProcessor(
    const Portal::Graphics::ShaderSpec& spec,
    bool native_f64)
    : ComputeProcessor(spec)
    , m_native_f64(native_f64)
{
    configure();
}

void AudioComputeProcessor::configure()
{
    m_processing_token = ProcessingToken::AUDIO_PARALLEL;

    auto& bindings = m_config.bindings;
    for (auto it = bindings.begin(); it != bindings.end();) {
        if (it->second.set == 0 && it->second.binding == 0) {
            it = bindings.erase(it);
        } else {
            ++it;
        }
    }

    add_binding(k_block_name, ShaderBinding(0, 0));
}

//==============================================================================
// Attachment
//==============================================================================

void AudioComputeProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioComputeProcessor can only attach to an AudioBuffer");
        return;
    }

    if (m_native_f64) {
        const auto features = Portal::Graphics::get_shader_foundry()
                                  .get_physical_device()
                                  .getFeatures();
        if (!features.shaderFloat64) {
            error<std::runtime_error>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "NATIVE_F64 requires shaderFloat64 on the selected device");
        }
    }

    if (!m_block) {
        resize_block(static_cast<uint32_t>(audio->get_data().size()));
    }
    m_attached.store(true, std::memory_order_seq_cst);
}

void AudioComputeProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    m_attached.store(false, std::memory_order_seq_cst);
    if (!m_in_flight.load(std::memory_order_seq_cst)) {
        release(buffer);
    }
}

void AudioComputeProcessor::release(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_holds_block.exchange(false, std::memory_order_seq_cst)) {
        return;
    }

    unbind_buffer(k_block_name);
    m_block.reset();
    m_frames = 0;
    ShaderProcessor::on_detach(buffer);
}

bool AudioComputeProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

//==============================================================================
// Block Management
//==============================================================================

void AudioComputeProcessor::resize_block(uint32_t frames)
{
    if (frames == 0) {
        return;
    }

    const size_t bytes = static_cast<size_t>(frames) * k_word_bytes;

    if (!m_block) {
        m_block = std::make_shared<VKBuffer>(
            bytes, VKBuffer::Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);
        bind_buffer(k_block_name, m_block);
        m_holds_block.store(true, std::memory_order_seq_cst);
    } else if (m_block->get_size_bytes() != bytes) {
        m_block->resize(bytes, false);
    }

    m_frames = frames;
}

//==============================================================================
// Processing
//==============================================================================

void AudioComputeProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    m_in_flight.store(true, std::memory_order_seq_cst);
    process_block(buffer);
    m_in_flight.store(false, std::memory_order_seq_cst);

    if (!m_attached.load(std::memory_order_seq_cst)) {
        release(buffer);
    }
}

void AudioComputeProcessor::process_block(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioComputeProcessor received a non-audio buffer");
        return;
    }

    auto& samples = audio->get_data();
    const auto frames = static_cast<uint32_t>(samples.size());

    if (frames == 0) {
        return;
    }

    if (!m_attached.load(std::memory_order_seq_cst)) {
        audio->clear();
        return;
    }

    if (frames != m_frames) {
        resize_block(frames);
    }

    const size_t bytes = static_cast<size_t>(frames) * k_word_bytes;

    auto* words = static_cast<unsigned char*>(m_block->get_buffer_resources().mapped_ptr);
    if (!words) {
        audio->clear();
        return;
    }

    for (size_t i = 0; i < frames; ++i) {
        store_device_word(words + i * k_word_bytes, samples[i]);
    }
    upload_to_gpu(words, bytes, m_block, nullptr, 0, true);

    const auto before = m_last_command_buffer;
    ShaderProcessor::processing_function(m_block);

    if (!is_shader_loaded() || m_last_command_buffer == before) {
        audio->clear();
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioComputeProcessor dispatch did not run, child cleared");
        return;
    }

    const auto& resources = m_block->get_buffer_resources();
    auto* service = Registry::BackendRegistry::instance()
                        .get_service<Registry::Service::BufferService>();
    if (service && service->invalidate_range) {
        service->invalidate_range(resources.memory, 0, bytes);
    }

    const auto* out = static_cast<const unsigned char*>(resources.mapped_ptr);
    for (size_t i = 0; i < frames; ++i) {
        samples[i] = load_device_word(out + i * k_word_bytes);
    }
}

std::array<uint32_t, 3> AudioComputeProcessor::calculate_dispatch_size(const std::shared_ptr<VKBuffer>&)
{
    const uint32_t workgroup = get_dispatch_config().workgroup_x;
    return { (m_frames + workgroup - 1) / workgroup, 1, 1 };
}

} // namespace MayaFlux::Buffers
