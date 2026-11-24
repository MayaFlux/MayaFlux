#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kriya/Course.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"
#include "MayaFlux/Kriya/ReflowHelpers/ReflowControls.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class Routine;
}

namespace MayaFlux::Kriya {

template <typename Frame>
class ReflowBuilder;

/** @brief What receives a frame a reflow writes. */
template <typename Frame>
using FrameSink = std::function<void(const std::shared_ptr<const Frame>&, const FrameContext&)>;

/**
 * @brief One sink that hands every frame to each of several, in order.
 *
 * For a frame that has to reach several targets, such as the parts of a realized assembly.
 * An empty sink in the list is skipped.
 */
template <typename Frame>
[[nodiscard]] FrameSink<Frame> fan_out(std::vector<FrameSink<Frame>> sinks)
{
    return [sinks = std::move(sinks)](const std::shared_ptr<const Frame>& frame, const FrameContext& context) {
        for (const auto& sink : sinks) {
            if (sink) {
                sink(frame, context);
            }
        }
    };
}

/**
 * @struct CourseRoute
 * @brief A course together with what it reads, what is done to its frames and where they go.
 *
 * A plain course shares the reflow's source, joins the other plain courses in the one mix, and
 * its mixed frame goes through the reflow's operations to the reflow's sink. A route changes
 * any of that for one course:
 *
 * - source or sampled reads another source of the same frames. At most one of them is given.
 * - selector picks the frames itself, from whatever it closed over, and reads no source of the
 *   lane. It excludes source and sampled. The course still steps every cycle for the lane's
 *   clock, weight, pause and events, and a selector lane with no usable course setting runs one
 *   that holds over [0, 1]. A selector that picks nothing counts a miss.
 * - operations run on this course's frame before it goes anywhere. They may transform, derive
 *   or decline, and there is no mix among them: the frames a course picks are reduced by the
 *   reflow's mix, or the heaviest wins.
 * - sink receives this course's frame itself. The course then leaves the shared mix, and the
 *   reflow's operations and sink never see its frames. Left empty, the frame joins the mix.
 * - enters_after delays the first step of the course by this many seconds of the reflow's
 *   cycles. Before then it does not step, read or write.
 * - selection picks among a listed source's frames in its own way, and the reflow's applies when
 *   it is left empty.
 * - every reads and writes once per this many cycles. The course still steps every cycle, so
 *   its time is the same however seldom it is shown.
 */
template <typename Frame>
struct CourseRoute {
    CourseSetting course;
    bool paused {};
    std::shared_ptr<const Kakshya::FrameSource<Frame>> source;
    std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled;
    FrameSelector<Frame> selector;
    std::vector<FrameOperation<Frame>> operations;
    FrameSink<Frame> sink;
    double enters_after {};
    uint32_t every { 1 };
    std::optional<FrameSelection> selection;
};

/**
 * @class Reflow
 * @brief Frames of a source brought back through time, one cycle at a time, into a sink.
 *
 * A reflow reads a FrameSource, or a SampledFrames that answers for any time, through any
 * number of courses. Each cycle every course advances and places its time among the frames,
 * the frames it picks are read and mixed into one, the frame operations run on that one, and
 * the sink receives the result. A buffer is only ever one frame, so the sink is where a frame
 * becomes a buffer's content, and what the buffer's own processors do afterwards is the
 * sink's concern.
 *
 * The selection among frames applies to a listed source. A sampled source answers for the time
 * itself and is never blended.
 *
 * A course added as a CourseRoute may read a source of its own, run operations of its own,
 * enter later, show less often, and write to a sink of its own, so one reflow can drive
 * several targets with independently timed readers of one or several sources.
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
 * What picks the frames is not fixed. A course reads its source through time, and a route's
 * selector may read anything: a position, a feature, weights a Nexus influence writes, a
 * FrameRing a lane trails. Whatever steers the reflow as a whole is in ReflowControls, set
 * through set() like a course's controls, so a Nexus influence can drive it with control_writer.
 *
 * Made by ReflowBuilder. Dropping the last reference stops the routine.
 */
template <typename Frame>
class Reflow : public std::enable_shared_from_this<Reflow<Frame>> {
public:
    using Sink = FrameSink<Frame>;

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

    /**
     * @brief Start a course with a source, operations or sink of its own.
     * @return The course, or nullptr when the setting is unusable, both kinds of source are
     * given, an operation is empty or is a mix, every is zero, enters_after is negative, or
     * what it leaves unsaid (a source, a sink) is not there to be shared.
     */
    std::shared_ptr<Course> add_route(const CourseRoute<Frame>& route);

    /** @brief Stop a course taking part. It keeps its own state. */
    bool remove_course(const std::shared_ptr<Course>& course);

    /**
     * @brief Make a course's lane read a listed source from the next cycle on.
     * @return false, changing nothing, when the course is not one of the reflow's or the source is null or has no frames.
     *
     * Replaces whatever the lane read, a source, a sampled source or a selector. Safe from any
     * thread while the reflow runs: the lane reads its input once per cycle, so a cycle sees the
     * old input or the new one, never a mix. The course keeps its time, so a source of another
     * length is read at the time the course is at.
     */
    bool set_source(const std::shared_ptr<Course>& course, std::shared_ptr<const Kakshya::FrameSource<Frame>> source);

    /** @brief As set_source, for a source that answers for any time. A null source changes nothing. */
    bool set_sampled(const std::shared_ptr<Course>& course, std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled);

    /** @brief As set_source, for a selector. An empty selector changes nothing. */
    bool set_selector(const std::shared_ptr<Course>& course, FrameSelector<Frame> selector);

    /** @brief The courses taking part now. */
    [[nodiscard]] std::vector<std::shared_ptr<Course>> courses() const;

    /** @brief The frame last written: what the sink has been shown, shared and unchanged. */
    [[nodiscard]] std::shared_ptr<const Frame> current() const;

    /**
     * @brief The frame last written by a course's own sink, or null when it has none or has written nothing.
     *
     * Readable from any thread, as a perception of what a lane shows.
     */
    [[nodiscard]] std::shared_ptr<const Frame> current(const std::shared_ptr<Course>& course) const;

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C ReflowControls, or a struct added with extend().
     *
     * Safe from any thread while the reflow runs. Read once per cycle.
     */
    template <typename C = ReflowControls>
    void set(const C& incoming)
    {
        m_store.merge(incoming);
    }

    /** @brief A copy of the controls of type C as they are now. */
    template <typename C = ReflowControls>
    [[nodiscard]] C get() const
    {
        return m_store.template snapshot<C>();
    }

    /**
     * @brief Add a struct of the user's own controls beside ReflowControls and return it.
     *
     * Belongs before the reflow is shared with another thread. A selector or operation reads it
     * through get() or store() while the reflow runs.
     */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        return m_store.template extend<C>();
    }

    /** @brief The live controls, read without a copy. */
    [[nodiscard]] const ReflowControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const ReflowStore& store() const noexcept { return m_store; }

    /**
     * @brief A stream of one kind of event from now on, for one waiting coroutine.
     *
     * A source holds one waiting coroutine and one pending value, so a slow subscriber
     * loses all but the latest of its kind.
     */
    [[nodiscard]] std::shared_ptr<ReflowEvents> subscribe(ReflowEventKind kind);

    /**
     * @brief Do one cycle: advance every course, select, mix, operate and write.
     *
     * Only one thread at a time, and the same thread every cycle. Does nothing while the
     * enabled control is zero.
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

    /** @brief Cycles per second the courses count in. Not the rate of the source's own frames. */
    [[nodiscard]] double cycle_rate() const noexcept { return m_cycle_rate; }

    /** @brief The shared source, or null when the reflow reads a sampled one. */
    [[nodiscard]] const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source() const noexcept { return m_source; }

    /** @brief The shared sampled source, or null when the reflow reads a listed one. */
    [[nodiscard]] const std::shared_ptr<const Kakshya::SampledFrames<Frame>>& sampled() const noexcept { return m_sampled; }

private:
    friend class ReflowBuilder<Frame>;

    struct Input {
        std::shared_ptr<const Kakshya::FrameSource<Frame>> source;
        std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled;
        FrameSelector<Frame> selector;
    };

    struct Action {
        uint64_t cycles {};
        std::function<void(Reflow&)> run;
    };

    struct Lane {
        std::shared_ptr<Course> course;
        Memory::SnapshotCell<Input> input;
        std::vector<FrameOperation<Frame>> operations;
        Sink sink;
        uint64_t enter_cycle {};
        uint32_t every { 1 };
        std::optional<FrameSelection> selection;
        std::shared_ptr<const Frame> last_written;
        Memory::SnapshotCell<std::shared_ptr<const Frame>> shown;

        Lane()
            : input(Input {})
            , shown(std::shared_ptr<const Frame> {})
        {
        }
    };

    struct Listener {
        ReflowEventKind kind {};
        std::shared_ptr<ReflowEvents> source;
    };

    Reflow(std::shared_ptr<const Kakshya::FrameSource<Frame>> source,
        std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled, Sink sink,
        std::vector<FrameOperation<Frame>> operations, FrameSelection selection, Rewrite rewrite, double cycle_rate);

    void hold();
    [[nodiscard]] bool operate(const std::vector<FrameOperation<Frame>>& operations,
        const FrameContext& context, std::shared_ptr<const Frame>& frame);
    [[nodiscard]] bool read_lane(Lane& lane, const FrameContext& context, const CourseStep& step,
        FrameSelection selection, std::vector<WeightedFrame<Frame>>& into);
    void announce(ReflowEventKind kind, uint64_t cycle, uint64_t value) const;
    void run_actions(uint64_t cycle) noexcept;
    bool replace_input(const std::shared_ptr<Course>& course, Input input);
    [[nodiscard]] std::shared_ptr<const Frame> reduce(
        const FrameContext& context, const std::vector<WeightedFrame<Frame>>& frames) const;

    std::shared_ptr<const Kakshya::FrameSource<Frame>> m_source;
    std::shared_ptr<const Kakshya::SampledFrames<Frame>> m_sampled;
    Sink m_sink;
    std::vector<FrameOperation<Frame>> m_operations;
    std::optional<typename FrameOperation<Frame>::MixFn> m_mix;
    FrameSelection m_selection;
    Rewrite m_rewrite;
    double m_cycle_rate;

    std::vector<Action> m_actions;

    ReflowStore m_store;
    ReflowControls& m_controls { m_store.template declare<ReflowControls>() };
    Memory::SnapshotCell<std::vector<Listener>> m_listeners;

    Memory::SnapshotCell<std::vector<std::shared_ptr<Lane>>> m_lanes;
    Memory::SnapshotCell<std::shared_ptr<const Frame>> m_current;

    uint64_t m_cycle {};
    std::shared_ptr<const Frame> m_last_written;
    std::vector<WeightedFrame<Frame>> m_gathered;
    std::vector<WeightedFrame<Frame>> m_lane_frames;
    std::vector<std::shared_ptr<Course>> m_course_scratch;
    bool m_holding {};
    bool m_was_finished {};

    std::atomic<uint64_t> m_cycle_cell { 0 };
    std::atomic<uint64_t> m_writes { 0 };
    std::atomic<uint64_t> m_skips { 0 };
    std::atomic<uint64_t> m_holds { 0 };
    std::atomic<uint64_t> m_failures { 0 };

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Vruta::Routine> m_routine;
};

/**
 * @struct ReflowConfig
 * @brief Everything about a reflow that is not its source, its sink and its scheduler.
 *
 * A description to hand over whole, as ReflowBuilder::configure takes it and as the
 * evolve entry of the API does.
 */
template <typename Frame>
struct ReflowConfig {
    std::vector<CourseSetting> courses; ///< Empty, with no routes, means one course over the whole source.
    std::vector<FrameOperation<Frame>> operations; ///< Run in order. At most one mix.
    FrameSelection selection { FrameSelection::Floor };
    Rewrite rewrite { Rewrite::OnChange };
    double cycle_rate {}; ///< Cycles per second. Zero takes the registered frame rate.
    bool paused {}; ///< Whether the courses start without running.
    std::vector<CourseRoute<Frame>> routes; ///< Courses with a source, operations or sink of their own.
};

/**
 * @class ReflowBuilder
 * @brief Describes a reflow, then builds it or starts it on a scheduler.
 *
 * Defaults: one course over the whole source (Once, or Hold for a single frame), Floor
 * selection, Rewrite::OnChange. Adding a course or a route replaces that default.
 *
 * @code
 * auto smoke = ReflowBuilder<VolumeData>(sequence, write_into_volume)
 *                  .course(setting)
 *                  .operation(FrameOperation<VolumeData>::derive(note_peak))
 *                  .selection(FrameSelection::Blend)
 *                  .start();
 * smoke->courses().front()->speed(0.5);
 * @endcode
 */
template <typename Frame>
class ReflowBuilder {
public:
    using Sink = typename Reflow<Frame>::Sink;

    /**
     * @param source The frames to read. Null makes build() return nullptr, unless every course is a
     *        route with a source of its own.
     * @param sink Receives each frame written. Empty makes build() return nullptr, unless every
     *        course is a route with a sink of its own.
     * @param cycle_rate Cycles per second. Zero takes the registered frame rate.
     * @param scheduler The scheduler start() runs the reflow on. Not needed to build().
     */
    ReflowBuilder(std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink, double cycle_rate = 0.0,
        std::shared_ptr<Vruta::TaskScheduler> scheduler = nullptr);

    /**
     * @brief A builder over a source that answers for any time, such as keyed motion.
     * @param sampled The source. Null makes build() return nullptr.
     * @param sink Receives each frame written. May be empty when every course is a route with a sink.
     */
    [[nodiscard]] static ReflowBuilder sampling(const std::shared_ptr<const Kakshya::SampledFrames<Frame>>& sampled, Sink sink,
        double cycle_rate = 0.0, std::shared_ptr<Vruta::TaskScheduler> scheduler = nullptr);

    /** @brief Add a course. */
    ReflowBuilder& course(const CourseSetting& setting, bool paused = false);

    /** @brief Add a course with a source, operations or sink of its own. */
    ReflowBuilder& route(CourseRoute<Frame> route);

    /**
     * @brief Add the courses and operations of a config and take its selection and rewrite.
     *
     * The cycle_rate of a config is for whoever makes the builder, since the builder's is
     * fixed by then.
     */
    ReflowBuilder& configure(const ReflowConfig<Frame>& config);

    /** @brief Add an operation, run after the earlier ones. At most one mix. */
    ReflowBuilder& operation(FrameOperation<Frame> operation);

    /**
     * @brief Call an action on the running reflow every this many seconds.
     *
     * Runs at the end of a cycle, on the thread that advances, the first time one interval after the
     * start. The action may call any control of the reflow, such as set_source, a course's seek or
     * speed, or set. One that throws is logged and skipped.
     *
     * @param seconds Interval, at least one cycle.
     * @param action Called with the running reflow.
     */
    ReflowBuilder& every(double seconds, std::function<void(Reflow<Frame>&)> action);

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
     * @return As build(), and nullptr when no scheduler was given.
     */
    [[nodiscard]] std::shared_ptr<Reflow<Frame>> start();

private:
    struct Planned {
        CourseSetting setting;
        bool paused {};
    };

    std::shared_ptr<const Kakshya::FrameSource<Frame>> m_source;
    std::shared_ptr<const Kakshya::SampledFrames<Frame>> m_sampled;
    Sink m_sink;
    double m_cycle_rate;
    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::vector<Planned> m_courses;
    std::vector<CourseRoute<Frame>> m_routes;
    std::vector<FrameOperation<Frame>> m_operations;
    FrameSelection m_selection { FrameSelection::Floor };
    Rewrite m_rewrite { Rewrite::OnChange };

    struct PlannedAction {
        double seconds {};
        std::function<void(Reflow<Frame>&)> run;
    };
    std::vector<PlannedAction> m_actions;
};

} // namespace MayaFlux::Kriya

#include "MayaFlux/Kriya/ReflowHelpers/Reflow.inl"
