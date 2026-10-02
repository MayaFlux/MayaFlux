#pragma once

#include "FrameAccessProcessor.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class FrameSeekProcessor
 * @brief FrameAccessProcessor whose read position follows a function of time.
 *
 * The video counterpart of the time-mapped read in CursorAccessProcessor. With a
 * time map set, each process() call advances a clock by 1 / global fps seconds and
 * reads the frame the map points at, so scrub, freeze, reverse, a tap trailing a
 * live DynamicVideoStream (TimeMaps::lagged with the stream's frame rate) and
 * stutter all work on video. Without a map it behaves exactly like
 * FrameAccessProcessor.
 *
 * The map takes seconds since the last reset() and returns a position in frames.
 * Positions wrap inside the loop region when the container loops, and are held at
 * the first or last frame otherwise.
 */
class MAYAFLUX_API FrameSeekProcessor : public FrameAccessProcessor {
public:
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Drive the read position from a function of time.
     *
     * Passing a different map restarts the clock at zero. Passing the map already
     * set is a no-op.
     *
     * @param map Time map, or null to go back to FrameAccessProcessor behaviour.
     */
    void set_time_map(const std::shared_ptr<const Kinesis::TimeMap>& map);

    /**
     * @brief Restart the time map over and over, with a length that can change.
     *
     * Needs a time map. The length map takes seconds since the last reset() and
     * returns how long, in seconds, the repeat that starts then should last. Each
     * repeat restarts the time map from zero.
     *
     * @param length Repeat length in seconds, or null for no repeating.
     */
    void set_repeat(const std::shared_ptr<const Kinesis::TimeMap>& length);

    /**
     * @brief Mix the two frames either side of a fractional position.
     *
     * Gives smooth slow motion at the cost of ghosting. Applies to 8-bit and
     * 32-bit float formats; other formats read the nearer frame. Off by default.
     */
    void set_blend(bool enable) { m_blend = enable; }

    /**
     * @brief Trail a stream that is being written, by a lag that can change.
     *
     * Sets a time map that follows @p write_head at this processor's frame rate
     * minus the lag, so the rate is the one frames are appended at. The clock
     * restarts at zero, so pass the write head as it is now.
     *
     * @param write_head Index of the next frame to be written.
     * @param lag Distance behind the head in frames, per second since the restart.
     */
    void trail(double write_head, Kinesis::TimeMap lag);

    /**
     * @brief Trail a stream that is being written by a fixed number of frames.
     * @param write_head Index of the next frame to be written.
     * @param lag_frames Distance behind the head in frames.
     */
    void trail(double write_head, double lag_frames);

    /**
     * @brief Restart the clock and the repeat.
     */
    void reset();

private:
    std::shared_ptr<const Kinesis::TimeMap> m_time_map;
    std::shared_ptr<const Kinesis::TimeMap> m_repeat;
    uint64_t m_ticks {};
    uint64_t m_repeat_start {};
    uint64_t m_repeat_length {};
    bool m_blend {};

    double position_at(uint64_t tick);
};

} // namespace MayaFlux::Kakshya
