#include "CursorAccessProcessor.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

CursorAccessProcessor::CursorAccessProcessor(uint64_t frames_per_block)
    : m_frames_per_block(frames_per_block)
{
}

void CursorAccessProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto stream = std::dynamic_pointer_cast<DynamicSoundStream>(container);
    if (!stream) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "CursorAccessProcessor requires a DynamicSoundStream");
        return;
    }

    m_stream = stream;
    m_cursor = 0;
    m_loop_start = 0;
    m_loop_end = stream->get_num_frames();

    const uint64_t n = m_frames_per_block * stream->get_num_channels();
    auto& pd = container->get_processed_data();
    pd.resize(1);
    pd[0] = std::vector<double>(n, 0.0);

    container->mark_ready_for_processing(true);
}

void CursorAccessProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>&)
{
    m_stream.reset();
    m_active = false;
    m_cursor = 0;
}

void CursorAccessProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    m_is_processing.store(true, std::memory_order_relaxed);

    if (!m_active) {
        write_silence(container);
        m_is_processing.store(false, std::memory_order_relaxed);
        return;
    }

    auto stream = m_stream.lock();
    if (!stream) {
        write_silence(container);
        m_is_processing.store(false, std::memory_order_relaxed);
        return;
    }

    const uint64_t num_channels = stream->get_num_channels();
    const uint64_t elements = m_frames_per_block * num_channels;
    const uint64_t loop_end = std::min(m_loop_end, stream->get_num_frames());

    auto& pd = container->get_processed_data();
    pd.resize(1);

    auto& vec = std::get<std::vector<double>>(pd[0]);
    if (vec.size() != elements)
        vec.assign(elements, 0.0);

    stream->peek_sequential(
        std::span<double> { vec.data(), elements },
        elements,
        m_cursor * num_channels);

    m_cursor += m_frames_per_block;

    if (m_cursor >= loop_end) {
        if (m_looping) {
            m_cursor = m_loop_start;
        } else {
            m_active = false;
            m_cursor = m_loop_start;
            if (m_on_end)
                m_on_end();
        }
    }

    container->update_processing_state(ProcessingState::PROCESSED);
    m_is_processing.store(false, std::memory_order_relaxed);
}

void CursorAccessProcessor::reset()
{
    m_cursor = m_loop_start;
    m_active = true;
}

void CursorAccessProcessor::stop()
{
    m_active = false;
}

void CursorAccessProcessor::set_frames_per_block(uint64_t frames_per_block)
{
    m_frames_per_block = frames_per_block;
}

void CursorAccessProcessor::set_loop_region(uint64_t start_frame, uint64_t end_frame)
{
    m_loop_start = start_frame;
    m_loop_end = end_frame;
}

void CursorAccessProcessor::write_silence(const std::shared_ptr<SignalSourceContainer>& container) const
{
    auto stream = m_stream.lock();
    const uint64_t num_channels = stream ? stream->get_num_channels() : 2;
    const uint64_t elements = m_frames_per_block * num_channels;

    auto& pd = container->get_processed_data();
    pd.resize(1);

    auto& vec = std::get<std::vector<double>>(pd[0]);
    vec.assign(elements, 0.0);

    container->update_processing_state(ProcessingState::PROCESSED);
}

} // namespace MayaFlux::Kakshya
