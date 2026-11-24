#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Kriya/BufferOperation.hpp"
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
 * @brief An operation for a BufferPipeline that appends a frame to a ring every cycle.
 * @param ring The ring to feed.
 * @param convert Makes a frame of what the pipeline holds that cycle, or null to skip the cycle.
 * @param cycles_per_second The rate the pipeline's cycles run at, which stamps the frames: the
 *        pipeline's cycle over this rate, in seconds. A pipeline run at the frame rate takes the
 *        registered frame rate.
 * @param token The domain the operation runs in.
 *
 * Chain it with >> after whatever the pipeline captures, and a lane that trails the ring reads what the
 * pipeline recorded, the way a Chimera layer reads a ring. The ring takes its times from the pipeline,
 * so a reflow trailing it should be started with the same cycle rate.
 */
template <typename Frame>
[[nodiscard]] BufferOperation pipeline_frames(
    const std::shared_ptr<Kakshya::FrameRing<Frame>>& ring,
    std::function<std::shared_ptr<const Frame>(const Kakshya::DataVariant&)> convert,
    double cycles_per_second,
    Buffers::ProcessingToken token = Buffers::ProcessingToken::GRAPHICS_BACKEND)
{
    return BufferOperation::dispatch_to(
        [ring, convert = std::move(convert), cycles_per_second](Kakshya::DataVariant& data, uint32_t cycle) {
            if (!ring || !convert || !(cycles_per_second > 0.0)) {
                return;
            }
            if (auto frame = convert(data)) {
                ring->append(std::move(frame), static_cast<double>(cycle) / cycles_per_second);
            }
        },
        token);
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

/**
 * @class RingTrail
 * @brief A lane's trail behind a ring that can be cut to another moment while it runs.
 *
 * Like trailing_frames, it shows the frame some seconds behind the newest one in a ring, and the lag
 * follows a time map of the seconds since the lane entered. cut() moves the trail to a moment
 * behind the newest frame, and the lag carries on from there, so a lag that varies keeps varying
 * about the new moment, as Chimera's cut does. Copies share one trail.
 *
 * Plain values, as a Chimera layer keeps: call cut() from the frame clock, or from where the lane
 * is steered, not from another thread while it advances.
 */
template <typename Frame>
class RingTrail {
public:
    /** @brief A trail behind a ring. A null ring or a lag with no function makes a trail that shows nothing. */
    RingTrail(const std::shared_ptr<Kakshya::FrameRing<Frame>>& ring, Kinesis::TimeMap lag)
        : m_state(std::make_shared<State>(State { .ring = ring, .lag = std::move(lag) }))
    {
    }

    /** @brief The selector for a lane's route. */
    [[nodiscard]] FrameSelector<Frame> selector() const
    {
        return [state = m_state](const FrameContext& context, std::vector<WeightedFrame<Frame>>& into) {
            if (!state->ring || !state->lag.fn || state->ring->head() == 0) {
                return;
            }
            state->elapsed = context.elapsed;
            const double behind = std::max(state->lag(context.elapsed) + state->offset, 0.0);
            if (!std::isfinite(behind)) {
                return;
            }
            if (const auto index = state->ring->locate(state->ring->extent().second - behind)) {
                if (auto frame = state->ring->read(*index)) {
                    into.push_back({ .frame = std::move(frame), .weight = 1.0, .index = static_cast<size_t>(*index) });
                }
            }
        };
    }

    /**
     * @brief Jump to a moment this many seconds behind the newest frame.
     * @param seconds Distance behind the newest frame, zero or more.
     */
    void cut(double seconds)
    {
        if (m_state->lag.fn && std::isfinite(seconds)) {
            m_state->offset = std::max(seconds, 0.0) - m_state->lag(m_state->elapsed);
        }
    }

    /** @brief Seconds behind the newest frame the trail shows now. */
    [[nodiscard]] double lag() const
    {
        return m_state->lag.fn ? std::max(m_state->lag(m_state->elapsed) + m_state->offset, 0.0) : 0.0;
    }

private:
    struct State {
        std::shared_ptr<Kakshya::FrameRing<Frame>> ring;
        Kinesis::TimeMap lag;
        double offset {};
        double elapsed {};
    };

    std::shared_ptr<State> m_state;
};

} // namespace MayaFlux::Kriya
