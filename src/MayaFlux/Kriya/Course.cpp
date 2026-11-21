#include "Course.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

using Kind = Detail::CourseCommand::Kind;

std::shared_ptr<Course> Course::create(double step_rate, const CourseSetting& setting, bool paused)
{
    auto clock = CourseTime::create(step_rate);
    if (!clock || !setting.usable()) {
        return nullptr;
    }
    return std::shared_ptr<Course>(new Course(*clock, setting, paused));
}

Course::Course(CourseTime time, const CourseSetting& setting, bool paused)
    : m_setting(setting)
    , m_listeners(std::vector<Detail::CourseListener> {})
    , m_clock(time)
    , m_paused(paused)
{
    m_paused_cell.store(paused, std::memory_order_relaxed);
    m_time_cell.store(setting.in, std::memory_order_relaxed);
}

bool Course::speed(double value)
{
    if (!std::isfinite(value)) {
        return false;
    }
    m_setting.update([value](CourseSetting& s) { s.speed = value; });
    return true;
}

bool Course::offset(double seconds)
{
    if (!std::isfinite(seconds)) {
        return false;
    }
    m_setting.update([seconds](CourseSetting& s) { s.offset = seconds; });
    return true;
}

bool Course::range(double in, double out)
{
    if (!std::isfinite(in) || !std::isfinite(out) || !(out > in)) {
        return false;
    }
    m_setting.update([in, out](CourseSetting& s) {
        s.in = in;
        s.out = out;
    });
    return true;
}

void Course::edge(CourseEdge edge, uint64_t count)
{
    m_setting.update([edge, count](CourseSetting& s) {
        s.edge = edge;
        s.loop_count = count;
    });
}

void Course::time_map(Kinesis::TimeMap map)
{
    auto shared = std::make_shared<const Kinesis::TimeMap>(std::move(map));
    m_setting.update([&shared](CourseSetting& s) { s.time_map = shared; });
}

void Course::clear_time_map()
{
    m_setting.update([](CourseSetting& s) { s.time_map.reset(); });
}

void Course::repeat(Kinesis::TimeMap length)
{
    auto shared = std::make_shared<const Kinesis::TimeMap>(std::move(length));
    m_setting.update([&shared](CourseSetting& s) { s.repeat = shared; });
}

void Course::clear_repeat()
{
    m_setting.update([](CourseSetting& s) { s.repeat.reset(); });
}

bool Course::weight(double value)
{
    if (!std::isfinite(value) || value < 0.0) {
        return false;
    }
    m_weight_cell.store(value, std::memory_order_relaxed);
    return true;
}

CourseSetting Course::setting() const
{
    const auto pinned = m_setting.read();
    return pinned ? *pinned : CourseSetting {};
}

bool Course::push(Kind kind, double value)
{
    return m_commands.push(Detail::CourseCommand { .kind = kind, .value = value });
}

bool Course::start() { return push(Kind::Start); }

bool Course::pause() { return push(Kind::Pause); }

bool Course::stop() { return push(Kind::Stop); }

bool Course::retrigger() { return push(Kind::Retrigger); }

bool Course::seek(double source_time)
{
    return std::isfinite(source_time) && push(Kind::Seek, source_time);
}

bool Course::seek_clock(double clock_seconds)
{
    return std::isfinite(clock_seconds) && push(Kind::SeekClock, clock_seconds);
}

bool Course::seek_normalized(double fraction)
{
    return std::isfinite(fraction) && push(Kind::SeekNormalized, fraction);
}

bool Course::nudge(double seconds)
{
    return std::isfinite(seconds) && push(Kind::Nudge, seconds);
}

std::optional<uint64_t> Course::frame() const noexcept
{
    if (!m_has_frame_cell.load(std::memory_order_acquire)) {
        return std::nullopt;
    }
    return m_frame_cell.load(std::memory_order_relaxed);
}

std::shared_ptr<CourseEvents> Course::subscribe(CourseEventKind kind)
{
    auto source = std::make_shared<CourseEvents>();
    m_listeners.update([&](std::vector<Detail::CourseListener>& listeners) {
        listeners.push_back({ .kind = kind, .source = source });
    });
    return source;
}

void Course::announce(CourseEventKind kind, double time, uint64_t value) const
{
    const auto listeners = m_listeners.read();
    if (!listeners) {
        return;
    }
    const CourseEvent event { .kind = kind, .time = time, .value = value };
    for (const auto& listener : *listeners) {
        if (listener.kind == kind) {
            listener.source->signal(event);
        }
    }
}

void Course::refuse()
{
    m_refusals_cell.fetch_add(1, std::memory_order_relaxed);
}

void Course::apply(const Detail::CourseCommand& command, const CourseSetting& setting)
{
    bool applied = true;
    switch (command.kind) {
    case Kind::Start:
        m_paused = false;
        break;
    case Kind::Pause:
        m_paused = true;
        break;
    case Kind::Stop:
        m_paused = true;
        m_clock.restart();
        break;
    case Kind::Retrigger:
        m_clock.restart();
        m_paused = false;
        break;
    case Kind::Seek:
        applied = m_clock.seek(command.value, setting);
        break;
    case Kind::SeekClock:
        applied = m_clock.seek_clock(command.value);
        break;
    case Kind::SeekNormalized:
        applied = m_clock.seek(setting.in + command.value * (setting.out - setting.in), setting);
        break;
    case Kind::Nudge: {
        const auto here = m_clock.resolve(setting);
        applied = here.has_value() && m_clock.seek(*here + command.value, setting);
        break;
    }
    }

    if (!applied) {
        refuse();
        MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Course: a seek was refused; seek by source time needs a setting with no time map, so use seek_clock");
    }
}

void Course::publish(const CourseSetting& setting, const CourseStep& step)
{
    const double length = setting.out - setting.in;
    m_time_cell.store(step.time, std::memory_order_relaxed);
    m_progress_cell.store(std::clamp((step.time - setting.in) / length, 0.0, 1.0), std::memory_order_relaxed);
    m_crossings_cell.store(m_clock.crossings(), std::memory_order_relaxed);
    m_steps_cell.store(m_clock.steps(), std::memory_order_relaxed);
    m_finished_cell.store(m_clock.finished(), std::memory_order_relaxed);
    m_paused_cell.store(m_paused, std::memory_order_relaxed);
}

std::optional<CourseStep> Course::advance(bool advance)
{
    const auto pinned = m_setting.read();
    if (!pinned) {
        return std::nullopt;
    }
    const CourseSetting& setting = *pinned;

    std::optional<CourseStep> step;
    try {
        m_commands.drain([&](const Detail::CourseCommand& command) { apply(command, setting); });

        if (m_paused || !advance) {
            if (const auto held = m_clock.resolve(setting)) {
                step = CourseStep { .time = *held, .passes = m_clock.passes(), .finished = m_clock.finished() };
            }
        } else {
            step = m_clock.step(setting);
        }
    } catch (const std::exception& error) {
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Course: a time map or repeat threw: {}", error.what());
        step.reset();
    } catch (...) {
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Course: a time map or repeat threw");
        step.reset();
    }

    if (!step) {
        if (!m_refusing) {
            MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Course: the setting cannot be stepped; holding the last time until it can");
        }
        m_refusing = true;
        refuse();
        return std::nullopt;
    }
    m_refusing = false;
    m_advanced = true;

    const uint64_t passes_before = m_passes_seen;
    m_passes_seen = step->passes;
    publish(setting, *step);

    if (!m_paused && advance) {
        if (step->crossings > 0) {
            announce(CourseEventKind::Crossed, step->time, step->crossings);
        }
        if (step->passes > passes_before) {
            announce(CourseEventKind::Repeated, step->time, step->passes);
        }
        if (step->finished && !m_was_finished) {
            announce(CourseEventKind::Finished, step->time, 0);
        }
    }
    m_was_finished = step->finished;
    return step;
}

std::optional<double> Course::time_at(double extra_offset) const
{
    const auto pinned = m_setting.read();
    return pinned ? m_clock.resolve(*pinned, extra_offset) : std::nullopt;
}

void Course::report_frame(uint64_t index)
{
    const bool changed = !m_last_frame || *m_last_frame != index;
    m_last_frame = index;
    m_frame_cell.store(index, std::memory_order_relaxed);
    m_has_frame_cell.store(true, std::memory_order_release);
    if (changed) {
        announce(CourseEventKind::Frame, time(), index);
    }
}

void Course::report_miss(uint64_t index)
{
    m_misses_cell.fetch_add(1, std::memory_order_relaxed);
    announce(CourseEventKind::Missed, time(), index);
}

} // namespace MayaFlux::Kriya
