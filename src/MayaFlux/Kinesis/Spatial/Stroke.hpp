#pragma once

namespace MayaFlux::Kinesis {

/**
 * @brief Write a level into an evenly spaced sample array at a fractional
 *        position, filling linearly from the previously written index.
 *
 * @p position in [0, 1] selects the index (0 is the first sample, 1 the
 * last). @p level is clamped to [0, 1]. When @p previous holds a different
 * valid index, every sample between that index and the new one is set on the
 * line from the value last stored at @p previous to @p level, in either
 * direction, so a fast gesture leaves no gaps. @p previous is updated to the
 * index just written; reset it to clear the gesture.
 *
 * A NaN position selects the first sample; infinite positions clamp to the
 * nearest end. An empty array is untouched.
 *
 * @param samples  Samples to write into.
 * @param previous Index written by the previous call of this gesture.
 * @param position Fractional position in [0, 1].
 * @param level    Level in [0, 1].
 */
inline void stroke_samples(
    std::span<float> samples,
    std::optional<size_t>& previous,
    float position,
    float level) noexcept
{
    if (samples.empty())
        return;

    if (std::isnan(position))
        position = 0.F;

    const size_t last = samples.size() - 1;
    const size_t index = static_cast<size_t>(std::clamp(position, 0.F, 1.F) * static_cast<float>(last));
    const float target = std::clamp(level, 0.F, 1.F);

    if (previous && *previous != index && *previous <= last) {
        const size_t from = *previous;
        const float start = samples[from];
        const size_t lo = std::min(from, index);
        const size_t hi = std::max(from, index);
        const auto run = static_cast<float>(hi - lo);

        for (size_t i = lo; i <= hi; ++i) {
            const size_t travelled = i > from ? i - from : from - i;
            const float fraction = static_cast<float>(travelled) / run;
            samples[i] = start + (target - start) * fraction;
        }
    } else {
        samples[index] = target;
    }

    previous = index;
}

} // namespace MayaFlux::Kinesis
