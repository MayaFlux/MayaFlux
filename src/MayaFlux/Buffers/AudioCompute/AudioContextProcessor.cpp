#include "AudioContextProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

namespace MayaFlux::Buffers {

AudioContextProcessor::AudioContextProcessor(const ContextFactory& factory, uint32_t depth, StageFunction stage)
    : m_stage(std::move(stage))
{
    if (!factory) {
        error<std::invalid_argument>(
            Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "AudioContextProcessor: null context factory");
    }

    m_processing_token = ProcessingToken::AUDIO_PARALLEL;

    m_slots.resize(std::max<uint32_t>(depth, 1U));
    for (auto& slot : m_slots) {
        slot.context = factory();
        if (!slot.context) {
            error<std::invalid_argument>(
                Journal::Component::Buffers, Journal::Context::Init,
                std::source_location::current(),
                "AudioContextProcessor: factory returned a null context");
        }
    }
}

AudioContextProcessor::~AudioContextProcessor()
{
    drain();
}

void AudioContextProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioContextProcessor can only attach to an AudioBuffer");
    }
}

void AudioContextProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    drain();
}

bool AudioContextProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

void AudioContextProcessor::drain()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    for (auto& slot : m_slots) {
        if (slot.fence == Portal::Graphics::INVALID_FENCE) {
            continue;
        }
        foundry.wait_for_fence(slot.fence);
        slot.fence = Portal::Graphics::INVALID_FENCE;
    }
}

void AudioContextProcessor::submit(Slot& slot, uint64_t sequence, std::span<const double> samples)
{
    if (m_stage) {
        m_stage(*slot.context, samples, sequence);
    } else {
        slot.context->stage_words(0, samples);
        slot.context->set_output_size(0, samples.size() * sizeof(uint64_t));
    }

    slot.sequence = sequence;
    slot.fence = slot.context->dispatch_async(Yantra::Datum<std::vector<Kakshya::DataVariant>> {});

    if (slot.fence == Portal::Graphics::INVALID_FENCE) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioContextProcessor dispatch {} did not run", sequence);
    }
}

void AudioContextProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AudioContextProcessor received a non-audio buffer");
        return;
    }

    auto& samples = audio->get_data();
    if (samples.empty()) {
        return;
    }

    const uint64_t sequence = m_next++;
    const uint64_t depth = m_slots.size();
    const bool due = sequence >= depth;
    const uint64_t expected = due ? sequence - depth : 0;
    auto& slot = m_slots[sequence % depth];

    std::vector<double> delivered;
    bool busy = false;

    if (slot.fence != Portal::Graphics::INVALID_FENCE) {
        if (Portal::Graphics::get_shader_foundry().is_fence_signaled(slot.fence)) {
            const auto result = slot.context->collect_result();
            slot.fence = Portal::Graphics::INVALID_FENCE;
            if (due && slot.sequence == expected) {
                delivered = slot.context->decode_primary(result);
            }
        } else {
            busy = true;
        }
    }

    if (!busy) {
        submit(slot, sequence, samples);
    }

    if (delivered.empty()) {
        if (due) {
            ++m_missed;
            if (!m_missing) {
                m_missing = true;
                MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "AudioContextProcessor block {} was not complete, output is silent until results arrive in time", expected);
            }
        }
        audio->clear();
        return;
    }

    m_missing = false;

    const size_t copied = std::min(samples.size(), delivered.size());
    std::copy_n(delivered.begin(), copied, samples.begin());
    std::fill(samples.begin() + static_cast<std::ptrdiff_t>(copied), samples.end(), 0.0);
}

} // namespace MayaFlux::Buffers
