#include "Inspector.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    InspectResult inspect_task(
        const Vruta::TaskEntry& entry,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h)
    {
        auto routine = entry.routine;
        const std::string name = entry.name;

        std::vector<ValueSpec> values {
            ValueSpec {
                .label = "name",
                .reader = [name] { return name.empty() ? "(unnamed)" : name; },
            },
            ValueSpec {
                .label = "token",
                .reader = [routine] {
                    return std::string(Reflect::enum_to_string(routine->get_processing_token()));
                },
            },
            ValueSpec {
                .label = "active",
                .reader = [routine] { return routine->is_active() ? "true" : "false"; },
            },
            ValueSpec {
                .label = "delay",
                .reader = [routine] {
                    return std::string(Reflect::enum_to_string(routine->get_delay_context()));
                },
            },
        };

        auto group = make_value_group(
            Reflect::short_dynamic_type_name(*entry.routine), values,
            layer, context, window, cursor,
            x_min, x_max, row_h, false);

        InspectResult result;
        result.group = std::move(group);
        return result;
    }

} // namespace

InspectResult Inspector::scheduler(
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::vector<ValueSpec> root_values {
        ValueSpec {
            .label = "tasks",
            .reader = [&m_sched = m_sched] {
                return std::to_string(m_sched.get_all_tasks().size());
            },
        },
    };

    auto root_group = make_value_group(
        "TaskScheduler", root_values,
        layer, context, window, cursor,
        x_min, x_max, row_h, true);

    InspectResult result;
    result.group = std::move(root_group);

    const auto tasks = m_sched.get_all_tasks();
    for (const auto& entry : tasks) {
        if (!entry.routine)
            continue;

        auto task_result = inspect_task(
            entry, layer, context, window, cursor,
            x_min, x_max, row_h);

        result.group.header.attach(layer, task_result.group.header.header_id);
        result.children.push_back(std::move(task_result));
    }

    return result;
}

InspectResult Inspector::tasks(
    Vruta::ProcessingToken token,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::string header_label = "TaskScheduler ["
        + std::string(Reflect::enum_to_string(token)) + "]";

    auto root_group = make_value_group(
        header_label, {},
        layer, context, window, cursor,
        x_min, x_max, row_h, true);

    InspectResult result;
    result.group = std::move(root_group);

    const auto all = m_sched.get_all_tasks();
    for (const auto& entry : all) {
        if (!entry.routine)
            continue;
        if (entry.routine->get_processing_token() != token)
            continue;

        auto task_result = inspect_task(
            entry, layer, context, window, cursor,
            x_min, x_max, row_h);

        result.group.header.attach(layer, task_result.group.header.header_id);
        result.children.push_back(std::move(task_result));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma
