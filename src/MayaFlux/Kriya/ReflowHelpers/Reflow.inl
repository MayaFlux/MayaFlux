#pragma once

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kriya/ReflowHelpers/ReflowSelect.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

template <typename Frame>
Reflow<Frame>::Reflow(std::shared_ptr<const Kakshya::FrameSource<Frame>> source,
    std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled, Sink sink,
    std::vector<FrameOperation<Frame>> operations, FrameSelection selection, Rewrite rewrite, double cycle_rate)
    : m_source(std::move(source))
    , m_sampled(std::move(sampled))
    , m_sink(std::move(sink))
    , m_selection(selection)
    , m_rewrite(rewrite)
    , m_cycle_rate(cycle_rate)
    , m_lanes(std::vector<std::shared_ptr<Lane>> {})
    , m_current(std::shared_ptr<const Frame> {})
{
    for (auto& operation : operations) {
        if (operation.kind() == FrameOperation<Frame>::Kind::Mix) {
            m_mix = operation.mix_fn();
        } else {
            m_operations.push_back(std::move(operation));
        }
    }
}

template <typename Frame>
Reflow<Frame>::~Reflow()
{
    stop();
}

template <typename Frame>
std::shared_ptr<Course> Reflow<Frame>::add_course(const CourseSetting& setting, bool paused)
{
    return add_route(CourseRoute<Frame> { .course = setting, .paused = paused });
}

template <typename Frame>
std::shared_ptr<Course> Reflow<Frame>::add_route(const CourseRoute<Frame>& route)
{
    if (route.every == 0 || !std::isfinite(route.enters_after) || route.enters_after < 0.0) {
        return nullptr;
    }
    if (route.source && route.sampled) {
        return nullptr;
    }
    for (const auto& operation : route.operations) {
        if (!operation.valid() || operation.kind() == FrameOperation<Frame>::Kind::Mix) {
            return nullptr;
        }
    }

    auto lane = std::make_shared<Lane>();
    if (route.sampled) {
        lane->sampled = route.sampled;
    } else if (route.source) {
        lane->source = route.source;
    } else {
        lane->sampled = m_sampled;
        lane->source = m_source;
    }
    if (!lane->source && !lane->sampled) {
        return nullptr;
    }
    if (lane->source && lane->source->count() == 0) {
        return nullptr;
    }

    lane->sink = route.sink ? route.sink : Sink {};
    if (!lane->sink && !m_sink) {
        return nullptr;
    }

    auto course = Course::create(m_cycle_rate, route.course, route.paused);
    if (!course) {
        return nullptr;
    }

    lane->course = course;
    lane->operations = route.operations;
    lane->every = route.every;
    lane->enter_cycle = m_cycle_cell.load(std::memory_order_relaxed)
        + static_cast<uint64_t>(std::ceil(route.enters_after * m_cycle_rate));

    m_lanes.update([&lane](std::vector<std::shared_ptr<Lane>>& all) { all.push_back(lane); });
    return course;
}

template <typename Frame>
bool Reflow<Frame>::remove_course(const std::shared_ptr<Course>& course)
{
    bool removed = false;
    m_lanes.update([&](std::vector<std::shared_ptr<Lane>>& all) {
        const auto it = std::ranges::find_if(all, [&course](const auto& lane) { return lane->course == course; });
        removed = it != all.end();
        if (removed) {
            all.erase(it);
        }
        return removed;
    });
    return removed;
}

template <typename Frame>
std::vector<std::shared_ptr<Course>> Reflow<Frame>::courses() const
{
    std::vector<std::shared_ptr<Course>> result;
    if (const auto pinned = m_lanes.read()) {
        result.reserve(pinned->size());
        for (const auto& lane : *pinned) {
            result.push_back(lane->course);
        }
    }
    return result;
}

template <typename Frame>
std::shared_ptr<const Frame> Reflow<Frame>::current() const
{
    const auto pinned = m_current.read();
    return pinned ? *pinned : nullptr;
}

template <typename Frame>
void Reflow<Frame>::hold()
{
    m_holds.fetch_add(1, std::memory_order_relaxed);
    if (!m_holding) {
        m_holding = true;
        MF_RT_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Reflow: no frame could be read; keeping the last frame written until one can");
    }
}

template <typename Frame>
bool Reflow<Frame>::operate(const std::vector<FrameOperation<Frame>>& operations,
    const FrameContext& context, std::shared_ptr<const Frame>& frame)
{
    for (const auto& operation : operations) {
        switch (operation.kind()) {
        case FrameOperation<Frame>::Kind::Transform:
            frame = operation.transform_fn()(context, std::move(frame));
            if (!frame) {
                return false;
            }
            break;
        case FrameOperation<Frame>::Kind::Derive:
            operation.derive_fn()(context, frame);
            break;
        case FrameOperation<Frame>::Kind::Condition:
            if (!operation.condition_fn()(context, frame)) {
                return false;
            }
            break;
        case FrameOperation<Frame>::Kind::Mix:
            break;
        }
    }
    return true;
}

template <typename Frame>
bool Reflow<Frame>::read_lane(Lane& lane, double time, std::vector<WeightedFrame<Frame>>& into)
{
    const double weight = lane.course->weight();

    if (lane.sampled) {
        if (!(weight > 0.0)) {
            return false;
        }
        auto frame = lane.sampled->sample(time);
        if (!frame) {
            lane.course->report_miss(0);
            return false;
        }
        into.push_back({ .frame = std::move(frame), .weight = weight, .index = 0 });
        return true;
    }

    const auto location = lane.source->locate(time);
    if (!location) {
        lane.course->report_miss(0);
        return false;
    }

    const auto picks = Detail::select_frames(*location, m_selection, weight);
    bool read_any = false;
    for (size_t i = 0; i < picks.count; ++i) {
        const auto& pick = picks.items[i];
        auto frame = lane.source->read(pick.index);
        if (!frame) {
            lane.course->report_miss(pick.index);
            continue;
        }
        into.push_back({ .frame = std::move(frame), .weight = pick.weight, .index = pick.index });
        read_any = true;
    }
    if (read_any) {
        lane.course->report_frame(Detail::nearest_frame(*location));
    }
    return read_any;
}

template <typename Frame>
std::shared_ptr<const Frame> Reflow<Frame>::reduce(
    const FrameContext& context, const std::vector<WeightedFrame<Frame>>& frames) const
{
    if (frames.size() == 1) {
        return frames.front().frame;
    }
    if (m_mix) {
        return (*m_mix)(context, std::span<const WeightedFrame<Frame>>(frames));
    }
    return std::ranges::max_element(frames, {}, &WeightedFrame<Frame>::weight)->frame;
}

template <typename Frame>
void Reflow<Frame>::advance()
{
    const auto pinned = m_lanes.read();
    std::span<const std::shared_ptr<Lane>> lanes;
    if (pinned) {
        lanes = std::span<const std::shared_ptr<Lane>>(*pinned);
    }

    m_course_scratch.clear();
    for (const auto& lane : lanes) {
        m_course_scratch.push_back(lane->course);
    }

    const uint64_t cycle = m_cycle;
    const FrameContext context { .courses = m_course_scratch, .cycle = cycle };
    m_cycle_cell.store(++m_cycle, std::memory_order_relaxed);

    try {
        m_gathered.clear();
        size_t shared_due = 0;

        for (const auto& lane : lanes) {
            if (cycle < lane->enter_cycle) {
                continue;
            }

            const auto step = lane->course->advance(lane->course->advanced());
            const bool due = lane->every <= 1 || (cycle - lane->enter_cycle) % lane->every == 0;
            if (!due) {
                continue;
            }

            const bool own = static_cast<bool>(lane->sink);
            if (!own) {
                ++shared_due;
            }
            if (!step) {
                continue;
            }

            const bool reduced = own || !lane->operations.empty();
            m_lane_frames.clear();
            if (!read_lane(*lane, step->time, reduced ? m_lane_frames : m_gathered) || !reduced) {
                continue;
            }

            FrameContext lane_context = context;
            lane_context.course = lane->course.get();
            auto frame = reduce(lane_context, m_lane_frames);
            if (!frame || !operate(lane->operations, lane_context, frame)) {
                m_skips.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            if (!own) {
                m_gathered.push_back({ .frame = std::move(frame), .weight = lane->course->weight(), .index = 0 });
                continue;
            }

            if (m_rewrite == Rewrite::OnChange && frame == lane->last_written) {
                m_skips.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            lane->sink(frame, lane_context);
            lane->last_written = std::move(frame);
            m_writes.fetch_add(1, std::memory_order_relaxed);
        }

        if (m_gathered.empty()) {
            if (shared_due > 0) {
                hold();
            }
            return;
        }
        m_holding = false;

        auto frame = reduce(context, m_gathered);
        if (!frame || !operate(m_operations, context, frame)) {
            m_skips.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (m_rewrite == Rewrite::OnChange && frame == m_last_written) {
            m_skips.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        m_sink(frame, context);
        m_last_written = frame;
        m_current.publish(frame);
        m_writes.fetch_add(1, std::memory_order_relaxed);
    } catch (const std::exception& error) {
        m_failures.fetch_add(1, std::memory_order_relaxed);
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Reflow: a source, operation or sink threw: {}", error.what());
    } catch (...) {
        m_failures.fetch_add(1, std::memory_order_relaxed);
        MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "Reflow: a source, operation or sink threw");
    }
}

template <typename Frame>
void Reflow<Frame>::stop()
{
    if (!m_routine) {
        return;
    }
    m_routine->set_should_terminate(true);
    if (m_scheduler) {
        m_scheduler->cancel_task(m_routine);
    }
    m_routine.reset();
}

template <typename Frame>
ReflowBuilder<Frame>::ReflowBuilder(
    std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink, double cycle_rate,
    std::shared_ptr<Vruta::TaskScheduler> scheduler)
    : m_source(std::move(source))
    , m_sink(std::move(sink))
    , m_cycle_rate(cycle_rate > 0.0 ? cycle_rate : static_cast<double>(Vruta::s_registered_frame_rate))
    , m_scheduler(std::move(scheduler))
{
}

template <typename Frame>
ReflowBuilder<Frame> ReflowBuilder<Frame>::sampling(
    std::shared_ptr<const Kakshya::SampledFrames<Frame>> sampled, Sink sink, double cycle_rate,
    std::shared_ptr<Vruta::TaskScheduler> scheduler)
{
    ReflowBuilder builder(nullptr, std::move(sink), cycle_rate, std::move(scheduler));
    builder.m_sampled = std::move(sampled);
    return builder;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::course(const CourseSetting& setting, bool paused)
{
    m_courses.push_back({ .setting = setting, .paused = paused });
    return *this;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::route(CourseRoute<Frame> route)
{
    m_routes.push_back(std::move(route));
    return *this;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::configure(const ReflowConfig<Frame>& config)
{
    for (const auto& setting : config.courses) {
        course(setting, config.paused);
    }
    for (const auto& route : config.routes) {
        m_routes.push_back(route);
    }
    for (const auto& operation : config.operations) {
        m_operations.push_back(operation);
    }
    m_selection = config.selection;
    m_rewrite = config.rewrite;
    return *this;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::operation(FrameOperation<Frame> operation)
{
    m_operations.push_back(std::move(operation));
    return *this;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::selection(FrameSelection selection)
{
    m_selection = selection;
    return *this;
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::rewrite(Rewrite rewrite)
{
    m_rewrite = rewrite;
    return *this;
}

template <typename Frame>
std::shared_ptr<Reflow<Frame>> ReflowBuilder<Frame>::build()
{
    const auto refuse = [](const char* reason) -> std::shared_ptr<Reflow<Frame>> {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration, "ReflowBuilder: {}", reason);
        return nullptr;
    };

    const bool shared_source = m_source || m_sampled;
    if (m_source && m_source->count() == 0) {
        return refuse("the source has no frames");
    }

    size_t mixes = 0;
    for (const auto& operation : m_operations) {
        if (!operation.valid()) {
            return refuse("an operation has no callable");
        }
        if (operation.kind() == FrameOperation<Frame>::Kind::Mix) {
            ++mixes;
        }
    }
    if (mixes > 1) {
        return refuse("at most one mix operation is allowed");
    }

    bool needs_shared_source = !m_courses.empty();
    bool needs_shared_sink = !m_courses.empty();
    for (const auto& route : m_routes) {
        needs_shared_source = needs_shared_source || (!route.source && !route.sampled);
        needs_shared_sink = needs_shared_sink || !route.sink;
    }

    auto planned = m_courses;
    if (planned.empty() && m_routes.empty()) {
        needs_shared_source = true;
        needs_shared_sink = true;

        double first = 0.0;
        double last = 0.0;
        bool single = false;
        if (m_source) {
            const auto& times = m_source->time_sampling();
            const auto from = times.time(0);
            const auto to = times.time(m_source->count() - 1);
            if (!from || !to) {
                return refuse("the source cannot time its first and last frame");
            }
            first = *from;
            last = *to;
            single = m_source->count() == 1;
        } else if (m_sampled) {
            const auto extent = m_sampled->extent();
            first = extent.first;
            last = extent.second;
        }

        CourseSetting whole;
        whole.in = first;
        whole.out = last > first ? last : first + 1.0;
        whole.edge = single || !(last > first) ? CourseEdge::Hold : CourseEdge::Once;
        planned.push_back({ .setting = whole, .paused = false });
    }

    if (needs_shared_source && !shared_source) {
        return refuse("a source and a sink are required");
    }
    if (needs_shared_sink && !m_sink) {
        return refuse("a source and a sink are required");
    }

    std::shared_ptr<Reflow<Frame>> reflow(
        new Reflow<Frame>(m_source, m_sampled, m_sink, m_operations, m_selection, m_rewrite, m_cycle_rate));
    for (const auto& plan : planned) {
        if (!reflow->add_course(plan.setting, plan.paused)) {
            return refuse("a course setting is unusable or the cycle rate is not above zero");
        }
    }
    for (const auto& route : m_routes) {
        if (!reflow->add_route(route)) {
            return refuse("a route is unusable: its course, its source, its operations, its entry or its cadence");
        }
    }
    return reflow;
}

template <typename Frame>
std::shared_ptr<Reflow<Frame>> ReflowBuilder<Frame>::start()
{
    if (!m_scheduler) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ReflowBuilder: start needs a scheduler, given to the builder");
        return nullptr;
    }

    auto reflow = build();
    if (!reflow) {
        return nullptr;
    }

    const std::weak_ptr<Reflow<Frame>> weak = reflow;
    auto routine = metro(
        1.0 / reflow->cycle_rate(),
        [weak] {
            if (const auto running = weak.lock()) {
                running->advance();
            }
        },
        Vruta::ProcessingToken::FRAME_ACCURATE);

    m_scheduler->add_task(routine);
    reflow->m_scheduler = m_scheduler;
    reflow->m_routine = std::move(routine);
    return reflow;
}

} // namespace MayaFlux::Kriya
