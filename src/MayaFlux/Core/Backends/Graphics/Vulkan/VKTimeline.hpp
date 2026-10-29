#pragma once

namespace MayaFlux::Core {

/// Upper bound on distinct queues: graphics, compute and transfer.
inline constexpr uint32_t k_max_timelines = 3;

/**
 * @struct TimelinePoint
 * @brief Completion token of one submission: a value on one queue's timeline
 *
 * A value of 0 means no submission (a failed submit or nothing pending).
 */
struct TimelinePoint {
    uint32_t timeline {};
    uint64_t value {};

    [[nodiscard]] explicit operator bool() const { return value != 0; }
};

/**
 * @struct TimelineSnapshot
 * @brief Values on every timeline that must complete, indexed by timeline
 *
 * A zero entry means no dependency on that timeline. A single point converts
 * implicitly, so APIs that take a snapshot also accept one point.
 */
struct TimelineSnapshot {
    std::array<uint64_t, k_max_timelines> values {};

    TimelineSnapshot() = default;

    TimelineSnapshot(TimelinePoint point)
    {
        if (point.timeline < k_max_timelines) {
            values[point.timeline] = point.value;
        }
    }
};

} // namespace MayaFlux::Core
