#include "TapSet.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

void TapSet::level(size_t tap, double gain)
{
    for (const auto& seat : m_taps.at(tap).seats) {
        seat.sampler->slice(seat.slot).scale = gain;
    }
}

void TapSet::speed(size_t tap, double ratio)
{
    const auto& target = m_taps.at(tap);
    *target.velocity = ratio * target.rate;
}

Kakshya::StreamSlice& TapSet::slice(size_t tap, size_t output)
{
    const auto& seat = m_taps.at(tap).seats.at(output);
    return seat.sampler->slice(seat.slot);
}

void TapSet::stop()
{
    if (m_entries) {
        m_entries->cancel();
        m_entries.reset();
    }

    for (const auto& tap : m_taps) {
        for (const auto& seat : tap.seats) {
            seat.sampler->stop(seat.slot);
        }
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
    current().channels = { channel };
    return *this;
}

TapSetBuilder& TapSetBuilder::on_channels(const std::vector<uint32_t>& channels)
{
    if (!channels.empty()) {
        current().channels = channels;
    }
    return *this;
}

TapSetBuilder& TapSetBuilder::from_channel(uint32_t channel)
{
    current().source = channel;
    return *this;
}

TapSetBuilder& TapSetBuilder::lag(double seconds)
{
    return lag(Kinesis::constant<double, double>(seconds));
}

TapSetBuilder& TapSetBuilder::lag(Kinesis::TimeMap seconds)
{
    current().lag = std::make_shared<const Kinesis::TimeMap>(std::move(seconds));
    return *this;
}

TapSetBuilder& TapSetBuilder::repeat_every(double seconds)
{
    return repeat_every(Kinesis::constant<double, double>(seconds));
}

TapSetBuilder& TapSetBuilder::repeat_every(Kinesis::TimeMap seconds)
{
    current().repeat = std::make_shared<const Kinesis::TimeMap>(std::move(seconds));
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
    const uint32_t source_count = std::max<uint32_t>(m_stream->get_num_channels(), 1);
    const double shortest = 2.0 * static_cast<double>(m_buf_size) / rate;

    std::vector<uint32_t> channels;
    std::vector<size_t> slot_counts;

    for (const auto& spec : m_specs) {
        TapSet::Tap built;
        built.rate = rate;
        built.velocity = std::make_shared<double>((spec.backward ? -spec.ratio : spec.ratio) * rate);

        Kinesis::TimeMap lag_frames;
        if (spec.lag) {
            lag_frames = Kinesis::TimeMap { .fn = [lag = spec.lag, rate, shortest](const double& t) -> double {
                return std::max((*lag)(t), shortest) * rate;
            } };
        }
        const double origin = static_cast<double>(m_stream->get_write_head(0)) + rate * spec.delay;

        for (const uint32_t output : spec.channels) {
            auto found = std::ranges::find(channels, output);
            if (found == channels.end()) {
                auto made = std::make_shared<SamplingPipeline>(
                    m_stream, m_mgr, m_scheduler, output, m_buf_size);
                made->build();
                taps.m_pipelines.push_back(std::move(made));
                channels.push_back(output);
                slot_counts.push_back(0);
                found = std::prev(channels.end());
            }

            const auto pipeline = static_cast<size_t>(std::distance(channels.begin(), found));
            const auto& sampler = taps.m_pipelines[pipeline];
            const size_t slot = slot_counts[pipeline]++;

            auto slice = sampler->slice_from_range(m_start_frame, end_frame, static_cast<uint8_t>(slot));
            slice.looping = m_looping;
            slice.scale = spec.level;
            slice.source_channel = spec.source.value_or(output % source_count);
            if (!spec.lag) {
                slice.repeat = spec.repeat;
            }
            slice.with_time_map(spec.lag
                    ? Kinesis::TimeMaps::lagged(origin, rate, lag_frames)
                    : Kinesis::TimeMaps::integrated(spec.backward ? last : first, built.velocity));
            sampler->load(slot, std::move(slice));

            built.seats.push_back({ .sampler = sampler, .slot = slot });
        }

        taps.m_taps.push_back(std::move(built));
    }

    std::vector<size_t> order(m_specs.size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::stable_sort(order, {}, [this](size_t i) { return m_specs[i].delay; });

    auto entries = std::make_shared<EventChain>(m_scheduler);
    double elapsed = 0.0;

    for (const size_t i : order) {
        const auto& seats = taps.m_taps[i].seats;
        const double delay = m_specs[i].delay;

        if (delay <= 0.0) {
            for (const auto& seat : seats) {
                seat.sampler->play(seat.slot);
            }
            continue;
        }

        entries->then([seats] {
            for (const auto& seat : seats) {
                seat.sampler->play(seat.slot);
            }
        },
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
