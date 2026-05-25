#include "SoundFileWriter.hpp"

#include "AudioEncodeContext.hpp"
#include "FFmpegMuxContext.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/SoundStreamContainer.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
}

#include <chrono>
#include <cstddef>

namespace MayaFlux::IO {

// =========================================================================
// Constructor / destructor
// =========================================================================

SoundFileWriter::SoundFileWriter()
    : m_queue(std::make_unique<Memory::LockFreeQueue<WorkItem, k_queue_capacity>>())
{
}

SoundFileWriter::~SoundFileWriter()
{
    if (m_open.load(std::memory_order_acquire) && !m_closing.exchange(true)) {
        auto fut = close();
        if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "SoundFileWriter destructor timed out; worker detached, file may be incomplete");
            m_worker.detach();
            return;
        }
    }
    if (m_worker.joinable())
        m_worker.join();
}

// =========================================================================
// Lifecycle
// =========================================================================

bool SoundFileWriter::open(const std::string& filepath,
    uint32_t channels,
    uint32_t sample_rate,
    AVCodecID explicit_codec)
{
    if (m_open.load(std::memory_order_acquire)) {
        set_error("open() called while already open");
        return false;
    }

    m_channels = channels;
    m_close_promise = std::promise<bool> {};
    m_closing.store(false, std::memory_order_release);

    m_worker = std::thread(&SoundFileWriter::worker_loop, this,
        filepath, channels, sample_rate, explicit_codec);

    constexpr int k_spin_ms = 500;
    constexpr int k_sleep_us = 500;
    for (int i = 0; i < (k_spin_ms * 1000 / k_sleep_us); ++i) {
        if (m_open.load(std::memory_order_acquire))
            return true;
        std::this_thread::sleep_for(std::chrono::microseconds(k_sleep_us));
    }

    if (m_worker.joinable())
        m_worker.join();
    return false;
}

std::future<bool> SoundFileWriter::close()
{
    if (!m_closing.exchange(true)) {
        post(CloseCmd {});
    }
    return m_close_promise.get_future();
}

// =========================================================================
// Write — interleaved doubles
// =========================================================================

void SoundFileWriter::write(std::span<const double> interleaved, uint32_t num_frames)
{
    if (!m_open.load(std::memory_order_acquire) || interleaved.empty())
        return;

    uint32_t frames = num_frames > 0
        ? num_frames
        : static_cast<uint32_t>(interleaved.size() / (m_channels > 0 ? m_channels : 1));

    post(FrameChunk {
        .samples = std::vector<double>(interleaved.begin(), interleaved.end()),
        .num_frames = frames });
}

void SoundFileWriter::write(const std::vector<Kakshya::DataVariant>& planar)
{
    if (!m_open.load(std::memory_order_acquire) || planar.empty())
        return;

    const auto& ch0 = std::get<std::vector<double>>(planar[0]);
    auto frames = static_cast<uint32_t>(ch0.size());
    if (frames == 0)
        return;

    PlanarChunk chunk;
    chunk.num_frames = frames;
    chunk.channels.reserve(planar.size());
    for (const auto& v : planar)
        chunk.channels.push_back(std::get<std::vector<double>>(v));

    post(std::move(chunk));
}

void SoundFileWriter::write(const std::shared_ptr<Buffers::AudioBuffer>& buffer)
{
    if (!m_open.load(std::memory_order_acquire) || !buffer)
        return;

    const auto& data = buffer->get_data();
    if (data.empty())
        return;

    post(FrameChunk { .samples = data, .num_frames = static_cast<uint32_t>(data.size()) });
}

void SoundFileWriter::write(const std::shared_ptr<Kakshya::SoundStreamContainer>& container)
{
    if (!m_open.load(std::memory_order_acquire) || !container)
        return;

    const auto& data = container->get_data();
    if (data.empty())
        return;

    write(data);
}

// =========================================================================
// Error
// =========================================================================

std::string SoundFileWriter::last_error() const
{
    std::lock_guard lock(m_error_mutex);
    return m_last_error;
}

void SoundFileWriter::set_error(std::string msg)
{
    std::lock_guard lock(m_error_mutex);
    m_last_error = std::move(msg);
}

// =========================================================================
// Internal helpers
// =========================================================================

bool SoundFileWriter::post(const WorkItem& item)
{
    return m_queue->push(item);
}

// =========================================================================
// Worker loop
// =========================================================================

void SoundFileWriter::worker_loop(const std::string& filepath,
    uint32_t channels,
    uint32_t sample_rate,
    AVCodecID codec_id)
{
    FFmpegMuxContext mux;
    AudioEncodeContext enc;

    auto fail = [&](std::string msg) {
        set_error(std::move(msg));
        m_open.store(false, std::memory_order_release);
        m_close_promise.set_value(false);
    };

    if (!mux.open(filepath)) {
        fail(mux.last_error());
        return;
    }

    if (!enc.open(mux, sample_rate, channels, codec_id)) {
        fail(enc.last_error());
        return;
    }

    if (!mux.write_header()) {
        fail(mux.last_error());
        return;
    }

    m_open.store(true, std::memory_order_release);

    bool ok = true;

    while (true) {
        auto item = m_queue->pop();
        if (!item) {
            std::this_thread::yield();
            continue;
        }

        if (std::holds_alternative<FrameChunk>(*item)) {
            auto& fc = std::get<FrameChunk>(*item);
            if (!enc.encode_frames(std::span<const double>(fc.samples), fc.num_frames, mux)) {
                set_error(enc.last_error());
                ok = false;
            }
        } else if (std::holds_alternative<PlanarChunk>(*item)) {
            auto& pc = std::get<PlanarChunk>(*item);
            const auto ch = static_cast<uint32_t>(pc.channels.size());
            std::vector<double> interleaved(static_cast<size_t>(pc.num_frames) * ch);

            for (uint32_t f = 0; f < pc.num_frames; ++f) {
                for (uint32_t c = 0; c < ch; ++c)
                    interleaved[(size_t)f * ch + c] = pc.channels[c][f];
            }

            if (!enc.encode_frames(interleaved, pc.num_frames, mux)) {
                set_error(enc.last_error());
                ok = false;
            }
        } else {
            if (ok && !enc.drain(mux)) {
                set_error(enc.last_error());
                ok = false;
            }
            mux.close();
            m_open.store(false, std::memory_order_release);
            m_close_promise.set_value(ok);
            return;
        }
    }
}

} // namespace MayaFlux::IO
