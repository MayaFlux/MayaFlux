#include "SamplingPipeline.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Kriya {

template <size_t N>
SamplingPipeline<N>::SamplingPipeline(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    Buffers::BufferManager& mgr,
    Vruta::TaskScheduler& scheduler,
    uint32_t channel,
    uint32_t buf_size)
    : m_stream(std::move(stream))
    , m_mgr(mgr)
    , m_capture(nullptr)
    , m_scheduler(scheduler)
    , m_channel(channel)
    , m_buf_size(buf_size)
{
    m_buffer = std::make_shared<Buffers::AudioBuffer>(channel, buf_size);

    m_processor = std::make_shared<Buffers::StreamSliceProcessor<N>>();

    m_buffer->set_default_processor(m_processor);

    for (size_t i = 0; i < N; ++i)
        m_processor->load(i, Kakshya::StreamSlice::from_stream(m_stream, static_cast<uint8_t>(i)));

    m_capture = CaptureBuilder(m_buffer);
    m_capture.on_capture_processing().for_cycles(1);

    m_pipeline = BufferPipeline::create(m_scheduler);
    m_processor->set_on_end([this](size_t) {
        if (!any_active())
            m_pipeline->stop_continuous();
    });
}

template <size_t N>
SamplingPipeline<N>::~SamplingPipeline()
{
    if (m_built) {
        m_pipeline->stop_continuous();
        m_mgr.remove_supplied_buffer(m_buffer,
            Buffers::ProcessingToken::AUDIO_BACKEND, m_channel);
    }
}

template <size_t N>
BufferPipeline& SamplingPipeline<N>::pipeline()
{
    return *m_pipeline;
}

template <size_t N>
CaptureBuilder& SamplingPipeline<N>::capture()
{
    return m_capture;
}

template <size_t N>
void SamplingPipeline<N>::build()
{
    if (m_built)
        return;

    m_mgr.supply_buffer_to(m_buffer,
        Buffers::ProcessingToken::AUDIO_BACKEND, m_channel, 1.0, true);

    m_pipeline >> static_cast<BufferOperation>(m_capture);

    m_pipeline->execute_buffer_rate(0);

    m_built = true;
}

template <size_t N>
void SamplingPipeline<N>::play(size_t index)
{
    if (index >= N)
        return;
    m_processor->bind(index);
}

template <size_t N>
void SamplingPipeline<N>::play(size_t index, Kakshya::StreamSlice slice)
{
    if (index >= N)
        return;

    if (!m_built) {
        m_processor->load(index, std::move(slice));
        build();
    }

    m_processor->bind(index);
}

template <size_t N>
void SamplingPipeline<N>::play_continuous(size_t index)
{
    if (index >= N)
        return;

    auto sl = m_processor->slice(index);
    sl.looping = true;
    m_processor->bind(index);
}

template <size_t N>
void SamplingPipeline<N>::play_continuous(size_t index, Kakshya::StreamSlice slice)
{
    if (index >= N)
        return;

    if (!m_built) {
        m_processor->load(index, std::move(slice));
        build();
    }

    auto sl = m_processor->slice(index);
    sl.looping = true;
    m_processor->bind(index);
}

template <size_t N>
void SamplingPipeline<N>::stop(size_t index)
{
    if (index >= N)
        return;
    m_processor->unbind(index);
}

template <size_t N>
Kakshya::StreamSlice& SamplingPipeline<N>::slice(size_t index)
{
    return m_processor->slice(index);
}

template <size_t N>
bool SamplingPipeline<N>::any_active() const
{
    for (size_t i = 0; i < N; ++i) {
        if (m_processor->slice(i).active)
            return true;
    }
    return false;
}

template class SamplingPipeline<2>;
template class SamplingPipeline<4>;
template class SamplingPipeline<8>;

} // namespace MayaFlux::Kriya
