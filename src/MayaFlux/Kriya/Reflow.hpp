#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kriya/Course.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class Routine;
}

namespace MayaFlux::Kriya {

template <typename Frame>
class ReflowBuilder;

/**
 * @class Reflow
 * @brief Frames of a source brought back through time, one cycle at a time, into a sink.
 *
 * A reflow reads one FrameSource through any number of courses. Each cycle every course
 * advances and places its time among the frames, the frames it picks are read and mixed
 * into one, the frame operations run on that one, and the sink receives the result. A
 * buffer is only ever one frame, so the sink is where a frame becomes a buffer's content,
 * and what the buffer's own processors do afterwards is the sink's concern.
 *
 * The reflow knows no medium and no buffer. The source, the sink and every operation are
 * the caller's, so a volume sequence, a point cache and a set of slot transforms differ
 * only in what they supply.
 *
 * A course that has not advanced before shows where it starts. A cycle with no frame to
 * show (every read failed, or nothing is weighted) writes nothing, keeps what was last
 * written and warns once. A frame is written when it differs from the last one, or every
 * cycle when asked (Rewrite::Always). A transform that answers null, or a condition that
 * answers false, skips the write for that cycle. A callable that throws is logged and
 * skips the cycle.
 *
 * advance() is the whole work of one cycle. start() runs it once per frame on the
 * scheduler's frame clock, and tests or a caller with a clock of its own call it by hand.
 * Courses may be added, removed, steered and subscribed to from any thread.
 *
 * Made by ReflowBuilder. Dropping the last reference stops the routine.
 */
template <typename Frame>
class Reflow : public std::enable_shared_from_this<Reflow<Frame>> {
public:
    using Sink = std::function<void(const std::shared_ptr<const Frame>&, const FrameContext&)>;

    ~Reflow();

    Reflow(const Reflow&) = delete;
    Reflow& operator=(const Reflow&) = delete;
    Reflow(Reflow&&) = delete;
    Reflow& operator=(Reflow&&) = delete;

    /**
     * @brief Start a course over the source.
     * @return The course, or nullptr when the setting is unusable.
     */
    std::shared_ptr<Course> add_course(const CourseSetting& setting, bool paused = false);

    /** @brief Stop a course taking part. It keeps its own state. */
    bool remove_course(const std::shared_ptr<Course>& course);

    /** @brief The courses taking part now. */
    [[nodiscard]] std::vector<std::shared_ptr<Course>> courses() const;

    /** @brief The frame last written: what the sink has been shown, shared and unchanged. */
    [[nodiscard]] std::shared_ptr<const Frame> current() const;

    /**
     * @brief Do one cycle: advance every course, select, mix, operate and write.
     *
     * Only one thread at a time, and the same thread every cycle.
     */
    void advance();

    /** @brief Stop the routine start() began. Safe to call again. */
    void stop();

    /** @brief Cycles done. */
    [[nodiscard]] uint64_t cycle() const noexcept { return m_cycle_cell.load(std::memory_order_relaxed); }

    /** @brief Frames written to the sink. */
    [[nodiscard]] uint64_t writes() const noexcept { return m_writes.load(std::memory_order_relaxed); }

    /** @brief Cycles that wrote nothing because the frame was unchanged or an operation declined. */
    [[nodiscard]] uint64_t skips() const noexcept { return m_skips.load(std::memory_order_relaxed); }

    /** @brief Cycles with no frame to show. */
    [[nodiscard]] uint64_t holds() const noexcept { return m_holds.load(std::memory_order_relaxed); }

    /** @brief Cycles ended early by a callable that threw. */
    [[nodiscard]] uint64_t failures() const noexcept { return m_failures.load(std::memory_order_relaxed); }

    /** @brief Cycles per second the courses count in. */
    [[nodiscard]] double rate() const noexcept { return m_rate; }

    [[nodiscard]] const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source() const noexcept { return m_source; }

private:
    friend class ReflowBuilder<Frame>;

    Reflow(std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink,
        std::vector<FrameOperation<Frame>> operations, FrameSelection selection, Rewrite rewrite, double rate);

    void hold();
    [[nodiscard]] bool operate(const FrameContext& context, std::shared_ptr<const Frame>& frame);

    std::shared_ptr<const Kakshya::FrameSource<Frame>> m_source;
    Sink m_sink;
    std::vector<FrameOperation<Frame>> m_operations;
    std::optional<typename FrameOperation<Frame>::MixFn> m_mix;
    FrameSelection m_selection;
    Rewrite m_rewrite;
    double m_rate;

    Memory::SnapshotCell<std::vector<std::shared_ptr<Course>>> m_courses;
    Memory::SnapshotCell<std::shared_ptr<const Frame>> m_current;

    uint64_t m_cycle {};
    std::shared_ptr<const Frame> m_last_written;
    std::vector<WeightedFrame<Frame>> m_gathered;
    bool m_holding {};

    std::atomic<uint64_t> m_cycle_cell { 0 };
    std::atomic<uint64_t> m_writes { 0 };
    std::atomic<uint64_t> m_skips { 0 };
    std::atomic<uint64_t> m_holds { 0 };
    std::atomic<uint64_t> m_failures { 0 };

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Vruta::Routine> m_routine;
};

/**
 * @class ReflowBuilder
 * @brief Describes a reflow, then builds it or starts it on a scheduler.
 *
 * Defaults: one course over the whole source (Once, or Hold for a single frame), Floor
 * selection, Rewrite::OnChange. Adding a course replaces that default.
 *
 * @code
 * auto smoke = ReflowBuilder<VolumeData>(sequence, write_into_volume)
 *                  .course(setting)
 *                  .operation(FrameOperation<VolumeData>::derive(note_peak))
 *                  .selection(FrameSelection::Blend)
 *                  .start(scheduler);
 * smoke->courses().front()->rate(0.5);
 * @endcode
 */
template <typename Frame>
class ReflowBuilder {
public:
    using Sink = typename Reflow<Frame>::Sink;

    /**
     * @param source The frames to read. Null makes build() return nullptr.
     * @param sink Receives each frame written. Empty makes build() return nullptr.
     * @param rate Cycles per second. Zero takes the registered frame rate.
     */
    ReflowBuilder(std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink, double rate = 0.0);

    /** @brief Add a course. */
    ReflowBuilder& course(const CourseSetting& setting, bool paused = false);

    /** @brief Add an operation, run after the earlier ones. At most one mix. */
    ReflowBuilder& operation(FrameOperation<Frame> operation);

    ReflowBuilder& selection(FrameSelection selection);

    ReflowBuilder& rewrite(Rewrite rewrite);

    /**
     * @brief Build without scheduling, for a caller that calls advance() itself.
     * @return nullptr, with the reason logged, for a missing source or sink, an empty
     * operation, a second mix, an unusable course setting or a source with no frames.
     */
    [[nodiscard]] std::shared_ptr<Reflow<Frame>> build();

    /**
     * @brief Build and run once per frame on the scheduler's frame clock.
     * @return As build().
     */
    [[nodiscard]] std::shared_ptr<Reflow<Frame>> start(const std::shared_ptr<Vruta::TaskScheduler>& scheduler);

private:
    struct Planned {
        CourseSetting setting;
        bool paused {};
    };

    std::shared_ptr<const Kakshya::FrameSource<Frame>> m_source;
    Sink m_sink;
    double m_rate;
    std::vector<Planned> m_courses;
    std::vector<FrameOperation<Frame>> m_operations;
    FrameSelection m_selection { FrameSelection::Floor };
    Rewrite m_rewrite { Rewrite::OnChange };
};

} // namespace MayaFlux::Kriya

#include "MayaFlux/Kriya/ReflowHelpers/Reflow.inl"
