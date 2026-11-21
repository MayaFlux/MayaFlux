#pragma once

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kriya/ReflowHelpers/ReflowSelect.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

template <typename Frame>
Reflow<Frame>::Reflow(std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink,
    std::vector<FrameOperation<Frame>> operations, FrameSelection selection, Rewrite rewrite, double rate)
    : m_source(std::move(source))
    , m_sink(std::move(sink))
    , m_selection(selection)
    , m_rewrite(rewrite)
    , m_rate(rate)
    , m_courses(std::vector<std::shared_ptr<Course>> {})
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
    auto course = Course::create(m_rate, setting, paused);
    if (!course) {
        return nullptr;
    }
    m_courses.update([&course](std::vector<std::shared_ptr<Course>>& all) { all.push_back(course); });
    return course;
}

template <typename Frame>
bool Reflow<Frame>::remove_course(const std::shared_ptr<Course>& course)
{
    bool removed = false;
    m_courses.update([&](std::vector<std::shared_ptr<Course>>& all) {
        const auto it = std::ranges::find(all, course);
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
    const auto pinned = m_courses.read();
    return pinned ? *pinned : std::vector<std::shared_ptr<Course>> {};
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
bool Reflow<Frame>::operate(const FrameContext& context, std::shared_ptr<const Frame>& frame)
{
    for (const auto& operation : m_operations) {
        switch (operation.kind()) {
        case FrameOperation<Frame>::Kind::Transform:
            frame = operation.transform_fn()(context, std::move(frame));
            if (!frame) {
                return false;
            }
            break;
        case FrameOperation<Frame>::Kind::Derive:
            operation.derive_fn()(context, *frame);
            break;
        case FrameOperation<Frame>::Kind::Condition:
            if (!operation.condition_fn()(context, *frame)) {
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
void Reflow<Frame>::advance()
{
    const auto pinned = m_courses.read();
    std::span<const std::shared_ptr<Course>> courses;
    if (pinned) {
        courses = std::span<const std::shared_ptr<Course>>(*pinned);
    }

    const FrameContext context { .courses = courses, .cycle = m_cycle };
    m_cycle_cell.store(++m_cycle, std::memory_order_relaxed);

    try {
        m_gathered.clear();
        for (const auto& course : courses) {
            const auto step = course->advance(course->advanced());
            if (!step) {
                continue;
            }

            const auto location = m_source->locate(step->time);
            if (!location) {
                course->report_miss(0);
                continue;
            }

            const auto picks = select_frames(*location, m_selection, course->weight());
            bool read_any = false;
            for (size_t i = 0; i < picks.count; ++i) {
                const auto& pick = picks.items[i];
                auto frame = m_source->read(pick.index);
                if (!frame) {
                    course->report_miss(pick.index);
                    continue;
                }
                m_gathered.push_back({ .frame = std::move(frame), .weight = pick.weight, .index = pick.index });
                read_any = true;
            }
            if (read_any) {
                course->report_frame(nearest_frame(*location));
            }
        }

        if (m_gathered.empty()) {
            hold();
            return;
        }
        m_holding = false;

        std::shared_ptr<const Frame> frame;
        if (m_gathered.size() == 1) {
            frame = m_gathered.front().frame;
        } else if (m_mix) {
            frame = (*m_mix)(context, std::span<const WeightedFrame<Frame>>(m_gathered));
        } else {
            const auto heaviest = std::ranges::max_element(m_gathered, {}, &WeightedFrame<Frame>::weight);
            frame = heaviest->frame;
        }

        if (!frame || !operate(context, frame)) {
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
    std::shared_ptr<const Kakshya::FrameSource<Frame>> source, Sink sink, double rate)
    : m_source(std::move(source))
    , m_sink(std::move(sink))
    , m_rate(rate > 0.0 ? rate : static_cast<double>(Vruta::s_registered_frame_rate))
{
}

template <typename Frame>
ReflowBuilder<Frame>& ReflowBuilder<Frame>::course(const CourseSetting& setting, bool paused)
{
    m_courses.push_back({ .setting = setting, .paused = paused });
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

    if (!m_source || !m_sink) {
        return refuse("a source and a sink are required");
    }
    if (m_source->count() == 0) {
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

    auto planned = m_courses;
    if (planned.empty()) {
        const auto& times = m_source->time_sampling();
        const auto first = times.time(0);
        const auto last = times.time(m_source->count() - 1);
        if (!first || !last) {
            return refuse("the source cannot time its first and last frame");
        }

        CourseSetting whole;
        whole.in = *first;
        whole.out = *last > *first ? *last : *first + 1.0;
        whole.edge = m_source->count() == 1 ? CourseEdge::Hold : CourseEdge::Once;
        planned.push_back({ .setting = whole, .paused = false });
    }

    std::shared_ptr<Reflow<Frame>> reflow(
        new Reflow<Frame>(m_source, m_sink, m_operations, m_selection, m_rewrite, m_rate));
    for (const auto& plan : planned) {
        if (!reflow->add_course(plan.setting, plan.paused)) {
            return refuse("a course setting is unusable or the rate is not above zero");
        }
    }
    return reflow;
}

template <typename Frame>
std::shared_ptr<Reflow<Frame>> ReflowBuilder<Frame>::start(const std::shared_ptr<Vruta::TaskScheduler>& scheduler)
{
    if (!scheduler) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ReflowBuilder: start needs a scheduler");
        return nullptr;
    }

    auto reflow = build();
    if (!reflow) {
        return nullptr;
    }

    const std::weak_ptr<Reflow<Frame>> weak = reflow;
    auto routine = metro(
        1.0 / reflow->rate(),
        [weak] {
            if (const auto running = weak.lock()) {
                running->advance();
            }
        },
        Vruta::ProcessingToken::FRAME_ACCURATE);

    scheduler->add_task(routine);
    reflow->m_scheduler = scheduler;
    reflow->m_routine = std::move(routine);
    return reflow;
}

} // namespace MayaFlux::Kriya
