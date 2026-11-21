#pragma once

#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Kriya {

/**
 * @struct FramePick
 * @brief One frame index a time picks and its weight.
 */
struct FramePick {
    size_t index {};
    double weight {};
};

/**
 * @struct FramePicks
 * @brief The frames a time picks, none to two.
 */
struct FramePicks {
    std::array<FramePick, 2> items {};
    size_t count {};
};

/**
 * @brief The frames a located time picks, and how much each counts.
 *
 * Floor takes the lower frame and Nearest the closer one (the upper at a fraction of one
 * half or more). Blend takes both, weighted by distance, and only the one when the time
 * is on a frame. A weight that is not above zero or not finite picks nothing.
 */
[[nodiscard]] MAYAFLUX_API FramePicks select_frames(
    const Kinesis::TimeLocation& location, FrameSelection selection, double weight);

/** @brief The index of the frame closest to a located time. */
[[nodiscard]] MAYAFLUX_API size_t nearest_frame(const Kinesis::TimeLocation& location);

} // namespace MayaFlux::Kriya
