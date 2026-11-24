#pragma once

#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Kriya {

/**
 * @brief A time map that holds each frame of a source for a duration of its own.
 * @param times The times of the source's frames.
 * @param count Number of frames.
 * @param durations Seconds to hold each frame, one for every frame and all above zero.
 * @return A map from clock seconds to the source time of the frame being held then: frame 0 from
 *         0 to durations[0], frame 1 from there for durations[1], and so on, the last frame held
 *         past the end. An empty map for durations that do not fit the source.
 *
 * Give it to a course with Course::time_map. The course's range and edge still apply to the times
 * it answers, so a range from the first frame's time to the last and a Loop edge cycle the hold.
 */
[[nodiscard]] inline Kinesis::TimeMap held_frames(
    const Kinesis::TimeSampling& times, size_t count, std::vector<double> durations)
{
    if (count == 0 || durations.size() != count
        || std::ranges::any_of(durations, [](double d) { return !(d > 0.0) || !std::isfinite(d); })) {
        return {};
    }

    std::vector<double> frame_times;
    frame_times.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const auto time = times.time(i);
        if (!time) {
            return {};
        }
        frame_times.push_back(*time);
    }

    std::vector<double> ends(count);
    std::partial_sum(durations.begin(), durations.end(), ends.begin());

    return Kinesis::TimeMap { .fn = [frame_times = std::move(frame_times), ends = std::move(ends)](const double& clock) {
        const auto it = std::ranges::upper_bound(ends, clock);
        const size_t index = it == ends.end() ? ends.size() - 1 : static_cast<size_t>(it - ends.begin());
        return frame_times[index];
    } };
}

} // namespace MayaFlux::Kriya
