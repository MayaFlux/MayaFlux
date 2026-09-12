#include "SpatialTransfer.hpp"

#include "FileWriter.hpp"
#include "SpatialExport.hpp"

#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <chrono>

namespace MayaFlux::IO {

SpatialCaptureSource make_network_geometry_source(
    std::string stream_name,
    std::shared_ptr<Buffers::NetworkGeometryBuffer> buffer)
{
    return SpatialCaptureSource {
        .stream_name = std::move(stream_name),
        .capture_frame = [buffer](SpatialCache& cache, const std::string& name) {
            return write_network_geometry_buffer_sample(cache, name, buffer);
        }
    };
}

namespace {
    std::atomic<uint32_t> g_next_spatial_capture_id { 1 };

    bool run_sources(SpatialCache& cache, const std::vector<SpatialCaptureSource>& sources)
    {
        for (const auto& source : sources) {
            if (!source.capture_frame(cache, source.stream_name)) {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialCapture: source '{}' failed", source.stream_name);
                return false;
            }
        }
        return true;
    }
} // namespace

struct SpatialCapture::CaptureState {
    std::shared_ptr<SpatialCache> cache;
    std::vector<SpatialCaptureSource> sources;
    uint32_t max_frames {};
    std::atomic<uint32_t> frame { 0 };
    std::atomic<bool> stop_requested { false };
    std::atomic<bool> recording { false };
};

bool SpatialCapture::run_one_frame(CaptureState& state)
{
    if (state.max_frames != 0 && state.frame.load(std::memory_order_relaxed) >= state.max_frames) {
        return false;
    }

    if (!run_sources(*state.cache, state.sources)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialCapture: failed at frame {}", state.frame.load(std::memory_order_relaxed));
        return false;
    }

    state.frame.fetch_add(1, std::memory_order_relaxed);
    return true;
}

SpatialCapture::SpatialCapture(
    Vruta::TaskScheduler& scheduler,
    std::shared_ptr<SpatialCache> cache,
    std::vector<SpatialCaptureSource> sources)
    : m_scheduler(scheduler)
    , m_cache(std::move(cache))
    , m_sources(std::move(sources))
{
}

SpatialCapture::~SpatialCapture()
{
    stop();
}

bool SpatialCapture::is_recording() const
{
    return m_state && m_state->recording.load(std::memory_order_acquire);
}

uint32_t SpatialCapture::frames_written() const
{
    return m_state ? m_state->frame.load(std::memory_order_relaxed) : 0;
}

void SpatialCapture::start(uint32_t max_frames, uint64_t frame_interval)
{
    if (!m_cache) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialCapture: cannot start, null cache");
        return;
    }

    if (m_sources.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialCapture: cannot start, no sources");
        return;
    }

    stop();

    m_state = std::make_shared<CaptureState>();
    m_state->cache = m_cache;
    m_state->sources = m_sources;
    m_state->max_frames = max_frames;
    m_state->recording.store(true, std::memory_order_release);

    m_task_name = "spatial_capture_"
        + std::to_string(g_next_spatial_capture_id.fetch_add(1, std::memory_order_relaxed));

    auto routine = [](Vruta::TaskScheduler&,
                       std::shared_ptr<CaptureState> state,
                       uint64_t interval) -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate
            && !state->stop_requested.load(std::memory_order_acquire)
            && run_one_frame(*state)) {
            co_await Kriya::FrameDelay { .frames_to_wait = interval };
        }
        state->recording.store(false, std::memory_order_release);
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, m_state, frame_interval)),
        m_task_name, false);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialCapture: recording {} stream(s) every {} frame(s), {}",
        m_sources.size(), frame_interval,
        max_frames == 0 ? std::string("unbounded")
                        : std::format("{} frames", max_frames));
}

void SpatialCapture::stop()
{
    if (!m_state || !m_state->recording.load(std::memory_order_acquire)) {
        return;
    }

    m_state->stop_requested.store(true, std::memory_order_release);
    m_scheduler.cancel_task(m_task_name);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialCapture: stop requested after {} frames",
        m_state->frame.load(std::memory_order_relaxed));
}

bool save_spatial_snapshot(
    const std::string& path_pattern,
    const std::vector<SpatialCaptureSource>& sources)
{
    if (sources.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_spatial_snapshot: no sources");
        return false;
    }

    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
                             .count();
    const auto path = resolve_sequence_path(path_pattern, static_cast<uint64_t>(now_ms));

    SpatialCache cache;
    if (!cache.open(path)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_spatial_snapshot: failed to open '{}': {}", path, cache.get_last_error());
        return false;
    }

    const bool ok = run_sources(cache, sources);
    cache.close();
    return ok;
}

} // namespace MayaFlux::IO
