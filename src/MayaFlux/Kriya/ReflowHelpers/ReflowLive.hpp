#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Kriya {

/**
 * @brief An operation that appends every frame it sees to a ring, stamped with the reflow's time.
 *
 * Put it on a route, or on the reflow, to record what that lane or the reflow writes. The
 * frame is the shared one and is not copied. A ring is also appended to by anything else that
 * calls FrameRing::append, so a reflow can read what another one recorded.
 */
template <typename Frame>
[[nodiscard]] FrameOperation<Frame> recording_frames(const std::shared_ptr<Kakshya::FrameRing<Frame>>& ring)
{
    return FrameOperation<Frame>::derive(
        [ring](const FrameContext& context, const std::shared_ptr<const Frame>& frame) {
            if (ring) {
                ring->append(frame, context.time);
            }
        });
}

/**
 * @brief A selector that shows the frame some seconds behind the newest one in a ring.
 * @param ring The frames to read.
 * @param lag Seconds behind the newest frame, as a function of the seconds since the lane
 *        entered. A negative lag is read as zero, and a lag past the oldest frame picks nothing.
 *
 * A lag that grows plays slower than the ring was written, one that shrinks plays faster, and a
 * lag that holds still follows the head. The lag reads whatever it closed over, so a cell a
 * Nexus influence writes can move it while the lane runs. Echo, delay lines and feedback are
 * a lane that records into the ring and another that trails it.
 */
template <typename Frame>
[[nodiscard]] FrameSelector<Frame> trailing_frames(
    const std::shared_ptr<Kakshya::FrameRing<Frame>>& ring, Kinesis::TimeMap lag)
{
    return [ring, lag = std::move(lag)](const FrameContext& context, std::vector<WeightedFrame<Frame>>& into) {
        if (!ring || !lag.fn || ring->head() == 0) {
            return;
        }
        const double newest = ring->extent().second;
        const double behind = std::max(lag(context.elapsed), 0.0);
        if (!std::isfinite(behind)) {
            return;
        }
        if (const auto index = ring->locate(newest - behind)) {
            if (auto frame = ring->read(*index)) {
                into.push_back({ .frame = std::move(frame), .weight = 1.0, .index = static_cast<size_t>(*index) });
            }
        }
    };
}

} // namespace MayaFlux::Kriya
