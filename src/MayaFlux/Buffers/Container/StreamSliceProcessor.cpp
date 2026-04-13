#include "StreamSliceProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

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
    m_num_frames = static_cast<uint32_t>(audio->get_data().size());
}

template <size_t N>
bool StreamSliceProcessor<N>::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

template <size_t N>
void StreamSliceProcessor<N>::load(size_t index, Kakshya::StreamSlice slice)
{
    if (index >= N)
        return;
    slice.cursor = slice.loop_start;
    slice.cursor_remainder = 0.0;
    slice.active = false;
    slice.index = static_cast<uint8_t>(index);
    m_slices[index] = std::move(slice);
}

template <size_t N>
void StreamSliceProcessor<N>::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio)
        return;

    auto& dst = audio->get_data();
    std::ranges::fill(dst, 0.0);

    for (size_t i = 0; i < N; ++i) {
        auto& s = m_slices[i];
        if (!s.active || !s.stream)
            continue;

        const uint64_t num_channels = s.stream->get_num_channels();
        const uint64_t elements = m_num_frames;

        thread_local std::vector<double> read_buf;
        if (read_buf.size() != elements)
            read_buf.assign(elements, 0.0);

        s.stream->peek_sequential(
            std::span<double> { read_buf.data(), elements },
            elements,
            s.cursor * num_channels);

        for (size_t j = 0; j < elements && j < dst.size(); ++j)
            dst[j] += read_buf[j] * s.scale;

        advance(s, i);
    }
}

template <size_t N>
void StreamSliceProcessor<N>::load(
    size_t index,
    std::shared_ptr<Kakshya::DynamicSoundStream> stream)
{
    if (index >= N || !stream)
        return;
    auto& s = m_slices[index];
    s.stream = std::move(stream);
    s.loop_start = 0;
    s.loop_end = s.stream->get_num_frames();
    s.cursor = 0;
    s.cursor_remainder = 0.0;
    s.active = false;
}

template <size_t N>
void StreamSliceProcessor<N>::load(
    size_t index,
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    uint64_t loop_start,
    uint64_t loop_end)
{
    if (index >= N || !stream)
        return;
    auto& s = m_slices[index];
    s.stream = std::move(stream);
    s.loop_start = loop_start;
    s.loop_end = loop_end;
    s.cursor = loop_start;
    s.cursor_remainder = 0.0;
    s.active = false;
}

template <size_t N>
void StreamSliceProcessor<N>::advance(Kakshya::StreamSlice& s, size_t index)
{
    const uint64_t num_channels = s.stream->get_num_channels();
    const double frames_f = static_cast<double>(m_num_frames) / (double)num_channels * s.speed;
    const auto whole_frames = static_cast<uint64_t>(frames_f);
    s.cursor_remainder += frames_f - static_cast<double>(whole_frames);

    const auto carry = static_cast<uint64_t>(s.cursor_remainder);
    s.cursor_remainder -= static_cast<double>(carry);
    s.cursor += whole_frames + carry;

    const uint64_t loop_end = std::min(s.loop_end, s.stream->get_num_frames());

    if (s.cursor >= loop_end) {
        if (s.looping) {
            s.cursor = s.loop_start;
        } else {
            s.active = false;
            s.cursor = s.loop_start;
            s.cursor_remainder = 0.0;
            if (m_on_end)
                m_on_end(index);
        }
    }
}

template <size_t N>
void StreamSliceProcessor<N>::bind(size_t index)
{
    if (index >= N)
        return;
    m_slices[index].cursor = m_slices[index].loop_start;
    m_slices[index].cursor_remainder = 0.0;
    m_slices[index].active = true;
}

template <size_t N>
void StreamSliceProcessor<N>::unbind(size_t index)
{
    if (index >= N)
        return;
    m_slices[index].active = false;
}

template class StreamSliceProcessor<4>;
template class StreamSliceProcessor<8>;

} // namespace MayaFlux::Buffers
