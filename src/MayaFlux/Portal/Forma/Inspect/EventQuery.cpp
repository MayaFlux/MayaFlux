#include "Inspector.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"

namespace MayaFlux::Portal::Forma {

InspectResult Inspector::event(
    const std::shared_ptr<Vruta::Event>& ev,
    std::string_view name,
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::string header_label = name.empty() ? "(unnamed)" : std::string(name);

    std::vector<ValueSpec> values {
        ValueSpec {
            .label = "token",
            .reader = [ev] {
                return std::string(Reflect::enum_to_string(ev->get_processing_token()));
            },
        },
        ValueSpec {
            .label = "active",
            .reader = [ev] { return ev->is_active() ? "true" : "false"; },
        },
    };

    const auto dims = row_pixel_dims(window, x_min, x_max, row_h);
    auto hbuf = make_row_buffer(window, header_label, dims);
    std::vector<RowBuffer> rbufs;
    rbufs.reserve(values.size());
    for (const auto& spec : values)
        rbufs.push_back(make_row_buffer(window, spec.label, dims));

    auto group = make_value_group(values, std::move(hbuf), rbufs,
        layer, context, cursor, x_min, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);
    return result;
}

InspectResult Inspector::event_manager(
    Layer& layer, Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::vector<ValueSpec> root_values {
        ValueSpec {
            .label = "events",
            .reader = [&m_event_mgr = m_event_mgr] {
                return std::to_string(m_event_mgr.get_all_events().size());
            },
        },
    };

    const auto dims = row_pixel_dims(window, x_min, x_max, row_h);
    auto hbuf = make_row_buffer(window, "EventManager", dims);
    std::vector<RowBuffer> rbufs;
    rbufs.reserve(root_values.size());
    for (const auto& spec : root_values)
        rbufs.push_back(make_row_buffer(window, spec.label, dims));

    auto root_group = make_value_group(root_values, std::move(hbuf), rbufs,
        layer, context, cursor, x_min, x_max, row_h, true);

    InspectResult result;
    result.group = std::move(root_group);

    const auto names = m_event_mgr.get_event_names();

    for (const auto& ev : m_event_mgr.get_all_events()) {
        if (!ev)
            continue;

        std::string header_label = "(unnamed)";
        for (const auto& n : names) {
            if (m_event_mgr.get_event(n) == ev) {
                header_label = n;
                break;
            }
        }

        std::vector<ValueSpec> values {
            ValueSpec {
                .label = "active",
                .reader = [ev] { return ev->is_active() ? "true" : "false"; },
            },
        };

        const auto ev_dims = row_pixel_dims(window, x_min, x_max, row_h);
        auto ev_hbuf = make_row_buffer(window, header_label, ev_dims);
        std::vector<RowBuffer> ev_rbufs;
        ev_rbufs.reserve(values.size());
        for (const auto& spec : values)
            ev_rbufs.push_back(make_row_buffer(window, spec.label, ev_dims));

        auto ev_group = make_value_group(values, std::move(ev_hbuf), ev_rbufs,
            layer, context, cursor, x_min, x_max, row_h, false);

        InspectResult ev_result;
        ev_result.group = std::move(ev_group);

        result.group.header.attach(layer, ev_result.group.header.header_id);
        result.children.push_back(std::move(ev_result));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma
