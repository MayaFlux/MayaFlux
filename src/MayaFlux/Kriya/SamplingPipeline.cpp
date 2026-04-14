#include "SamplingPipeline.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Kriya {

SamplingPipeline::SamplingPipeline(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    Buffers::BufferManager& mgr,
    Vruta::TaskScheduler& scheduler,
    uint32_t channel,
    uint32_t buf_size)
    : m_stream(std::move(stream))
    , m_processor(std::make_shared<Buffers::StreamSliceProcessor>())
    , m_mgr(mgr)
    , m_capture(nullptr)
    , m_scheduler(scheduler)
    , m_channel(channel)
    , m_buf_size(buf_size)
{
    m_buffer = std::make_shared<Buffers::AudioBuffer>(channel, buf_size);

    m_buffer->set_default_processor(m_processor);

    m_capture = CaptureBuilder(m_buffer);
    m_capture.on_capture_processing().for_cycles(1);

    m_pipeline = BufferPipeline::create(m_scheduler);
    m_processor->set_on_end([this](size_t) {
        if (!any_active())
            m_pipeline->stop_continuous();
    });
}

SamplingPipeline::~SamplingPipeline()
{
    if (m_built) {
        m_pipeline->stop_continuous();
        m_mgr.remove_supplied_buffer(m_buffer,
            Buffers::ProcessingToken::AUDIO_BACKEND, m_channel);
    }
}

BufferPipeline& SamplingPipeline::pipeline()
{
    return *m_pipeline;
}

CaptureBuilder& SamplingPipeline::capture()
{
    return m_capture;
}

void SamplingPipeline::build()
{
    if (m_built)
        return;

    m_mgr.supply_buffer_to(m_buffer,
        Buffers::ProcessingToken::AUDIO_BACKEND, m_channel, 1.0, true);

    m_pipeline >> static_cast<BufferOperation>(m_capture);

    m_pipeline->execute_buffer_rate(0);

    m_built = true;
}

void SamplingPipeline::play(size_t index)
{
    if (index >= m_processor->slot_count()) {
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "SamplingPipeline::play index {} exceeds slot count {}", index, m_processor->slot_count());
        return;
    }

    m_processor->load(index, m_processor->slice(index));
    m_processor->bind(index);
}

void SamplingPipeline::play(size_t index, Kakshya::StreamSlice slice)
{
    m_processor->load(index, std::move(slice));
    if (!m_built) {
        build();
    }

    m_processor->bind(index);
}

void SamplingPipeline::play_continuous(size_t index)
{
    if (index >= m_processor->slot_count()) {
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "SamplingPipeline::play_continuous index {} exceeds slot count {}", index, m_processor->slot_count());
        return;
    }

    auto sl = m_processor->slice(index);
    sl.looping = true;
    m_processor->load(index, sl);
    m_processor->bind(index);
}

void SamplingPipeline::play_continuous(size_t index, Kakshya::StreamSlice slice)
{
    slice.looping = true;
    m_processor->load(index, std::move(slice));

    if (!m_built) {
        build();
    }

    m_processor->bind(index);
}

void SamplingPipeline::stop(size_t index)
{
    if (index >= m_processor->slot_count()) {
        MF_WARN(Journal::Component::Kriya, Journal::Context::Configuration,
            "SamplingPipeline::stop index {} exceeds slot count {}", index, m_processor->slot_count());
        return;
    }
    m_processor->unbind(index);
}

Kakshya::StreamSlice& SamplingPipeline::slice(size_t index)
{
    return m_processor->slice(index);
}

bool SamplingPipeline::any_active() const
{
    return m_processor->any_active();
}

} // namespace MayaFlux::Kriya
