#include "TapSet.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

Kakshya::StreamSlice& TapSet::tap(size_t index)
{
    const auto& seat = m_seats.at(index);
    return seat.sampler->slice(seat.slot);
}

void TapSet::stop()
{
    if (m_entries) {
        m_entries->cancel();
        m_entries.reset();
    }

    for (const auto& seat : m_seats) {
        seat.sampler->stop(seat.slot);
    }
}

TapSetBuilder::TapSetBuilder(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    Buffers::BufferManager& mgr,
    Vruta::TaskScheduler& scheduler,
    uint32_t buf_size)
    : m_stream(std::move(stream))
    , m_mgr(mgr)
    , m_scheduler(scheduler)
    , m_buf_size(buf_size)
{
}

TapSetBuilder::Spec& TapSetBuilder::current()
{
    if (m_specs.empty()) {
        m_specs.emplace_back();
    }
    return m_specs.back();
}

TapSetBuilder& TapSetBuilder::tap()
{
    m_specs.emplace_back();
    return *this;
}

TapSetBuilder& TapSetBuilder::enters_after(double seconds)
{
    current().delay = std::max(seconds, 0.0);
    return *this;
}

TapSetBuilder& TapSetBuilder::speed(double ratio)
{
    auto& spec = current();
    spec.ratio = std::abs(ratio);
    spec.backward = spec.backward || ratio < 0.0;
    return *this;
}

TapSetBuilder& TapSetBuilder::backward(bool enable)
{
    current().backward = enable;
    return *this;
}

TapSetBuilder& TapSetBuilder::level(double gain)
{
    current().level = gain;
    return *this;
}

TapSetBuilder& TapSetBuilder::on_channel(uint32_t channel)
{
    current().channel = channel;
    return *this;
}

TapSetBuilder& TapSetBuilder::loop(bool enable)
{
    m_looping = enable;
    return *this;
}

TapSetBuilder& TapSetBuilder::region(uint64_t start_frame, uint64_t end_frame)
{
    m_start_frame = start_frame;
    m_end_frame = end_frame;
    return *this;
}

TapSet TapSetBuilder::start()
{
    TapSet taps;

    if (!m_stream) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "TapSetBuilder::start called with a null stream");
        return taps;
    }

    if (m_specs.empty()) {
        m_specs.emplace_back();
    }

    const uint64_t frames = m_stream->get_num_frames();
    const uint64_t end_frame = (m_end_frame == 0 || m_end_frame > frames) ? frames : m_end_frame;

    if (m_start_frame + 1 >= end_frame) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "TapSetBuilder::start region [{}, {}) is too short", m_start_frame, end_frame);
        return taps;
    }

    const auto rate = static_cast<double>(m_stream->get_sample_rate());
    const auto first = static_cast<double>(m_start_frame);
    const auto last = static_cast<double>(end_frame - 1);

    std::vector<uint32_t> channels;
    std::vector<size_t> slot_counts;

    for (const auto& spec : m_specs) {
        auto found = std::ranges::find(channels, spec.channel);
        if (found == channels.end()) {
            auto made = std::make_shared<SamplingPipeline>(
                m_stream, m_mgr, m_scheduler, spec.channel, m_buf_size);
            made->build();
            taps.m_pipelines.push_back(std::move(made));
            channels.push_back(spec.channel);
            slot_counts.push_back(0);
            found = std::prev(channels.end());
        }

        const auto pipeline = static_cast<size_t>(std::distance(channels.begin(), found));
        const auto& sampler = taps.m_pipelines[pipeline];
        const size_t slot = slot_counts[pipeline]++;

        auto slice = sampler->slice_from_range(m_start_frame, end_frame, static_cast<uint8_t>(slot));
        slice.looping = m_looping;
        slice.scale = spec.level;
        slice.with_time_map(Kinesis::TimeMaps::quadratic(
            spec.backward ? last : first,
            (spec.backward ? -spec.ratio : spec.ratio) * rate,
            0.0));
        sampler->load(slot, std::move(slice));

        taps.m_seats.push_back({ .sampler = sampler, .slot = slot });
    }

    std::vector<size_t> order(m_specs.size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::stable_sort(order, {}, [this](size_t i) { return m_specs[i].delay; });

    auto entries = std::make_shared<EventChain>(m_scheduler);
    double elapsed = 0.0;

    for (const size_t i : order) {
        const auto& seat = taps.m_seats[i];
        const double delay = m_specs[i].delay;

        if (delay <= 0.0) {
            seat.sampler->play(seat.slot);
            continue;
        }

        entries->then([sampler = seat.sampler, slot = seat.slot] { sampler->play(slot); },
            delay - elapsed);
        elapsed = delay;
    }

    if (entries->event_count() > 0) {
        entries->start();
        taps.m_entries = std::move(entries);
    }

    return taps;
}

} // namespace MayaFlux::Kriya
