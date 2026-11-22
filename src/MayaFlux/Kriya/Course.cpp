#include "Course.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

using Kind = Detail::CourseCommand::Kind;

namespace {

    double number(const ControlValue& slot, double fallback) noexcept
    {
        return slot.has_value() ? slot.value() : fallback;
    }

    std::shared_ptr<const Kinesis::TimeMap> mapped(const ControlSnapshot<Kinesis::TimeMap>& slot)
    {
        auto map = slot.value();
        return map && map->fn ? map : nullptr;
    }

} // namespace

std::shared_ptr<Course> Course::create(double step_rate, const CourseSetting& setting, bool paused)
{
    auto clock = CourseTime::create(step_rate);
    if (!clock || !setting.usable()) {
        return nullptr;
    }
    return std::shared_ptr<Course>(new Course(*clock, setting, paused));
}

CourseControls& Course::open(CourseStore& store)
{
    return store.declare<CourseControls>();
}

Course::Course(CourseTime time, const CourseSetting& setting, bool paused)
    : m_listeners(std::vector<Detail::CourseListener> {})
    , m_clock(time)
    , m_paused(paused)
{
    m_controls.speed.observe(setting.speed);
    m_controls.offset.observe(setting.offset);
    m_controls.weight.observe(1.0);
    m_controls.edge.observe(setting.edge);
    m_controls.loop_count.observe(static_cast<double>(setting.loop_count));
    m_controls.range.set(CourseRange { .in = setting.in, .out = setting.out });
    if (setting.time_map) {
        m_controls.time_map.set(*setting.time_map);
    }
    if (setting.repeat) {
        m_controls.repeat.set(*setting.repeat);
    }

    m_paused_cell.store(paused, std::memory_order_relaxed);
    m_time_cell.store(setting.in, std::memory_order_relaxed);
}

CourseSetting Course::compose() const
{
    CourseSetting setting;
    setting.speed = number(m_controls.speed, setting.speed);
    setting.offset = number(m_controls.offset, setting.offset);
    if (const auto range = m_controls.range.value()) {
        setting.in = range->in;
        setting.out = range->out;
    }
    if (m_controls.edge.has_value()) {
        setting.edge = m_controls.edge.value<CourseEdge>();
    }
    const double count = number(m_controls.loop_count, 0.0);
    setting.loop_count = std::isfinite(count) && count > 0.0 ? static_cast<uint64_t>(count) : 0;
    setting.time_map = mapped(m_controls.time_map);
    setting.repeat = mapped(m_controls.repeat);
    return setting;
}

bool Course::speed(double value)
{
    if (!std::isfinite(value)) {
        return false;
    }
    CourseControls incoming;
    incoming.speed = ControlValue(value);
    set(incoming);
    return true;
}

bool Course::offset(double seconds)
{
    if (!std::isfinite(seconds)) {
        return false;
    }
    CourseControls incoming;
    incoming.offset = ControlValue(seconds);
    set(incoming);
    return true;
}

bool Course::range(double in, double out)
{
    if (!std::isfinite(in) || !std::isfinite(out) || !(out > in)) {
        return false;
    }
    CourseControls incoming;
    incoming.range = ControlSnapshot<CourseRange>(CourseRange { .in = in, .out = out });
    set(incoming);
    return true;
}

void Course::edge(CourseEdge edge, uint64_t count)
{
    CourseControls incoming;
    incoming.edge = ControlValue(edge);
    incoming.loop_count = ControlValue(static_cast<double>(count));
    set(incoming);
}

void Course::time_map(Kinesis::TimeMap map)
{
    CourseControls incoming;
    incoming.time_map = ControlSnapshot<Kinesis::TimeMap>(std::move(map));
    set(incoming);
}

void Course::clear_time_map()
{
    CourseControls incoming;
    incoming.time_map = ControlSnapshot<Kinesis::TimeMap>(Kinesis::TimeMap {});
    set(incoming);
}

void Course::repeat(Kinesis::TimeMap length)
{
    CourseControls incoming;
    incoming.repeat = ControlSnapshot<Kinesis::TimeMap>(std::move(length));
    set(incoming);
}

void Course::clear_repeat()
{
    CourseControls incoming;
    incoming.repeat = ControlSnapshot<Kinesis::TimeMap>(Kinesis::TimeMap {});
    set(incoming);
}

bool Course::weight(double value)
{
    if (!std::isfinite(value) || value < 0.0) {
        return false;
    }
    CourseControls incoming;
    incoming.weight = ControlValue(value);
    set(incoming);
    return true;
}

double Course::weight() const noexcept
{
    const double value = number(m_controls.weight, 1.0);
    return std::isfinite(value) && value > 0.0 ? value : 0.0;
}

CourseSetting Course::setting() const
{
    return compose();
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
    const CourseSetting setting = compose();

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
    return m_clock.resolve(compose(), extra_offset);
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
