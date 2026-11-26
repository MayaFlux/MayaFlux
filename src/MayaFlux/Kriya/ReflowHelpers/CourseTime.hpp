#pragma once

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Kriya {

/**
 * @enum CourseEdge
 * @brief What a course does at the ends of its range.
 */
enum class CourseEdge : uint8_t {
    Once, ///< Finish on leaving the range and hold the end it left by.
    Hold, ///< Hold the nearest end and never finish.
    Loop, ///< Wrap from one end to the other.
    PingPong ///< Run back and forth between the ends.
};

/**
 * @struct CourseSetting
 * @brief Everything that steers a course through time, read afresh on every step.
 *
 * The setting is a value. Whoever owns the cells builds one per step, so a change
 * takes effect on the next step and never rewinds or jumps the clock.
 */
struct CourseSetting {
    /** @brief Clock seconds per second, as StreamSlice and Chimera say speed. Negative runs backwards, zero holds. */
    double speed { 1.0 };

    /** @brief Seconds added to the clock before the time map. */
    double offset {};

    double in {}; ///< Start of the range, in source seconds.
    double out {}; ///< End of the range, in source seconds. Must be above in.

    CourseEdge edge { CourseEdge::Once };

    /**
     * @brief Edge crossings after which a Loop or PingPong course finishes.
     *
     * Every crossing of an end counts, whichever way the course is moving. Zero never
     * finishes. Ignored by Once and Hold.
     */
    uint64_t loop_count {};

    /**
     * @brief Source seconds as a function of clock seconds, or null for source time
     * that runs with the clock from where the course started.
     *
     * The clock is scaled by speed first, so unlike StreamSlice, where a time map replaces
     * speed, the map here is applied to the scaled clock. The two agree at speed 1.
     */
    std::shared_ptr<const Kinesis::TimeMap> time_map;

    /**
     * @brief How long, in clock seconds, the repeat that starts now lasts, or null for
     * no repeating. Each repeat restarts the time map from zero.
     */
    std::shared_ptr<const Kinesis::TimeMap> repeat;

    /** @brief Whether a course can step under this setting: finite numbers and a range with length. */
    [[nodiscard]] bool usable() const noexcept
    {
        return std::isfinite(speed) && std::isfinite(offset)
            && std::isfinite(in) && std::isfinite(out) && out > in;
    }
};

/**
 * @struct CourseStep
 * @brief Where a course is after a step.
 */
struct CourseStep {
    double time {}; ///< Source seconds, inside [in, out].
    uint64_t crossings {}; ///< Edge crossings during this step.
    uint64_t passes {}; ///< Repeats begun since the last seek or restart, counting this step's.
    bool reversed {}; ///< A ping-pong pass is running backwards.
    bool finished {}; ///< The course has ended and holds its last time.
};

/**
 * @class CourseTime
 * @brief The time of one course: steps in, source seconds out.
 *
 * Pure arithmetic with no scheduler, buffer, source or file type. It counts whole
 * steps, so the clock is the integer step count over the step rate and never a sum of
 * rounded steps. The clock runs at the setting's speed and is re-based whenever the speed
 * changes, so a change carries on from where the clock is. The step rate is a plain
 * number the caller resolves.
 *
 * Source time is the time map of the clock, or the clock itself offset to where the course
 * started (the start of the range going forwards, its end going backwards, or wherever
 * seek() put it). It is folded into the range by the setting's edge.
 *
 * A step that cannot be computed returns nullopt and changes nothing: a speed, offset or
 * range that is not finite, a range with no length, or a time map or repeat that answers
 * with something that is not finite.
 *
 * Not thread safe. The owner steps and reads it from one thread.
 */
class MAYAFLUX_API CourseTime {
public:
    /**
     * @brief Make a course clock.
     * @param step_rate Steps per second, such as the frame rate.
     * @return nullopt when the rate is not finite and above zero.
     */
    [[nodiscard]] static std::optional<CourseTime> create(double step_rate);

    /**
     * @brief Advance one step.
     *
     * A repeat restarts at the first step at or after its length, and a restart is a jump,
     * not a crossing. A finished course returns its last step again and does not move
     * until seek, seek_clock or restart.
     */
    [[nodiscard]] std::optional<CourseStep> step(const CourseSetting& setting);

    /**
     * @brief Put the course at a source time, as the start of a new pass.
     *
     * Restarts the clock, the repeat and the crossing count and clears finished. Needs
     * a setting with no time map, since a time map cannot be inverted: with one, use seek_clock.
     * @return false when the setting is unusable or has a time map, leaving the course as it was.
     */
    bool seek(double source_time, const CourseSetting& setting);

    /**
     * @brief Put the clock at a number of seconds, as the start of a new pass.
     * @return false when the seconds are not finite.
     */
    bool seek_clock(double clock_seconds);

    /**
     * @brief Make source time that runs with the clock continue from a source time.
     *
     * For a course whose time map is being removed. The clock, the repeat, the crossing
     * count and finished are untouched. A seek, a restart or the next repeat ends it.
     * @return false when the setting is unusable or the time is not finite.
     */
    bool carry_on(double source_time, const CourseSetting& setting);

    /** @brief Back to the start: clock zero, no anchor, no repeat, no crossings, not finished. */
    void restart();

    /**
     * @brief Source time now, as if the offset were greater by an amount.
     *
     * Reads without stepping, so a part of an asset can run ahead of or behind the course
     * (the time map of the clock plus that part's own offset). Changes nothing.
     * @return nullopt when the setting is unusable or the time map answers with something not finite.
     */
    [[nodiscard]] std::optional<double> resolve(const CourseSetting& setting, double extra_offset = 0.0) const;

    /** @brief Steps stepped since construction. Seeks and restarts do not reset it. */
    [[nodiscard]] uint64_t steps() const noexcept { return m_steps; }

    /** @brief The clock now, in seconds. */
    [[nodiscard]] double clock() const noexcept { return clock_at(m_steps); }

    /** @brief Crossings since the last seek or restart. */
    [[nodiscard]] uint64_t crossings() const noexcept { return m_crossings; }

    /** @brief Repeats begun since the last seek or restart. */
    [[nodiscard]] uint64_t passes() const noexcept { return m_passes; }

    [[nodiscard]] bool finished() const noexcept { return m_finished; }

    [[nodiscard]] double step_rate() const noexcept { return 1.0 / m_period; }

private:
    friend class Course;

    explicit CourseTime(double period)
        : m_period(period)
    {
    }

    [[nodiscard]] double clock_at(uint64_t step) const noexcept;

    double m_period;
    uint64_t m_steps {};
    uint64_t m_base_step {};
    double m_base_clock {};
    double m_speed { 1.0 };

    std::optional<double> m_anchor;
    double m_shift {};

    double m_repeat_origin {};
    std::optional<double> m_repeat_length;

    uint64_t m_crossings {};
    uint64_t m_passes {};
    bool m_finished {};
    CourseStep m_last;
};

} // namespace MayaFlux::Kriya
