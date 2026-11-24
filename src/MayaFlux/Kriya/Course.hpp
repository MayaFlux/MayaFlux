#pragma once

#include "MayaFlux/Kriya/Awaiters/BroadcastAwaiter.hpp"
#include "MayaFlux/Kriya/ReflowHelpers/CourseTime.hpp"
#include "MayaFlux/Transitive/Facet/ControlSnapshot.hpp"
#include "MayaFlux/Transitive/Facet/ControlValue.hpp"
#include "MayaFlux/Transitive/Facet/ControlValueStore.hpp"
#include "MayaFlux/Transitive/Memory/PendingQueue.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Kriya {

/**
 * @enum CourseEventKind
 * @brief What a course announces.
 */
enum class CourseEventKind : uint8_t {
    Finished, ///< The course ended and holds its last time.
    Crossed, ///< A Loop or PingPong course crossed an end. value is the crossings this advance.
    Repeated, ///< A repeat began. value is the number of repeats begun.
    Frame, ///< A new frame index was reported. value is the index.
    Missed, ///< A frame could not be produced. value is the index.
    Marked ///< A marker was passed. value is the marker's id and time its time.
};

/**
 * @struct CourseEvent
 * @brief One announcement from a course.
 */
struct CourseEvent {
    CourseEventKind kind {};
    double time {}; ///< Source seconds when it happened.
    uint64_t value {}; ///< Crossings or frame index, by kind.
};

/**
 * @struct CourseMarker
 * @brief A named moment of source time that announces itself when the course passes it.
 */
struct CourseMarker {
    uint64_t id {}; ///< Stable for the life of the course, and what a Marked event carries.
    std::string name;
    double time {}; ///< Source seconds.
};

/** @brief A stream of a course's events, awaited by one coroutine. */
using CourseEvents = Vruta::BroadcastSource<CourseEvent>;

/**
 * @struct CourseRange
 * @brief The range of source seconds a course lives in, as one value so both ends change together.
 */
struct CourseRange {
    double in {};
    double out {};
};

/**
 * @struct CourseControls
 * @brief What steers a course through time, written from any thread and read once per advance.
 *
 * Numbers are ControlValue and the rest ControlSnapshot, so a write never waits for the
 * advance and the advance never waits for a write. An empty member is left alone when a
 * struct is merged. `edge` is a CourseEdge. `loop_count` is the crossings after which a
 * Loop or PingPong course finishes, 0 for never. `time_map` and `repeat` take a TimeMap,
 * and a TimeMap with no function clears them. `range` is one snapshot because it is the
 * one setting with a rule across its parts, out above in; every other member is valid
 * whatever the others hold. A value that cannot be stepped, such as a speed that is not
 * finite, is held back by the advance as CourseTime holds it back.
 */
struct CourseControls {
    ControlValue speed;
    ControlValue offset;
    ControlValue weight;
    ControlSnapshot<CourseRange> range;
    ControlValue edge;
    ControlValue loop_count;
    ControlSnapshot<Kinesis::TimeMap> time_map;
    ControlSnapshot<Kinesis::TimeMap> repeat;
};

MF_DESCRIBE(CourseControls)

/** @brief The store every course holds its controls in. */
using CourseStore = ControlValueStore<ControlValue>;

namespace Detail {

    /** @brief A queued change to a course, applied at its next advance. */
    struct CourseCommand {
        enum class Kind : uint8_t {
            Start,
            Pause,
            Stop,
            Retrigger,
            Seek,
            SeekClock,
            SeekNormalized,
            Nudge
        };

        Kind kind {};
        double value {};
    };

    /** @brief One subscription: the kind it hears and where it is delivered. */
    struct CourseListener {
        CourseEventKind kind {};
        std::shared_ptr<CourseEvents> source;
    };

} // namespace Detail

/**
 * @class Course
 * @brief One independent traversal of a source in time, and the whole surface that
 *        steers it.
 *
 * A course owns the time of one reading of a source (see CourseTime) and nothing about
 * what is read or where it goes. Whatever drives it, a reflow or a test, calls advance() once
 * per frame and then reads the source time it returns. Everything else is open to any
 * thread at any moment, so a Nexus agent, an expanse or an event handler can steer it
 * while it runs:
 *
 * - Settings (speed, offset, range, edge, time_map, repeat, weight) are the members of
 *   CourseControls. advance() reads each once and builds one setting from them, so a step never
 *   sees the two ends of the range half changed, and no write waits for it. A change takes effect
 *   on the next advance and never rewinds or jumps the clock. The named setters below check
 *   their value and refuse what cannot be used; set() writes what it is given, and the advance
 *   holds back a value that cannot be stepped.
 * - More can be added beside them. extend() holds a struct of the user's own controls, which a
 *   frame operation reads through get() or store() while the course runs, and a member added to
 *   CourseControls reaches the advance in one place, compose().
 * - Commands (start, pause, stop, retrigger, seek, nudge) are queued and applied in order at
 *   the start of the next advance. They return false when not queued, because the value is not
 *   finite or the queue is full.
 * - State (time, progress, finished, paused, crossings, frame, misses) is read at any time.
 * - Events go to every subscriber of their kind. A source holds one waiting coroutine and
 *   one pending value, so a subscriber that is slow loses all but the latest of its kind.
 *
 * Only the driving thread may call advance(), time_at(), report_frame() and report_miss().
 *
 * A course starts running unless made paused. A paused course does not step, but it keeps
 * following seeks and nudges, so a scrub moves what is shown.
 */
class MAYAFLUX_API Course {
public:
    using Controls = CourseControls;

    /**
     * @brief Make a course.
     * @param step_rate Steps per second, such as the frame rate.
     * @param setting Where it starts. Must be usable.
     * @param paused Start without stepping.
     * @return nullptr when the step rate or the setting is unusable.
     */
    [[nodiscard]] static std::shared_ptr<Course> create(
        double step_rate, const CourseSetting& setting, bool paused = false);

    // Controls

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C CourseControls, or a struct added with extend().
     *
     * Members left empty are untouched. Safe from any thread while the course runs.
     */
    template <typename C = CourseControls>
    void set(const C& incoming)
    {
        m_store.merge(incoming);
    }

    /** @brief A copy of the controls of type C as they are now. */
    template <typename C = CourseControls>
    [[nodiscard]] C get() const
    {
        return m_store.snapshot<C>();
    }

    /**
     * @brief Add a struct of the user's own controls beside CourseControls and return it.
     *
     * Belongs before the course is shared with another thread, and before get() is called
     * from one: the advance reads only CourseControls and is not affected.
     */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        return m_store.extend<C>();
    }

    /** @brief The live controls, read without a copy. */
    [[nodiscard]] const CourseControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const CourseStore& store() const noexcept { return m_store; }

    // Settings

    /** @brief Clock seconds per second. Negative runs backwards, zero holds. */
    bool speed(double value);

    /** @brief Seconds added to the clock before the time map. */
    bool offset(double seconds);

    /** @brief The range of source seconds the course lives in. Needs out above in. */
    bool range(double in, double out);

    /** @brief What happens at the ends. @p count is the crossings before Loop or PingPong finish, 0 for never. */
    void edge(CourseEdge edge, uint64_t count = 0);

    /** @brief Source seconds as a function of clock seconds. */
    void time_map(Kinesis::TimeMap map);

    /** @brief Go back to source time that runs with the clock. */
    void clear_time_map();

    /** @brief Restart the time map over and over, each repeat as long as the map says when it starts. */
    void repeat(Kinesis::TimeMap length);

    /** @brief Stop repeating. */
    void clear_repeat();

    /** @brief How much this course counts where several are mixed. Finite and not negative. */
    bool weight(double value);

    /** @brief The setting the next advance will step with, built from the controls now. */
    [[nodiscard]] CourseSetting setting() const;

    // Commands

    /** @brief Run, from where the course is. */
    bool start();

    /** @brief Hold where the course is. Seeks and nudges still move it. */
    bool pause();

    /** @brief Pause and go back to the start. */
    bool stop();

    /** @brief Go back to the start and run, also after finishing. */
    bool retrigger();

    /**
     * @brief Put the course at a source time, as a new pass.
     *
     * Refused at the advance, with a warning, while a time map is set: use seek_clock.
     */
    bool seek(double source_time);

    /** @brief Put the clock at a number of seconds, as a new pass. Works with a time map. */
    bool seek_clock(double clock_seconds);

    /** @brief Put the course at a fraction of its range, as seek does. Not clamped. */
    bool seek_normalized(double fraction);

    /** @brief Move by seconds of source time from where the course is, as seek does. */
    bool nudge(double seconds);

    // Following and markers

    /**
     * @brief Follow another course: this course's source time becomes the other's, plus an offset.
     * @param other The course to follow. Held weakly; when it is gone this course holds where it was.
     * @param offset Source seconds added to the other's time, negative to trail it.
     * @return false for a null course, itself, or an offset that is not finite.
     *
     * Sets the time map, so seek is refused until unfollow() and the range and edge still fold the
     * time. The offset is in source seconds, not clock seconds: the lag is exact while the
     * other course runs straight and means the same place after it loops. Two courses over sources
     * that share a timeline, such as the objects of one Alembic archive, stay in step this way.
     */
    bool follow(const std::shared_ptr<const Course>& other, double offset = 0.0);

    /** @brief Stop following. Source time runs with the clock again, from where the course is. */
    void unfollow();

    /**
     * @brief Add a marker at a source time.
     * @return Its id, or nothing for a time that is not finite.
     *
     * A step that passes the marker announces CourseEventKind::Marked, forward or backward, in a
     * Loop or a PingPong too: a step that crosses one end announces what it passes before and after
     * the end, and a step that crosses more than once announces every marker once. A seek, a restart
     * and a repeat are jumps and announce nothing. A subscriber holds only the latest event, so
     * markers passed in one step reach it as the last of them.
     */
    std::optional<uint64_t> add_marker(std::string name, double source_time);

    /** @brief Remove a marker. false when no marker has the id. */
    bool remove_marker(uint64_t id);

    /** @brief The markers, in the order they were added. */
    [[nodiscard]] std::vector<CourseMarker> markers() const;

    /** @brief Seek to the first marker of this name, as seek does. false for no such marker or when the seek is not queued. */
    bool jump(std::string_view name);

    // State

    /** @brief Source seconds after the last advance. */
    [[nodiscard]] double time() const noexcept { return m_time_cell.load(std::memory_order_relaxed); }

    /** @brief Where the last advance's time sits in the range, 0 to 1. */
    [[nodiscard]] double progress() const noexcept { return m_progress_cell.load(std::memory_order_relaxed); }

    [[nodiscard]] bool finished() const noexcept { return m_finished_cell.load(std::memory_order_relaxed); }

    [[nodiscard]] bool paused() const noexcept { return m_paused_cell.load(std::memory_order_relaxed); }

    /** @brief Edge crossings since the last seek or restart. */
    [[nodiscard]] uint64_t crossings() const noexcept { return m_crossings_cell.load(std::memory_order_relaxed); }

    /** @brief Steps the course has seen. */
    [[nodiscard]] uint64_t steps() const noexcept { return m_steps_cell.load(std::memory_order_relaxed); }

    /** @brief The last frame index reported, if any. */
    [[nodiscard]] std::optional<uint64_t> frame() const noexcept;

    /** @brief Frames reported missing. */
    [[nodiscard]] uint64_t misses() const noexcept { return m_misses_cell.load(std::memory_order_relaxed); }

    /** @brief Steps and commands that could not be applied, such as a seek under a time map. */
    [[nodiscard]] uint64_t refusals() const noexcept { return m_refusals_cell.load(std::memory_order_relaxed); }

    /** @brief How much this course counts: the weight control, and zero where it is not finite or is negative. */
    [[nodiscard]] double weight() const noexcept;

    // Events

    /**
     * @brief A stream of one kind of event from now on, for one waiting coroutine.
     *
     * Each kind has its own stream, so an advance that crosses an edge and finishes tells a
     * subscriber to each. Subscribe once per consumer and kind.
     */
    [[nodiscard]] std::shared_ptr<CourseEvents> subscribe(CourseEventKind kind);

    // Driving thread

    /**
     * @brief Apply the queued commands, step once and publish the state and events.
     *
     * A paused course does not step and returns where it is held. So does an advance with
     * @p advance false, which is how a driver shows where a new course starts before it
     * moves: advance with !advanced(). The finished flag of the step is the course's own.
     * @param advance Whether to step the clock.
     * @return The step, or nullopt when the setting cannot be stepped, which is counted
     * and warned once until it works again.
     */
    [[nodiscard]] std::optional<CourseStep> advance(bool advance = true);

    /** @brief Whether advance() has ever run. Driving thread only. */
    [[nodiscard]] bool advanced() const noexcept { return m_advanced; }

    /**
     * @brief Source time now, as if the offset were greater by an amount.
     *
     * For a part of a source that runs ahead of or behind the course, such as one slot of
     * an asset whose offset comes from a field at its position. Changes nothing.
     */
    [[nodiscard]] std::optional<double> time_at(double extra_offset) const;

    /** @brief Note the frame the course's time selected. Announces a change of index. */
    void report_frame(uint64_t index);

    /** @brief Note that a frame could not be produced. */
    void report_miss(uint64_t index);

private:
    Course(CourseTime time, const CourseSetting& setting, bool paused);

    [[nodiscard]] static CourseControls& open(CourseStore& store);
    [[nodiscard]] CourseSetting compose() const;
    bool push(Detail::CourseCommand::Kind kind, double value = 0.0);
    void apply(const Detail::CourseCommand& command, const CourseSetting& setting);
    void publish(const CourseSetting& setting, const CourseStep& step);
    void announce(CourseEventKind kind, double time, uint64_t value) const;
    void scan_markers(double before, const CourseStep& step, const CourseSetting& setting) const;
    void refuse();

    CourseStore m_store;
    CourseControls& m_controls { open(m_store) };
    Memory::PendingQueue<Detail::CourseCommand, 64> m_commands;
    Memory::SnapshotCell<std::vector<Detail::CourseListener>> m_listeners;
    Memory::SnapshotCell<std::vector<CourseMarker>> m_markers;
    std::atomic<uint64_t> m_next_marker { 1 };

    CourseTime m_clock;
    bool m_paused;
    bool m_was_finished {};
    bool m_refusing {};
    bool m_advanced {};
    bool m_jumped {};
    bool m_last_reversed {};
    uint64_t m_passes_seen {};
    std::optional<uint64_t> m_last_frame;

    std::atomic<double> m_time_cell { 0.0 };
    std::atomic<double> m_progress_cell { 0.0 };
    std::atomic<uint64_t> m_steps_cell { 0 };
    std::atomic<uint64_t> m_crossings_cell { 0 };
    std::atomic<uint64_t> m_frame_cell { 0 };
    std::atomic<uint64_t> m_misses_cell { 0 };
    std::atomic<uint64_t> m_refusals_cell { 0 };
    std::atomic<bool> m_has_frame_cell { false };
    std::atomic<bool> m_finished_cell { false };
    std::atomic<bool> m_paused_cell { false };
};

/**
 * @brief Make several courses follow one, so lanes and reflows over one timeline stay in step.
 * @param leader The course the others follow.
 * @param followers The courses that take its time, as Course::follow does.
 * @param offset Source seconds each follower is set from the leader.
 * @return How many followed. A null, a leader among the followers or an offset that is not finite is left out.
 *
 * Steer the leader, with its speed, its seeks, its markers and its edge, and every follower moves
 * with it, whichever reflow it is in. The followers keep their own range and edge.
 */
MAYAFLUX_API size_t lock_step(const std::shared_ptr<const Course>& leader,
    std::span<const std::shared_ptr<Course>> followers, double offset = 0.0);

} // namespace MayaFlux::Kriya

#include "Course.describe.hpp"
