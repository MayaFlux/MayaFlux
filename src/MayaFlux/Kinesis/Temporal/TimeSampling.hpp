#pragma once

namespace MayaFlux::Kinesis {

/**
 * @enum OutOfRange
 * @brief What a sampler answers for a time outside the times it holds.
 *
 * Absent answers nothing and leaves the choice of a rest value to the caller.
 * RepeatOffset repeats a cumulative quantity so each copy continues from where
 * the last ended, and behaves as Repeat for values that cannot be added.
 */
enum class OutOfRange : uint8_t {
    Absent,
    Hold,
    Repeat,
    PingPong,
    Extrapolate,
    RepeatOffset
};

/**
 * @enum TimeWrap
 * @brief How fold_time maps a time onto a closed range.
 */
enum class TimeWrap : uint8_t {
    Clamp,
    Repeat,
    PingPong
};

/**
 * @struct FoldedTime
 * @brief A time folded into [begin, end].
 *
 * cycles counts the whole ranges passed (negative before begin), saturating at
 * the int64 limits for times beyond that. reversed is true while a ping-pong
 * pass runs backwards, which is every odd cycle.
 */
struct FoldedTime {
    double time {};
    int64_t cycles {};
    bool reversed {};
};

/**
 * @brief Fold a time into the closed range [begin, end].
 *
 * Clamp holds the nearest end. Repeat wraps with modulo semantics. PingPong
 * runs forward over even cycles and backward over odd ones. A range of zero
 * length folds every time to begin.
 *
 * @return nullopt when any argument is not finite or end is below begin.
 */
[[nodiscard]] MAYAFLUX_API std::optional<FoldedTime> fold_time(
    double t, double begin, double end, TimeWrap mode);

/**
 * @enum TimePlacement
 * @brief Where a time falls against the first and last sample time.
 */
enum class TimePlacement : uint8_t {
    Before,
    Within,
    After
};

/**
 * @struct TimeLocation
 * @brief The two samples around a time and how far between them it lies.
 *
 * A time on a sample, or on the last one, gives lower == upper and a fraction
 * of 0. A time outside the samples gives the nearest end for both indices, a
 * fraction of 0 and a placement of Before or After. Otherwise fraction is in
 * [0, 1], reaching 1 only when rounding carries a time just under the upper
 * sample onto it.
 */
struct TimeLocation {
    size_t lower {};
    size_t upper {};
    double fraction {};
    TimePlacement placement { TimePlacement::Within };
};

/**
 * @struct RationalRate
 * @brief Samples per second as an exact fraction, such as 24000 over 1001.
 */
struct RationalRate {
    uint32_t numerator {};
    uint32_t denominator {};

    [[nodiscard]] bool operator==(const RationalRate&) const noexcept = default;
};

/**
 * @class TimeSampling
 * @brief The time of every sample of an ordered sequence, in seconds.
 *
 * Time is a plain number. This class does not know ticks, frame rates in force
 * elsewhere, or any file. Whoever reads a source resolves its time unit to
 * seconds before building one.
 *
 * Three shapes, the same three Alembic uses:
 * - Uniform: one sample per step. Built from a double step and start, or from an
 *   exact RationalRate and a first frame number. A rate computes each time from
 *   its index with one rounding, so 24000 over 1001 never drifts, and frame
 *   24000 * k lands on exactly 1001 * k.
 * - Cyclic: a fixed number of unevenly spaced samples per cycle, repeating every
 *   time_per_cycle.
 * - Acyclic: an explicit strictly increasing list of times.
 *
 * Times never decrease with the index, which is what makes lookup a binary
 * search. A sampling carries no sample count and no duration. The owner of the
 * samples supplies the count, as Alembic does.
 */
class MAYAFLUX_API TimeSampling {
public:
    enum class Kind : uint8_t {
        Uniform,
        Cyclic,
        Acyclic
    };

    /**
     * @brief Uniform sampling by a step and a start time.
     * @return nullopt unless step is finite and above zero, start is finite, and
     * start + step differs from start.
     */
    [[nodiscard]] static std::optional<TimeSampling> uniform(double step, double start = 0.0);

    /**
     * @brief Uniform sampling by an exact rate. Sample i is frame first_frame + i.
     * @return nullopt when the numerator or denominator is zero. The fraction is
     * stored reduced.
     */
    [[nodiscard]] static std::optional<TimeSampling> uniform_rate(RationalRate rate, int64_t first_frame = 0);

    /**
     * @brief Cyclic sampling: times[k] is the offset of sample k within each cycle.
     * @return nullopt unless there are at least two finite, strictly increasing
     * times, the cycle is finite and above zero, and the last time falls before
     * the first plus the cycle.
     */
    [[nodiscard]] static std::optional<TimeSampling> cyclic(std::vector<double> times, double time_per_cycle);

    /**
     * @brief Acyclic sampling by explicit times.
     * @return nullopt unless the list is not empty and finite and strictly increasing.
     */
    [[nodiscard]] static std::optional<TimeSampling> acyclic(std::vector<double> times);

    /**
     * @brief Build from the three numbers Alembic stores: samples per cycle, time
     *        per cycle and the stored times.
     *
     * One sample per cycle is uniform from times[0], more is cyclic. The count of
     * times must equal the samples per cycle.
     */
    [[nodiscard]] static std::optional<TimeSampling> from_cycles(
        uint32_t samples_per_cycle, double time_per_cycle, std::vector<double> times);

    [[nodiscard]] Kind kind() const noexcept { return m_kind; }

    /** @brief The exact rate of a uniform sampling built from one. */
    [[nodiscard]] std::optional<RationalRate> exact_rate() const noexcept { return m_rate; }

    /** @brief Seconds between samples of a uniform sampling, 0 for the other kinds. */
    [[nodiscard]] double step() const noexcept { return m_kind == Kind::Uniform ? m_step : 0.0; }

    /** @brief Time of the first sample of a uniform sampling, 0 for the other kinds. */
    [[nodiscard]] double start() const noexcept { return m_kind == Kind::Uniform ? m_start : 0.0; }

    /** @brief Length of a cycle for a cyclic sampling, 0 for the other kinds. */
    [[nodiscard]] double time_per_cycle() const noexcept { return m_kind == Kind::Cyclic ? m_period : 0.0; }

    /** @brief The stored times: one cycle for cyclic, all of them for acyclic, none for uniform. */
    [[nodiscard]] const std::vector<double>& stored_times() const noexcept { return m_times; }

    /** @brief Samples an acyclic sampling holds. The other kinds have no limit. */
    [[nodiscard]] std::optional<size_t> stored_count() const noexcept;

    /**
     * @brief Time of sample index.
     * @return nullopt past the end of an acyclic list, or where an exact rate
     * would overflow its 64-bit arithmetic.
     */
    [[nodiscard]] std::optional<double> time(size_t index) const noexcept;

    /**
     * @brief Place a time among the first count samples.
     * @return nullopt for a count of 0, a time that is not finite, a count past
     * the end of an acyclic list, or a sampling that cannot produce the last time.
     */
    [[nodiscard]] std::optional<TimeLocation> locate(double t, size_t count) const noexcept;

    /** @brief Largest index whose time is at or below t, or 0 before the first sample. */
    [[nodiscard]] std::optional<size_t> floor_index(double t, size_t count) const noexcept;

    /** @brief Smallest index whose time is at or above t, or the last index after the last sample. */
    [[nodiscard]] std::optional<size_t> ceil_index(double t, size_t count) const noexcept;

    /** @brief Index of the nearest sample time, the lower one on a tie. */
    [[nodiscard]] std::optional<size_t> nearest_index(double t, size_t count) const noexcept;

    [[nodiscard]] bool operator==(const TimeSampling&) const noexcept = default;

private:
    TimeSampling() = default;

    [[nodiscard]] std::optional<double> rational_time(size_t index) const noexcept;

    Kind m_kind { Kind::Uniform };
    double m_start {};
    double m_step {};
    std::optional<RationalRate> m_rate;
    int64_t m_first_frame {};
    std::vector<double> m_times;
    double m_period {};
};

}
