#include "StreamSliceProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kakshya/Utils/ContainerUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

template <size_t N>
void StreamSliceProcessor<N>::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "StreamSliceProcessor requires an AudioBuffer");
        return;
    }
    m_frames_per_block = static_cast<uint32_t>(audio->get_num_samples());
}

template <size_t N>
void StreamSliceProcessor<N>::on_detach(const std::shared_ptr<Buffer>&)
{
    for (size_t i = 0; i < N; ++i)
        detach_slot(i);
}

template <size_t N>
bool StreamSliceProcessor<N>::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

template <size_t N>
void StreamSliceProcessor<N>::detach_slot(size_t index)
{
    auto& slot = m_slots[index];
    if (slot.proc && slot.slice.stream)
        slot.proc->on_detach(slot.slice.stream);
    slot = {};
}

template <size_t N>
void StreamSliceProcessor<N>::load(size_t index, Kakshya::StreamSlice slice)
{
    if (m_frames_per_block == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "StreamSliceProcessor::load called before attachment to a buffer -- attach first");
        return;
    }

    if (index >= N || !slice.stream)
        return;

    detach_slot(index);

    slice.active = false;
    slice.index = static_cast<uint8_t>(index);

    auto proc = std::make_shared<Kakshya::CursorAccessProcessor>(m_frames_per_block);
    proc->set_loop_region(slice.start_frame(), slice.end_frame());
    proc->set_looping(slice.looping);

    if (slice.speed != 1.0)
        proc->set_speed(slice.speed);

    proc->set_on_end([this, index] {
        m_slots[index].slice.active = false;
        if (m_on_end)
            m_on_end(index);
    });

    proc->on_attach(slice.stream);

    m_slots[index].slice = std::move(slice);
    m_slots[index].proc = std::move(proc);
}

template <size_t N>
void StreamSliceProcessor<N>::bind(size_t index)
{
    if (index >= N || !m_slots[index].proc)
        return;
    m_slots[index].proc->reset();
    m_slots[index].slice.active = true;
}

template <size_t N>
void StreamSliceProcessor<N>::unbind(size_t index)
{
    if (index >= N || !m_slots[index].proc)
        return;
    m_slots[index].proc->stop();
    m_slots[index].slice.active = false;
}

template <size_t N>
void StreamSliceProcessor<N>::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio)
        return;

    const uint32_t ch = audio->get_channel_id();
    auto& dst = audio->get_data();
    std::ranges::fill(dst, 0.0);

    for (size_t i = 0; i < N; ++i) {
        auto& slot = m_slots[i];
        if (!slot.slice.active || !slot.slice.stream || !slot.proc)
            continue;

        slot.proc->process(slot.slice.stream);

        const auto& pd = slot.slice.stream->get_dynamic_data(slot.proc->get_slot_index());
        if (pd.empty())
            continue;

        const auto structure = slot.slice.stream->get_structure();
        Kakshya::extract_processed_data(
            pd, structure.organization, structure.get_channel_count(), ch, dst);
    }
}

template class StreamSliceProcessor<4>;
template class StreamSliceProcessor<8>;

} // namespace MayaFlux::Buffers
